#include "core.h"
#include <algorithm>
#include <cwctype>
#include <vector>
#include <windows.h>
namespace wfa {
namespace {
constexpr DWORD limit = 65536;
struct Handle {
  HKEY key = nullptr;
  ~Handle() {
    if (key != nullptr)
      RegCloseKey(key);
  }
  Handle() = default;
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
};
void fail(Key &out, LSTATUS error) {
  out.error = static_cast<unsigned long>(error);
  out.status =
      (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
          ? L"missing"
          : (error == ERROR_ACCESS_DENIED ? L"inaccessible" : L"error");
}
void value(Key &out, const std::wstring &name, DWORD type,
           const std::vector<wchar_t> &data, DWORD bytes) {
  if (type != REG_SZ && type != REG_EXPAND_SZ) {
    out.values.push_back({name, L"[non-string metadata omitted]", type});
    return;
  }
  if (bytes % sizeof(wchar_t) != 0 || bytes > limit) {
    out.status = L"partial";
    return;
  }
  auto n = static_cast<size_t>(bytes) / sizeof(wchar_t);
  if (n > 0 && data[n - 1] == L'\0')
    --n;
  else
    out.status = L"partial";
  std::wstring metadata(data.data(), n);
  if (metadata.find(L'\0') != std::wstring::npos)
    out.status = L"partial";
  for (size_t i = 0; i < metadata.size(); ++i) {
    const auto c = static_cast<unsigned int>(metadata[i]);
    if (c >= 0xd800U && c <= 0xdbffU) {
      if (i + 1 >= metadata.size() || metadata[i + 1] < 0xdc00U ||
          metadata[i + 1] > 0xdfffU)
        out.status = L"partial";
      else
        ++i;
    } else if (c >= 0xdc00U && c <= 0xdfffU)
      out.status = L"partial";
  }
  out.values.push_back({name, std::move(metadata), type});
}
} // namespace
// Windows SDK 10.0.26100 winreg.h gives RegOpenKeyExW an invalid value-type
// SAL annotation. Reproduced without application code in
// scripts/sdk-fixture.cpp. CMake disables only C6553 in this translation unit
// for /analyze builds; all application diagnostics remain errors.
Key WindowsSource::read(const std::wstring &layer, const std::wstring &path) {
  Key out;
  HKEY root = nullptr;
  std::wstring full;
  if (layer == L"user" || layer == L"machine") {
    root = layer == L"user" ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    full = L"Software\\Classes\\" + path;
  } else if (layer == L"merged") {
    root = HKEY_CLASSES_ROOT;
    full = path;
  } else if (layer == L"user_raw" || layer == L"machine_raw") {
    root = layer == L"user_raw" ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    full = path;
  } else {
    fail(out, ERROR_INVALID_PARAMETER);
    return out;
  }
  Handle h;
  const auto result = RegOpenKeyExW(
      root, full.c_str(), 0,
      KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_64KEY, &h.key);
  if (result != ERROR_SUCCESS) {
    fail(out, result);
    return out;
  }
  out.status = L"ok";
  std::vector<wchar_t> data(limit / sizeof(wchar_t) + 1, L'\0');
  std::vector<wchar_t> name(16384, L'\0');
  // UserChoice hash is deliberately never queried or enumerated.
  if (path.size() >= 11 &&
      CompareStringOrdinal(path.data() + path.size() - 11, 11, L"\\UserChoice",
                           11, TRUE) == CSTR_EQUAL) {
    DWORD bytes = limit, type = 0;
    const auto e =
        RegQueryValueExW(h.key, L"ProgId", nullptr, &type,
                         reinterpret_cast<BYTE *>(data.data()), &bytes);
    if (e == ERROR_SUCCESS)
      value(out, L"ProgId", type, data, bytes);
    else if (e != ERROR_FILE_NOT_FOUND) {
      fail(out, e);
      out.status = L"partial";
    }
    return out;
  }
  size_t retainedBytes = 0;
  DWORD i = 0;
  for (; i < 256; ++i) {
    DWORD chars = static_cast<DWORD>(name.size()), bytes = limit, type = 0;
    const auto e = RegEnumValueW(h.key, i, name.data(), &chars, nullptr, &type,
                                 reinterpret_cast<BYTE *>(data.data()), &bytes);
    if (e == ERROR_NO_MORE_ITEMS)
      break;
    if (e != ERROR_SUCCESS) {
      fail(out, e);
      out.status = L"partial";
      break;
    }
    retainedBytes += bytes + static_cast<size_t>(chars) * sizeof(wchar_t);
    if (retainedBytes > size_t{256} * 1024U) {
      out.status = L"partial";
      out.error = ERROR_MORE_DATA;
      break;
    }
    value(out, std::wstring(name.data(), chars), type, data, bytes);
  }
  if (i == 256) {
    out.status = L"partial";
    out.error = ERROR_MORE_DATA;
  }
  for (i = 0; i < 512; ++i) {
    DWORD chars = static_cast<DWORD>(name.size());
    const auto e = RegEnumKeyExW(h.key, i, name.data(), &chars, nullptr,
                                 nullptr, nullptr, nullptr);
    if (e == ERROR_NO_MORE_ITEMS)
      break;
    if (e != ERROR_SUCCESS) {
      fail(out, e);
      out.status = L"partial";
      break;
    }
    out.children.emplace_back(name.data(), chars);
  }
  if (i == 512) {
    out.status = L"partial";
    out.error = ERROR_MORE_DATA;
  }
  return out;
}
std::wstring WindowsSource::presence(const std::wstring &path) {
  if (path.size() < 4 || !std::iswalpha(path[0]) || path[1] != L':' ||
      path[2] != L'\\' ||
      path.find_first_of(L"/%\"<>|?*", 3) != std::wstring::npos ||
      path.find(L':', 2) != std::wstring::npos ||
      path.find(L'\0') != std::wstring::npos)
    return L"unsafe_path_not_checked";
  const auto drive = path.substr(0, 3);
  if (GetDriveTypeW(drive.c_str()) != DRIVE_FIXED)
    return L"non_fixed_drive_not_checked";
  size_t pos = 3;
  while (pos < path.size()) {
    const auto next = path.find(L'\\', pos);
    const auto part = path.substr(
        pos, next == std::wstring::npos ? std::wstring::npos : next - pos);
    if (part.empty() || part == L"." || part == L".." || part.back() == L'.' ||
        part.back() == L' ')
      return L"unsafe_path_not_checked";
    const auto prefix = path.substr(0, next);
    const auto attr = GetFileAttributesW(prefix.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
      const auto e = GetLastError();
      return (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND)
                 ? L"missing"
                 : L"inaccessible_or_unknown";
    }
    if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
      return L"reparse_point_not_checked";
    if (next == std::wstring::npos)
      return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0
                 ? L"present"
                 : L"directory_not_executable";
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
      return L"missing";
    pos = next + 1;
  }
  return L"unsafe_path_not_checked";
}
} // namespace wfa
