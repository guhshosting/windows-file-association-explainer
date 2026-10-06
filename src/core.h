#pragma once
#include <map>
#include <string>
#include <vector>
namespace wfa {
struct Value {
  std::wstring name;
  std::wstring text;
  unsigned long type = 1;
};
struct Key {
  std::wstring status = L"missing";
  unsigned long error = 0;
  std::vector<Value> values;
  std::vector<std::wstring> children;
};
struct Source {
  virtual ~Source() = default;
  virtual Key read(const std::wstring &layer, const std::wstring &path) = 0;
  virtual std::wstring presence(const std::wstring &path) = 0;
};
struct Command {
  std::wstring target;
  std::wstring status;
  std::wstring presence;
  std::vector<std::wstring> tokens;
};
struct Record {
  std::wstring layer;
  std::wstring path;
  Key key;
  std::vector<Command> commands;
};
struct Report {
  std::wstring query;
  std::wstring extension;
  std::vector<Record> records;
  std::vector<std::wstring> warnings;
  bool partial = false;
};
std::wstring normalize(const std::wstring &query);
Command parse_command(const std::wstring &command, Source &source);
Report inspect(const std::wstring &query, Source &source);
std::string utf8(const std::wstring &value);
std::string json_string(const std::wstring &value);
std::string json(const Report &report, bool verbs, bool registrations);
std::string text(const Report &report, bool verbs, bool registrations);
class WindowsSource final : public Source {
public:
  Key read(const std::wstring &layer, const std::wstring &path) override;
  std::wstring presence(const std::wstring &path) override;
};
} // namespace wfa
