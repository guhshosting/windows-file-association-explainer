#include "core.h"
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <windows.h>
namespace {
void check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  try {
    wfa::WindowsSource s;
    check(s.presence(L"\\\\unreachable.invalid\\share\\x.exe") ==
              L"unsafe_path_not_checked",
          "UNC blocked");
    check(s.presence(L"\\\\?\\C:\\Windows\\x.exe") ==
              L"unsafe_path_not_checked",
          "device blocked");
    check(s.presence(L"C:\\Windows\\x.exe:stream") ==
              L"unsafe_path_not_checked",
          "ADS blocked");
    check(s.presence(L"relative.exe") == L"unsafe_path_not_checked",
          "relative blocked");
    wchar_t system[MAX_PATH]{};
    const auto n = GetSystemDirectoryW(system, MAX_PATH);
    check(n > 0 && n < MAX_PATH, "system directory");
    const std::wstring base(system, n);
    check(s.presence(base + L"\\cmd.exe") == L"present",
          "local executable attributes");
    check(s.presence(base + L"\\wfa_missing_8ab29.exe") == L"missing",
          "missing local executable");
    check(s.presence(base) == L"directory_not_executable",
          "directory is not file");
    check(s.read(L"invalid", L".txt").status == L"error", "invalid layer");
    check(s.read(L"merged", L".wfa_nonexistent_8ab29").status == L"missing",
          "missing registry key");
    auto k = s.read(L"machine_raw",
                    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
    check(k.status == L"ok", "local registry acquisition");
    std::cout << "Read-only Windows acquisition checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  } catch (...) {
    return 1; // Convert unexpected fixture failures into a failing test exit
              // code.
  }
}
