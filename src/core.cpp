#include "core.h"
#include <algorithm>
#include <cwctype>
#include <iterator>
#include <numeric>
#include <set>
#include <stdexcept>
#include <windows.h>
namespace wfa {
namespace {
bool component(const std::wstring &s) {
  return !s.empty() && s.size() <= 255 &&
         s.find_first_of(L"\\/:\r\n\t") == std::wstring::npos &&
         s.find(L'\0') == std::wstring::npos && s != L"." && s != L"..";
}
std::wstring lower(std::wstring s) {
  std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) {
    return static_cast<wchar_t>(std::towlower(c));
  });
  return s;
}
std::wstring get(const Key &k, const std::wstring &n) {
  const auto name = lower(n);
  const auto found =
      std::find_if(k.values.begin(), k.values.end(), [&](const Value &v) {
        return lower(v.name) == name &&
               (v.type == REG_SZ || v.type == REG_EXPAND_SZ);
      });
  return found == k.values.end() ? std::wstring{} : found->text;
}
bool selected(const Record &r, bool verbs, bool registrations) {
  return registrations || !verbs ||
         r.path.find(L"\\shell") != std::wstring::npos;
}
} // namespace
std::wstring normalize(const std::wstring &query) {
  if (query.empty() || query.size() > 32767 ||
      query.find(L'\0') != std::wstring::npos)
    throw std::invalid_argument("Invalid or oversized query");
  const auto slash = query.find_last_of(L"\\/");
  const auto name = query.substr(slash == std::wstring::npos ? 0 : slash + 1);
  const auto dot = name.find_last_of(L'.');
  if (dot == std::wstring::npos || dot + 1 == name.size())
    throw std::invalid_argument(
        "Supply an extension or a filename with an extension");
  auto ext = name.substr(dot);
  if (!component(ext) || ext.find_first_of(L" *?\"<>|") != std::wstring::npos)
    throw std::invalid_argument("Invalid extension");
  return lower(ext);
}
Command parse_command(const std::wstring &command, Source &source) {
  Command out;
  out.presence = L"not_checked";
  for (size_t i = 0; i + 1 < command.size(); ++i)
    if (command[i] == L'%' &&
        (command[i + 1] == L'*' || command[i + 1] == L'~' ||
         (command[i + 1] >= L'0' && command[i + 1] <= L'9') ||
         command[i + 1] == L'L' || command[i + 1] == L'l' ||
         command[i + 1] == L'V' || command[i + 1] == L'v')) {
      const auto token = command.substr(i, 2);
      if (std::find(out.tokens.begin(), out.tokens.end(), token) ==
          out.tokens.end())
        out.tokens.push_back(token);
    }
  const auto start = command.find_first_not_of(L" \t");
  if (start == std::wstring::npos) {
    out.status = L"empty";
    return out;
  }
  size_t end = 0;
  if (command[start] == L'"') {
    end = command.find(L'"', start + 1);
    if (end == std::wstring::npos ||
        (end + 1 < command.size() && command[end + 1] != L' ' &&
         command[end + 1] != L'\t')) {
      out.status = L"malformed_quotes";
      return out;
    }
    out.target = command.substr(start + 1, end - start - 1);
  } else {
    end = command.find_first_of(L" \t", start);
    out.target = command.substr(
        start, end == std::wstring::npos ? std::wstring::npos : end - start);
  }
  if (out.target.find_first_of(L"\"%\r\n") != std::wstring::npos ||
      out.target.find(L'\0') != std::wstring::npos || out.target.size() < 4 ||
      lower(out.target.substr(out.target.size() - 4)) != L".exe") {
    out.status = L"ambiguous_or_non_executable";
    return out;
  }
  if (out.target.size() < 3 || !std::iswalpha(out.target[0]) ||
      out.target[1] != L':' || out.target[2] != L'\\') {
    out.status = L"relative_or_search_dependent";
    return out;
  }
  out.status = L"literal_absolute_executable";
  out.presence = source.presence(out.target);
  return out;
}
Report inspect(const std::wstring &query, Source &source) {
  Report out;
  out.query = query;
  out.extension = normalize(query);
  std::map<std::pair<std::wstring, std::wstring>, size_t> seen;
  size_t acquiredChars = 0;
  bool budgetReached = false;
  std::set<std::wstring> progids, apps, perceived;
  const auto add = [&](const std::wstring &layer,
                       const std::wstring &path) -> Key {
    const auto previous = seen.find({layer, path});
    if (previous != seen.end())
      return out.records[previous->second].key;
    if (budgetReached || out.records.size() >= 4096) {
      out.partial = true;
      return {L"limit", 234, {}, {}};
    }
    Key k = source.read(layer, path);
    auto keyChars =
        std::accumulate(k.values.begin(), k.values.end(), path.size(),
                        [](size_t sum, const Value &v) {
                          return sum + v.name.size() + v.text.size();
                        });
    keyChars = std::accumulate(k.children.begin(), k.children.end(), keyChars,
                               [](size_t sum, const std::wstring &child) {
                                 return sum + child.size();
                               });
    if (keyChars > size_t{4} * 1024U * 1024U - acquiredChars) {
      out.partial = true;
      budgetReached = true;
      out.warnings.push_back(L"Aggregate metadata size limit reached.");
      return {L"limit", 234, {}, {}};
    }
    acquiredChars += keyChars;
    std::sort(k.values.begin(), k.values.end(),
              [](const Value &a, const Value &b) { return a.name < b.name; });
    std::sort(k.children.begin(), k.children.end());
    if (k.status != L"ok" && k.status != L"missing")
      out.partial = true;
    Record r{layer, path, k, {}};
    if (path.size() >= 8 && path.substr(path.size() - 8) == L"\\command")
      for (const auto &v : k.values)
        if (v.name.empty() && (v.type == REG_SZ || v.type == REG_EXPAND_SZ)) {
          auto c = parse_command(v.text, source);
          if (c.presence == L"missing") {
            std::wstring message = L"Missing executable in ";
            message += layer;
            message += L'\\';
            message += path;
            out.warnings.push_back(std::move(message));
          }
          r.commands.push_back(std::move(c));
        }
    seen.emplace(std::make_pair(layer, path), out.records.size());
    out.records.push_back(std::move(r));
    return k;
  };
  const auto collect = [&](const Key &k, std::set<std::wstring> &dest,
                           bool names) {
    if (names) {
      for (const auto &v : k.values)
        if (component(v.name))
          dest.insert(v.name);
    } else {
      auto p = get(k, L"");
      if (component(p))
        dest.insert(p);
    }
  };
  const std::vector<std::wstring> layers{L"user", L"machine", L"merged"};
  std::wstring userProg, machineProg;
  for (const auto &layer : layers) {
    const auto k = add(layer, out.extension);
    collect(k, progids, false);
    if (layer == L"user")
      userProg = get(k, L"");
    if (layer == L"machine")
      machineProg = get(k, L"");
    auto p = get(k, L"PerceivedType");
    if (component(p))
      perceived.insert(p);
    collect(add(layer, out.extension + L"\\OpenWithProgids"), progids, true);
    auto a = add(layer, out.extension + L"\\OpenWithList");
    for (const auto &child : a.children)
      if (component(child))
        apps.insert(child);
  }
  if (!userProg.empty() && !machineProg.empty() && userProg != machineProg)
    out.warnings.push_back(
        L"User and machine extension ProgIDs differ; the merged view is "
        L"evidence, not a guarantee of application behavior.");
  const auto base =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" +
      out.extension;
  const auto uc = add(L"user_raw", base + L"\\UserChoice");
  auto p = get(uc, L"ProgId");
  if (component(p))
    progids.insert(p);
  collect(add(L"user_raw", base + L"\\OpenWithProgids"), progids, true);
  const auto ow = add(L"user_raw", base + L"\\OpenWithList");
  for (const auto &v : ow.values)
    if (v.name != L"MRUList" && component(v.text))
      apps.insert(v.text);
  // Only query capability paths from fixed registry roots; never follow
  // arbitrary paths.
  for (const auto &layer :
       std::vector<std::wstring>{L"user_raw", L"machine_raw"}) {
    const auto registered = add(layer, L"Software\\RegisteredApplications");
    for (const auto &v : registered.values)
      if ((v.type == REG_SZ || v.type == REG_EXPAND_SZ) &&
          v.text.size() < 1024 && lower(v.text).rfind(L"software\\", 0) == 0 &&
          v.text.find(L"..") == std::wstring::npos &&
          v.text.find_first_of(L"/\r\n%") == std::wstring::npos &&
          v.text.find(L'\0') == std::wstring::npos &&
          lower(v.text).find(L"\\userchoice") == std::wstring::npos) {
        const auto k = add(layer, v.text + L"\\FileAssociations");
        auto id = get(k, out.extension);
        if (component(id)) {
          progids.insert(id);
          add(layer, v.text);
        }
      }
  }
  std::set<std::wstring> types = progids;
  types.insert(out.extension);
  types.insert(L"SystemFileAssociations\\" + out.extension);
  types.insert(L"*");
  std::transform(perceived.begin(), perceived.end(),
                 std::inserter(types, types.end()),
                 [](const std::wstring &type) {
                   return L"SystemFileAssociations\\" + type;
                 });
  std::transform(
      apps.begin(), apps.end(), std::inserter(types, types.end()),
      [](const std::wstring &app) { return L"Applications\\" + app; });
  // Bound all fanout, including hostile fixtures and unusually large
  // registrations.
  if (types.size() > 256) {
    out.partial = true;
    out.warnings.push_back(L"Type inspection limit reached.");
  }
  size_t count = 0;
  for (const auto &type : types) {
    if (count++ >= 256)
      break;
    for (const auto &layer : layers) {
      const auto k = add(layer, type);
      if (k.status == L"missing" && progids.count(type) != 0 &&
          layer == L"merged")
        out.warnings.push_back(
            L"Referenced ProgID is missing from merged view: " + type);
      add(layer, type + L"\\DefaultIcon");
      add(layer, type + L"\\Application");
      const auto cv = add(layer, type + L"\\CurVer");
      const auto current = get(cv, L"");
      if (component(current) && current != type) {
        add(layer, current);
        out.warnings.push_back(L"CurVer reference recorded without recursively "
                               L"resolving version chains: " +
                               current);
      }
      const auto shell = add(layer, type + L"\\shell");
      auto def = get(shell, L"");
      if (!def.empty()) {
        std::wstring message = L"Registered default verb for ";
        message += layer;
        message += L'\\';
        message += type;
        message += L": ";
        message += def;
        out.warnings.push_back(std::move(message));
      }
      size_t verbs = 0;
      for (const auto &verb : shell.children) {
        if (verbs++ >= 128) {
          out.partial = true;
          break;
        }
        if (component(verb)) {
          auto vp = type;
          vp += L"\\shell\\";
          vp += verb;
          add(layer, vp);
          add(layer, vp + L"\\command");
          add(layer, vp + L"\\ddeexec");
        }
      }
    }
  }
  out.warnings.push_back(L"Effective default unavailable: UserChoice ProgId is "
                         L"unvalidated metadata; its hash is never read or "
                         L"interpreted. No handler is executed.");
  out.warnings.push_back(L"HKCR is a Windows merged view of user and machine "
                         L"Classes. Registrations and default/open verbs do "
                         L"not guarantee what every application will use.");
  return out;
}
std::string utf8(const std::wstring &value) {
  if (value.empty())
    return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                    static_cast<int>(value.size()), nullptr, 0,
                                    nullptr, nullptr);
  if (n <= 0)
    throw std::runtime_error("UTF-8 conversion failed");
  std::string out(static_cast<size_t>(n), '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, value.data(),
                          static_cast<int>(value.size()), out.data(), n,
                          nullptr, nullptr) != n)
    throw std::runtime_error("UTF-8 conversion failed");
  return out;
}
std::string json_string(const std::wstring &value) {
  const auto s = utf8(value);
  std::string out = "\"";
  const char hex[] = "0123456789abcdef";
  for (const unsigned char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c < 32 || c == 127) {
      out += "\\u00";
      out += hex[static_cast<unsigned int>(c) >> 4U];
      out += hex[static_cast<unsigned int>(c) & 15U];
    } else
      out += static_cast<char>(c);
  }
  return out + '"';
}
std::string json(const Report &report, bool verbs, bool registrations) {
  std::string s = "{\"schema_version\":1,\"version\":\"1.0.0\",\"query\":" +
                  json_string(report.query) +
                  ",\"extension\":" + json_string(report.extension) +
                  ",\"partial\":" + (report.partial ? "true" : "false") +
                  ",\"effective_default\":{\"status\":\"unavailable\","
                  "\"reason\":\"Protected selection is not validated by this "
                  "metadata inspector\"},\"records\":[";
  bool first = true;
  for (const auto &r : report.records) {
    if (!selected(r, verbs, registrations))
      continue;
    if (!first)
      s += ',';
    first = false;
    s += "{\"layer\":" + json_string(r.layer) +
         ",\"path\":" + json_string(r.path) +
         ",\"status\":" + json_string(r.key.status) +
         ",\"error\":" + std::to_string(r.key.error) + ",\"values\":[";
    bool fv = true;
    for (const auto &v : r.key.values) {
      if (!fv)
        s += ',';
      fv = false;
      s += "{\"name\":" + json_string(v.name) +
           ",\"type\":" + std::to_string(v.type) +
           ",\"text\":" + json_string(v.text) + '}';
    }
    s += "],\"subkeys\":[";
    fv = true;
    for (const auto &child : r.key.children) {
      if (!fv)
        s += ',';
      fv = false;
      s += json_string(child);
    }
    s += "],\"commands\":[";
    fv = true;
    for (const auto &c : r.commands) {
      if (!fv)
        s += ',';
      fv = false;
      s += "{\"target\":" + json_string(c.target) +
           ",\"parse_status\":" + json_string(c.status) +
           ",\"presence\":" + json_string(c.presence) + ",\"placeholders\":[";
      bool ft = true;
      for (const auto &t : c.tokens) {
        if (!ft)
          s += ',';
        ft = false;
        s += json_string(t);
      }
      s += "]}";
    }
    s += "]}";
  }
  s += "],\"warnings\":[";
  first = true;
  for (const auto &w : report.warnings) {
    if (!first)
      s += ',';
    first = false;
    s += json_string(w);
  }
  return s + "]}\n";
}
std::string text(const Report &report, bool verbs, bool registrations) {
  std::string s = "windows-file-association-explainer 1.0.0\nExtension: " +
                  json_string(report.extension) +
                  "\nEffective default: unavailable\n";
  for (const auto &r : report.records) {
    if (!selected(r, verbs, registrations) ||
        (!registrations && r.key.status == L"missing"))
      continue;
    s += "\n[" + utf8(r.layer) + "] " + json_string(r.path) + " (" +
         utf8(r.key.status) + ", error " + std::to_string(r.key.error) + ")\n";
    for (const auto &v : r.key.values)
      s += "  " + json_string(v.name) + " = " + json_string(v.text) +
           " [type " + std::to_string(v.type) + "]\n";
    for (const auto &c : r.commands) {
      s += "  Implied target: " + json_string(c.target) + "; " +
           utf8(c.status) + "; " + utf8(c.presence) + "\n  Placeholders:";
      for (const auto &t : c.tokens)
        s += ' ' + utf8(t);
      s += '\n';
    }
  }
  s += "\nAcquisition: " +
       std::string(report.partial ? "partial"
                                  : "completed within documented scope") +
       "\n";
  for (const auto &w : report.warnings)
    s += "- " + json_string(w) + '\n';
  return s;
}
} // namespace wfa
