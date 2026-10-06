#include "core.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <stdexcept>
namespace {
struct Fixture final : wfa::Source {
  std::map<std::pair<std::wstring, std::wstring>, wfa::Key> keys;
  std::map<std::wstring, std::wstring> files;
  int probes = 0;
  wfa::Key read(const std::wstring &l, const std::wstring &p) override {
    const auto i = keys.find({l, p});
    return i == keys.end() ? wfa::Key{} : i->second;
  }
  std::wstring presence(const std::wstring &p) override {
    ++probes;
    const auto i = files.find(p);
    return i == files.end() ? L"missing" : i->second;
  }
  void put(const std::wstring &l, const std::wstring &p,
           std::vector<wfa::Value> v = {}, std::vector<std::wstring> c = {}) {
    keys[{l, p}] = {L"ok", 0, std::move(v), std::move(c)};
  }
};
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
} // namespace
int main(int argc, char **argv) {
  try {
    Fixture f;
    f.put(L"user", L".txt",
          {{L"", L"User.Text"},
           {L"PerceivedType", L"text"},
           {L"Content Type", L"text/plain"}});
    f.put(L"machine", L".txt", {{L"", L"Machine.Text"}});
    f.put(L"merged", L".txt", {{L"", L"User.Text"}});
    f.put(L"merged", L"User.Text");
    f.put(L"merged", L".txt\\OpenWithProgids",
          {{L"Another.Text", L"", 0}, {L"Missing.Text", L"", 0}});
    f.put(L"user_raw",
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\."
          L"txt\\OpenWithList",
          {{L"a", L"editor.exe"}, {L"b", L"viewer.exe"}, {L"MRUList", L"ba"}});
    f.put(L"merged", L"User.Text\\shell", {{L"", L"open"}},
          {L"open", L"edit", L"print"});
    f.put(
        L"merged", L"User.Text\\shell\\open\\command",
        {{L"", L"\"C:\\Program Files\\Editor\\editor.exe\" \"%1\" %* %L %V"}});
    f.files[L"C:\\Program Files\\Editor\\editor.exe"] = L"present";
    f.put(L"merged", L"User.Text\\shell\\edit\\command",
          {{L"", L"C:\\missing.exe %1"}});
    f.keys[{L"machine", L"User.Text\\shell"}] = {L"inaccessible", 5, {}, {}};
    auto r = wfa::inspect(L"example.txt", f);
    check(r.partial, "partial/inaccessible");
    check(r.extension == L".txt", "filename extension");
    auto j = wfa::json(r, false, true);
    check(j == wfa::json(wfa::inspect(L"example.txt", f), false, true),
          "deterministic JSON");
    check(j.find("Missing executable") != std::string::npos,
          "missing executable");
    check(j.find("Missing.Text") != std::string::npos, "missing ProgID");
    check(j.find("viewer.exe") != std::string::npos, "OpenWithList");
    check(j.find("Another.Text") != std::string::npos, "OpenWithProgids");
    check(j.find("ProgIDs differ") != std::string::npos,
          "user machine conflict");
    check(j.find("print") != std::string::npos, "multiple verbs");
    auto c = wfa::parse_command(
        L"\"C:\\Program Files\\Editor\\editor.exe\" \"%1\" %* %L", f);
    check(c.presence == L"present" && c.tokens.size() == 3,
          "quoted placeholders");
    const auto probes = f.probes;
    check(wfa::parse_command(L"\"C:\\bad.exe", f).status == L"malformed_quotes",
          "malformed quote");
    check(wfa::parse_command(L"C:\\Program Files\\editor.exe %1", f).status ==
              L"ambiguous_or_non_executable",
          "unquoted path ambiguity");
    check(wfa::parse_command(L"editor.exe %1", f).status ==
              L"relative_or_search_dependent",
          "relative path");
    check(wfa::parse_command(L"%SystemRoot%\\editor.exe %1", f).presence ==
              L"not_checked",
          "no environment expansion");
    check(wfa::parse_command(L"\\\\server\\share\\editor.exe %1", f).presence ==
              L"not_checked",
          "UNC no probe");
    check(f.probes == probes, "unsafe commands never probed");
    check(wfa::normalize(L"C:\\missing\\résumé.文書") == L".文書",
          "Unicode extension");
    f.put(L"merged", L".文書",
          {{L"", L"文字.Type"}, {L"description", L"résumé \"quoted\"\n\t"}});
    auto u = wfa::json(wfa::inspect(L".文書", f), false, true);
    check(u.find(wfa::utf8(L"文字.Type")) != std::string::npos &&
              u.find("\\u000a\\u0009") != std::string::npos,
          "Unicode metadata and escapes");
    check(wfa::json_string(L"\x1b") == "\"\\u001b\"",
          "terminal control escape");
    bool invalid = false;
    try {
      wfa::normalize(L"no-extension");
    } catch (const std::invalid_argument &) {
      invalid = true;
    }
    check(invalid, "invalid query");
    Fixture missing;
    auto m = wfa::inspect(L".unknown", missing);
    check(!m.partial, "missing is not partial");
    Fixture partial;
    partial.keys[{L"merged", L".txt"}] = {
        L"partial", 234, {{L"", L"Partial.Text"}}, {}};
    check(wfa::inspect(L".txt", partial).partial,
          "partial acquisition retained");
    if (argc == 2 && std::string(argv[1]) == "--json-fixture")
      std::cout << u;
    else
      std::cout << "All fixture checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  } catch (...) {
    return 1; // Convert unexpected fixture failures into a failing test exit
              // code.
  }
}
