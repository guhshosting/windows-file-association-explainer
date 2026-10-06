#include "core.h"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <windows.h>
namespace {
void emit(const std::string &s, bool error = false) {
  const auto h = GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (h != INVALID_HANDLE_VALUE && h != nullptr &&
      GetConsoleMode(h, &mode) != 0) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                      static_cast<int>(s.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        wide.data(), n);
    size_t offset = 0;
    while (offset < wide.size()) {
      DWORD written = 0;
      const auto chunk =
          static_cast<DWORD>((std::min)(wide.size() - offset, size_t{8192}));
      if (WriteConsoleW(h, wide.data() + offset, chunk, &written, nullptr) ==
              0 ||
          written == 0)
        throw std::runtime_error("Console output failed");
      offset += written;
    }
  } else {
    auto *f = error ? stderr : stdout;
    if (std::fwrite(s.data(), 1, s.size(), f) != s.size() ||
        std::fflush(f) != 0)
      throw std::runtime_error("Output failed");
  }
}
const char *help =
    "windows-file-association-explainer 1.0.0\nUsage: "
    "windows-file-association-explainer <extension-or-file> [--verbs] "
    "[--registrations] [--json]\n       windows-file-association-explainer "
    "--help | --version\n\nRead-only x64 Windows association evidence; "
    "filenames are parsed lexically and never opened.\n--verbs          Show "
    "verb registration records only\n--registrations  Include all queried "
    "records, including missing keys; overrides --verbs\n--json           Emit "
    "UTF-8 JSON schema version 1\nExit codes: 0 complete acquisition, 1 "
    "failure, 2 invalid CLI, 3 partial acquisition.\nA missing registration is "
    "evidence, not a failure. Effective default is unavailable.\nMetadata does "
    "not guarantee any application's behavior. No commands are executed.\n";
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    if (argc == 2 && std::wstring(argv[1]) == L"--help") {
      emit(help);
      return 0;
    }
    if (argc == 2 && std::wstring(argv[1]) == L"--version") {
      emit("windows-file-association-explainer 1.0.0\n");
      return 0;
    }
    std::wstring query;
    bool verbs = false, registrations = false, asJson = false;
    for (int i = 1; i < argc; ++i) {
      const std::wstring a = argv[i];
      if (a == L"--verbs")
        verbs = true;
      else if (a == L"--registrations")
        registrations = true;
      else if (a == L"--json")
        asJson = true;
      else if (a.empty() || a[0] == L'-' || !query.empty()) {
        emit("Invalid arguments. Use --help.\n", true);
        return 2;
      } else
        query = a;
    }
    if (query.empty()) {
      emit("Supply an extension or filename. Use --help.\n", true);
      return 2;
    }
    wfa::WindowsSource source;
    const auto report = wfa::inspect(query, source);
    emit(asJson ? wfa::json(report, verbs, registrations)
                : wfa::text(report, verbs, registrations));
    return report.partial ? 3 : 0;
  } catch (const std::invalid_argument &e) {
    return std::fprintf(stderr, "%s\n", e.what()) < 0 ? 1 : 2;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "Failure: %s\n", e.what());
    return 1;
  } catch (...) {
    std::fputs("Unexpected failure\n", stderr);
    return 1;
  }
}
