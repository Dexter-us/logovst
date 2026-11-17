#include "logo_music_interpreter.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace logo_midi {
namespace {

constexpr std::size_t kMaximumScriptBytes = 64 * 1024;
constexpr std::size_t kMaximumTokens = 32768;
constexpr std::size_t kMaximumCommands = 4096;
constexpr std::size_t kMaximumLoopIterations = 4096;
constexpr std::size_t kMaximumProcedures = 256;
constexpr std::size_t kMaximumNestingDepth = 64;
constexpr std::size_t kMaximumListLength = 4096;

struct Value;
bool equal_values(const Value& left, const Value& right);

struct Value {
  using List = std::vector<Value>;
  std::variant<double, bool, std::string, List> data;

  Value() : data(0.0) {}
  explicit Value(double value) : data(value) {}
  explicit Value(bool value) : data(value) {}
  explicit Value(std::string value) : data(std::move(value)) {}
  explicit Value(List value) : data(std::move(value)) {}
  friend bool operator==(const Value& left, const Value& right) {
    return equal_values(left, right);
  }
};

struct Token {
  std::string text;
  std::size_t line = 0;
  bool newline = false;
};

struct Procedure {
  std::vector<std::string> parameters;
  std::vector<Token> body;
};

struct Program {
  std::vector<Token> main;
  std::unordered_map<std::string, Procedure> procedures;
};

std::string uppercase(std::string value) {
  for (char& character : value) {
    character = static_cast<char>(
        std::toupper(static_cast<unsigned char>(character)));
  }
  return value;
}

[[noreturn]] void fail(std::size_t line, const std::string& message) {
  throw std::runtime_error("Line " + std::to_string(line) + ": " + message);
}

bool is_word_character(char character) {
  const auto value = static_cast<unsigned char>(character);
  return std::isalnum(value) || value == '_' || value == '?' || value == '!' ||
         value == '-' || value == '#' || value == '.';
}

bool is_operator_character(char character) {
  return character == '+' || character == '-' || character == '*' ||
         character == '/' || character == '<' || character == '>' ||
         character == '=' || character == '!';
}

std::vector<Token> tokenize(const std::string& script) {
  std::vector<Token> tokens;
  std::istringstream input(script);
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    std::size_t position = 0;
    while (position < line.size()) {
      const auto character = static_cast<unsigned char>(line[position]);
      if (std::isspace(character)) {
        ++position;
        continue;
      }
      if (line[position] == ';') {
        break;
      }
      if (line[position] == '[' || line[position] == ']' ||
          line[position] == '(' || line[position] == ')') {
        tokens.push_back({std::string(1, line[position++]), line_number, false});
        continue;
      }
      if (line[position] == '"') {
        const auto start = position++;
        while (position < line.size() &&
               !std::isspace(static_cast<unsigned char>(line[position])) &&
               line[position] != '[' && line[position] != ']' &&
               line[position] != '(' && line[position] != ')') {
          ++position;
        }
        tokens.push_back(
            {line.substr(start, position - start), line_number, false});
        continue;
      }
      if (line[position] == ':' ) {
        const auto start = position++;
        while (position < line.size() &&
               is_word_character(line[position])) {
          ++position;
        }
        tokens.push_back(
            {line.substr(start, position - start), line_number, false});
        continue;
      }
      if (std::isdigit(character) ||
          (line[position] == '.' && position + 1 < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[position + 1])))) {
        char* end = nullptr;
        std::strtod(line.c_str() + position, &end);
        if (end != line.c_str() + position) {
          const auto end_position =
              static_cast<std::size_t>(end - line.c_str());
          tokens.push_back(
              {line.substr(position, end_position - position), line_number,
               false});
          position = end_position;
          continue;
        }
      }
      if (is_operator_character(line[position])) {
        const auto start = position++;
        if (position < line.size() &&
            ((line[start] == '<' &&
              (line[position] == '=' || line[position] == '>')) ||
             (line[start] == '>' && line[position] == '=') ||
             (line[start] == '!' && line[position] == '='))) {
          ++position;
        }
        tokens.push_back(
            {line.substr(start, position - start), line_number, false});
        continue;
      }

      const auto start = position;
      while (position < line.size() &&
             !std::isspace(static_cast<unsigned char>(line[position])) &&
             line[position] != '[' && line[position] != ']' &&
             line[position] != '(' && line[position] != ')' &&
             !is_operator_character(line[position]) && line[position] != ';') {
        ++position;
      }
      if (position == start) {
        fail(line_number, "cannot tokenize character `" +
                              std::string(1, line[position]) + "`.");
      }
      tokens.push_back({line.substr(start, position - start), line_number,
                        false});
      if (tokens.size() > kMaximumTokens) {
        fail(line_number, "script exceeds the 32768-token limit.");
      }
    }
    tokens.push_back({"", line_number, true});
    if (tokens.size() > kMaximumTokens) {
      fail(line_number, "script exceeds the 32768-token limit.");
    }
  }
  return tokens;
}

std::string quoted_word(const std::string& token, std::size_t line) {
  if (token.empty() || token.front() != '"') {
    fail(line, "expected a quoted Logo word.");
  }
  auto value = token.substr(1);
  if (!value.empty() && value.back() == '"') {
    value.pop_back();
  }
  if (value.empty()) {
    fail(line, "quoted word cannot be empty.");
  }
  return value;
}

bool is_valid_name(const std::string& name) {
  if (name.empty() ||
      !std::isalpha(static_cast<unsigned char>(name.front()))) {
    return false;
  }
  return std::all_of(name.begin() + 1, name.end(), [](char character) {
    const auto value = static_cast<unsigned char>(character);
    return std::isalnum(value) || value == '_' || value == '?' ||
           value == '!';
  });
}

bool is_builtin_name(const std::string& name) {
  static const std::vector<std::string> names = {
      "TEMPO", "WAIT", "VELOCITY", "CHANNEL", "GATE", "REST", "PROGRAM",
      "NOTE", "CHORD", "CC", "STEAL", "ANTICIPATE", "SYNC", "MAKE",
      "LOCAL", "LOCALMAKE", "RERANDOM", "REPEAT", "REPCOUNT", "IF",
      "IFELSE", "WHILE", "UNTIL", "FOREVER", "OUTPUT", "OP", "STOP",
      "PI", "TRUE", "FALSE", "RANDOM", "THING", "SUM", "DIFFERENCE",
      "PRODUCT", "QUOTIENT", "REMAINDER", "MODULO", "POWER", "ABS", "INT",
      "ROUND", "SQRT", "SIN", "COS", "TAN", "EXP", "LN", "LOG10",
      "ARCSIN", "ARCCOS", "ARCTAN", "RADARCSIN", "RADARCCOS",
      "RADARCTAN", "MIN", "MAX", "AND", "OR", "NOT", "EQUAL?",
      "NOTEQUAL?", "LESS?", "GREATER?", "WORD", "LIST", "SENTENCE", "SE",
      "FIRST", "LAST", "ITEM", "COUNT", "EMPTY?", "MEMBER?", "FPUT", "LPUT",
      "BUTFIRST", "BUTLAST", "PICK", "REVERSE", "WORD?", "LIST?", "NUMBER?",
      "BOOLEAN?"};
  return std::find(names.begin(), names.end(), name) != names.end();
}

Program split_procedures(const std::vector<Token>& tokens) {
  Program program;
  std::size_t position = 0;
  std::size_t main_bracket_depth = 0;
  while (position < tokens.size()) {
    while (position < tokens.size() && tokens[position].newline) {
      program.main.push_back(tokens[position++]);
    }
    if (position >= tokens.size()) {
      break;
    }
    const auto line_start = position;
    const auto line_number = tokens[position].line;
    const auto first = uppercase(tokens[position].text);
    std::size_t line_end = position;
    while (line_end < tokens.size() && !tokens[line_end].newline) {
      ++line_end;
    }
    if (first == "END") {
      fail(line_number, "END has no matching TO.");
    }
    if (first != "TO") {
      for (std::size_t index = line_start; index < line_end; ++index) {
        if (tokens[index].text == "[") ++main_bracket_depth;
        if (tokens[index].text == "]" && main_bracket_depth > 0) {
          --main_bracket_depth;
        }
        program.main.push_back(tokens[index]);
      }
      if (line_end < tokens.size()) {
        program.main.push_back(tokens[line_end]);
        position = line_end + 1;
      } else {
        position = line_end;
      }
      continue;
    }
    if (main_bracket_depth != 0) {
      fail(line_number, "TO procedure definitions must be outside blocks.");
    }
    if (line_end - line_start < 2) {
      fail(line_number, "TO requires a procedure name.");
    }
    const auto name = uppercase(tokens[line_start + 1].text);
    if (!is_valid_name(name) || is_builtin_name(name)) {
      fail(line_number, "invalid or reserved procedure name `" + name + "`.");
    }
    if (program.procedures.size() >= kMaximumProcedures) {
      fail(line_number, "script exceeds the 256 procedure limit.");
    }
    Procedure procedure;
    for (std::size_t index = line_start + 2; index < line_end; ++index) {
      const auto& parameter = tokens[index].text;
      if (parameter.size() < 2 || parameter.front() != ':' ||
          !is_valid_name(parameter.substr(1))) {
        fail(line_number, "procedure parameters must be names such as `:PITCH`.");
      }
      const auto normalized = uppercase(parameter.substr(1));
      if (std::find(procedure.parameters.begin(), procedure.parameters.end(),
                    normalized) != procedure.parameters.end()) {
        fail(line_number, "duplicate procedure parameter `:" + normalized +
                              "`.");
      }
      procedure.parameters.push_back(normalized);
    }
    if (procedure.parameters.size() > 64) {
      fail(line_number, "a procedure may have at most 64 parameters.");
    }
    position = line_end < tokens.size() ? line_end + 1 : line_end;
    bool found_end = false;
    while (position < tokens.size()) {
      while (position < tokens.size() && tokens[position].newline) {
        procedure.body.push_back(tokens[position++]);
      }
      if (position >= tokens.size()) break;
      const auto body_line_start = position;
      auto body_line_end = position;
      while (body_line_end < tokens.size() && !tokens[body_line_end].newline) {
        ++body_line_end;
      }
      if (uppercase(tokens[body_line_start].text) == "TO") {
        fail(tokens[body_line_start].line,
             "procedure definitions cannot be nested.");
      }
      if (uppercase(tokens[body_line_start].text) == "END") {
        if (body_line_end - body_line_start != 1) {
          fail(tokens[body_line_start].line, "END takes no arguments.");
        }
        position = body_line_end < tokens.size() ? body_line_end + 1
                                                : body_line_end;
        found_end = true;
        break;
      }
      for (std::size_t index = body_line_start; index < body_line_end; ++index) {
        procedure.body.push_back(tokens[index]);
      }
      if (body_line_end < tokens.size()) {
        procedure.body.push_back(tokens[body_line_end]);
        position = body_line_end + 1;
      } else {
        position = body_line_end;
      }
    }
    if (!found_end) {
      fail(line_number, "procedure `" + name + "` is missing END.");
    }
    if (!program.procedures.emplace(name, std::move(procedure)).second) {
      fail(line_number, "procedure `" + name + "` is already defined.");
    }
  }
  return program;
}

bool equal_values(const Value& left, const Value& right) {
  if (left.data.index() != right.data.index()) return false;
  if (const auto* number = std::get_if<double>(&left.data)) {
    return *number == std::get<double>(right.data);
  }
  if (const auto* boolean = std::get_if<bool>(&left.data)) {
    return *boolean == std::get<bool>(right.data);
  }
  if (const auto* word = std::get_if<std::string>(&left.data)) {
    return *word == std::get<std::string>(right.data);
  }
  return std::get<Value::List>(left.data) == std::get<Value::List>(right.data);
}

std::string value_string(const Value& value) {
  if (const auto* number = std::get_if<double>(&value.data)) {
    std::ostringstream output;
    output << std::setprecision(15) << *number;
    return output.str();
  }
  if (const auto* boolean = std::get_if<bool>(&value.data)) {
    return *boolean ? "TRUE" : "FALSE";
  }
  if (const auto* word = std::get_if<std::string>(&value.data)) {
    return *word;
  }
  const auto& list = std::get<Value::List>(value.data);
  std::string output = "[";
  for (std::size_t index = 0; index < list.size(); ++index) {
    if (index != 0) output += " ";
    if (std::holds_alternative<std::string>(list[index].data)) {
      output += "\"" + value_string(list[index]);
    } else {
      output += value_string(list[index]);
    }
  }
  output += "]";
  return output;
}

int builtin_arity(const std::string& name) {
  if (name == "PI" || name == "TRUE" || name == "FALSE" ||
      name == "REPCOUNT") return 0;
  if (name == "RANDOM" || name == "THING" || name == "ABS" ||
      name == "INT" || name == "ROUND" || name == "SQRT" ||
      name == "SIN" || name == "COS" || name == "TAN" || name == "EXP" ||
      name == "LN" || name == "LOG10" || name == "ARCSIN" ||
      name == "ARCCOS" || name == "ARCTAN" || name == "RADARCSIN" ||
      name == "RADARCCOS" || name == "RADARCTAN" || name == "NOT" ||
      name == "FIRST" || name == "LAST" || name == "COUNT" ||
      name == "EMPTY?" || name == "BUTFIRST" || name == "BUTLAST" ||
      name == "PICK" || name == "REVERSE" || name == "WORD?" ||
      name == "LIST?" || name == "NUMBER?" || name == "BOOLEAN?") return 1;
  if (name == "SUM" || name == "DIFFERENCE" || name == "PRODUCT" ||
      name == "QUOTIENT" || name == "REMAINDER" || name == "MODULO" ||
      name == "POWER" || name == "MIN" || name == "MAX" || name == "AND" ||
      name == "OR" || name == "EQUAL?" || name == "NOTEQUAL?" ||
      name == "LESS?" || name == "GREATER?" || name == "WORD" ||
      name == "LIST" || name == "SENTENCE" || name == "SE" ||
      name == "ITEM" || name == "MEMBER?" || name == "FPUT" ||
      name == "LPUT") return 2;
  return -1;
}

struct OutputSignal {
  Value value;
};
struct StopSignal {};

class MusicLogoInterpreter {
 public:
  MusicLogoInterpreter(const std::string& script, double initial_tempo)
      : engine_(), random_(std::random_device{}()) {
    if (script.size() > kMaximumScriptBytes) {
      throw std::runtime_error("Music script exceeds the 64 KiB limit.");
    }
    engine_.set_tempo(initial_tempo);
    program_ = split_procedures(tokenize(script));
  }

  MusicEngine run() {
    std::size_t position = 0;
    try {
      execute_sequence(program_.main, position, 0, false);
    } catch (const StopSignal&) {
      // STOP at top level stops execution after the current command.
    } catch (const OutputSignal&) {
      throw std::runtime_error("OUTPUT can only be used inside a procedure.");
    }
    if (engine_.beat_position() <= 0.0) {
      throw std::runtime_error(
          "Music script must advance time with NOTE, CHORD, REST, or WAIT.");
    }
    engine_.sort_events();
    return std::move(engine_);
  }

 private:
  using Frame = std::unordered_map<std::string, Value>;
  using Environment = std::vector<Frame>;

  const Token& peek(const std::vector<Token>& tokens, std::size_t position) const {
    static const Token end_token{"", 1, true};
    return position < tokens.size() ? tokens[position] : end_token;
  }

  std::size_t line_at(const std::vector<Token>& tokens,
                      std::size_t position) const {
    return peek(tokens, position).line;
  }

  void skip_newlines(const std::vector<Token>& tokens,
                     std::size_t& position) const {
    while (position < tokens.size() && tokens[position].newline) ++position;
  }

  double as_number(const Value& value, std::size_t line,
                   const std::string& purpose) const {
    if (const auto* number = std::get_if<double>(&value.data)) {
      if (std::isfinite(*number)) return *number;
    }
    if (const auto* boolean = std::get_if<bool>(&value.data)) {
      return *boolean ? 1.0 : 0.0;
    }
    fail(line, purpose + " requires a number, got `" + value_string(value) +
                   "`.");
  }

  bool as_boolean(const Value& value, std::size_t line,
                  const std::string& purpose) const {
    if (const auto* boolean = std::get_if<bool>(&value.data)) return *boolean;
    if (const auto* number = std::get_if<double>(&value.data)) {
      return *number != 0.0;
    }
    fail(line, purpose + " requires TRUE or FALSE.");
  }

  std::string as_word(const Value& value, std::size_t line,
                      const std::string& purpose) const {
    if (const auto* word = std::get_if<std::string>(&value.data)) return *word;
    fail(line, purpose + " requires a word.");
  }

  const Value& variable(const std::string& name, std::size_t line) const {
    for (auto frame = frames_.rbegin(); frame != frames_.rend(); ++frame) {
      const auto local = frame->find(name);
      if (local != frame->end()) return local->second;
    }
    const auto global = globals_.find(name);
    if (global == globals_.end()) {
      fail(line, "unknown variable `:" + name + "`.");
    }
    return global->second;
  }

  void make_variable(const std::string& name, Value value) {
    for (auto frame = frames_.rbegin(); frame != frames_.rend(); ++frame) {
      const auto local = frame->find(name);
      if (local != frame->end()) {
        local->second = std::move(value);
        return;
      }
    }
    globals_[name] = std::move(value);
  }

  void expect_token(const std::vector<Token>& tokens, std::size_t& position,
                    const std::string& expected, const std::string& context) {
    if (position >= tokens.size() || tokens[position].text != expected) {
      fail(line_at(tokens, position), context + " expects `" + expected + "`.");
    }
    ++position;
  }

  Value parse_expression(const std::vector<Token>& tokens,
                         std::size_t& position, std::size_t depth = 0) {
    if (depth > kMaximumNestingDepth) {
      fail(line_at(tokens, position), "expression nesting exceeds 64 levels.");
    }
    return parse_comparison(tokens, position, depth + 1);
  }

  Value parse_comparison(const std::vector<Token>& tokens,
                         std::size_t& position, std::size_t depth) {
    Value left = parse_sum(tokens, position, depth);
    const auto operation = peek(tokens, position).text;
    if (operation == "=" || operation == "<>" || operation == "!=" ||
        operation == "<" || operation == "<=" || operation == ">" ||
        operation == ">=") {
      const auto line = line_at(tokens, position++);
      const Value right = parse_sum(tokens, position, depth);
      if (operation == "=") return Value(equal_values(left, right));
      if (operation == "<>" || operation == "!=") {
        return Value(!equal_values(left, right));
      }
      bool result = false;
      if (const auto* left_number = std::get_if<double>(&left.data)) {
        const auto* right_number = std::get_if<double>(&right.data);
        if (right_number == nullptr) {
          fail(line, "numeric comparison requires two numbers.");
        }
        if (operation == "<") result = *left_number < *right_number;
        if (operation == "<=") result = *left_number <= *right_number;
        if (operation == ">") result = *left_number > *right_number;
        if (operation == ">=") result = *left_number >= *right_number;
      } else if (const auto* left_word = std::get_if<std::string>(&left.data)) {
        const auto* right_word = std::get_if<std::string>(&right.data);
        if (right_word == nullptr) {
          fail(line, "word comparison requires two words.");
        }
        if (operation == "<") result = *left_word < *right_word;
        if (operation == "<=") result = *left_word <= *right_word;
        if (operation == ">") result = *left_word > *right_word;
        if (operation == ">=") result = *left_word >= *right_word;
      } else {
        fail(line, "ordered comparisons require two numbers or two words.");
      }
      return Value(result);
    }
    return left;
  }

  Value parse_sum(const std::vector<Token>& tokens, std::size_t& position,
                  std::size_t depth) {
    Value value = parse_product(tokens, position, depth);
    while (peek(tokens, position).text == "+" ||
           peek(tokens, position).text == "-") {
      const auto operation = tokens[position++].text;
      const auto line = line_at(tokens, position - 1);
      const auto left = as_number(value, line, operation);
      const auto right = as_number(parse_product(tokens, position, depth), line,
                                   operation);
      value = Value(operation == "+" ? left + right : left - right);
    }
    return value;
  }

  Value parse_product(const std::vector<Token>& tokens, std::size_t& position,
                      std::size_t depth) {
    Value value = parse_unary(tokens, position, depth);
    while (peek(tokens, position).text == "*" ||
           peek(tokens, position).text == "/") {
      const auto operation = tokens[position++].text;
      const auto line = line_at(tokens, position - 1);
      const auto left = as_number(value, line, operation);
      const auto right = as_number(parse_unary(tokens, position, depth), line,
                                   operation);
      if (operation == "/" && right == 0.0) fail(line, "division by zero.");
      value = Value(operation == "*" ? left * right : left / right);
    }
    return value;
  }

  Value parse_unary(const std::vector<Token>& tokens, std::size_t& position,
                    std::size_t depth) {
    if (peek(tokens, position).text == "+") {
      ++position;
      return Value(as_number(parse_unary(tokens, position, depth + 1),
                             line_at(tokens, position - 1), "unary plus"));
    }
    if (peek(tokens, position).text == "-") {
      const auto line = line_at(tokens, position++);
      return Value(-as_number(parse_unary(tokens, position, depth + 1), line,
                              "unary minus"));
    }
    return parse_primary(tokens, position, depth + 1);
  }

  Value parse_list_literal(const std::vector<Token>& tokens,
                           std::size_t& position, std::size_t depth) {
    const auto line = line_at(tokens, position);
    if (depth > kMaximumNestingDepth) {
      fail(line, "list nesting exceeds 64 levels.");
    }
    expect_token(tokens, position, "[", "list");
    Value::List values;
    while (position < tokens.size() && peek(tokens, position).text != "]") {
      if (tokens[position].newline) {
        ++position;
        continue;
      }
      if (values.size() >= kMaximumListLength) {
        fail(line, "list exceeds the 4096-item limit.");
      }
      const auto& token = tokens[position];
      if (token.text == "[") {
        values.push_back(parse_list_literal(tokens, position, depth + 1));
      } else if (token.text.front() == '"') {
        values.emplace_back(quoted_word(token.text, token.line));
        ++position;
      } else if (token.text.front() == ':') {
        values.emplace_back(token.text);
        ++position;
      } else if ((token.text == "-" || token.text == "+") &&
                 position + 1 < tokens.size()) {
        char* end = nullptr;
        const auto signed_number = token.text + tokens[position + 1].text;
        const double number = std::strtod(signed_number.c_str(), &end);
        if (end != signed_number.c_str() && *end == '\0' &&
            std::isfinite(number)) {
          values.emplace_back(number);
          position += 2;
        } else {
          values.emplace_back(token.text);
          ++position;
        }
      } else {
        char* end = nullptr;
        const double number = std::strtod(token.text.c_str(), &end);
        if (end != token.text.c_str() && *end == '\0' &&
            std::isfinite(number)) {
          values.emplace_back(number);
        } else {
          values.emplace_back(token.text);
        }
        ++position;
      }
    }
    expect_token(tokens, position, "]", "list");
    return Value(std::move(values));
  }

  Value parse_primary(const std::vector<Token>& tokens, std::size_t& position,
                      std::size_t depth) {
    const auto token = peek(tokens, position);
    if (depth > kMaximumNestingDepth) {
      fail(token.line, "expression nesting exceeds 64 levels.");
    }
    if (token.newline || token.text.empty() || token.text == "]") {
      fail(token.line, "expected a Logo value.");
    }
    if (token.text == "(") {
      ++position;
      auto value = parse_expression(tokens, position, depth + 1);
      expect_token(tokens, position, ")", "parenthesized expression");
      return value;
    }
    if (token.text == "[") return parse_list_literal(tokens, position, depth);
    if (token.text.front() == '"') {
      ++position;
      return Value(quoted_word(token.text, token.line));
    }
    if (token.text.front() == ':') {
      ++position;
      const auto name = uppercase(token.text.substr(1));
      if (!is_valid_name(name)) fail(token.line, "invalid variable reference.");
      return variable(name, token.line);
    }

    char* end = nullptr;
    const double number = std::strtod(token.text.c_str(), &end);
    if (end != token.text.c_str() && *end == '\0') {
      ++position;
      if (!std::isfinite(number)) fail(token.line, "number must be finite.");
      return Value(number);
    }

    const auto name = uppercase(token.text);
    ++position;
    if (name == "TRUE") return Value(true);
    if (name == "FALSE") return Value(false);
    if (name == "PI") return Value(3.14159265358979323846);
    const int arity = builtin_arity(name);
    if (arity >= 0) {
      return evaluate_builtin(name, arity, tokens, position, depth + 1,
                              token.line);
    }
    const auto procedure = program_.procedures.find(name);
    if (procedure != program_.procedures.end()) {
      std::vector<Value> arguments;
      arguments.reserve(procedure->second.parameters.size());
      for (std::size_t index = 0;
           index < procedure->second.parameters.size(); ++index) {
        arguments.push_back(parse_unary(tokens, position, depth + 1));
      }
      return call_procedure(name, arguments, true, token.line);
    }
    fail(token.line, "unknown reporter or word `" + token.text +
                         "`; quote words, for example `\"C4`.");
  }

  Value parse_arg(const std::vector<Token>& tokens, std::size_t& position,
                  std::size_t depth, std::size_t line) {
    if (depth > kMaximumNestingDepth) {
      fail(line, "reporter nesting exceeds 64 levels.");
    }
    if (position >= tokens.size() || tokens[position].newline ||
        tokens[position].text == "]") {
      fail(line, "reporter is missing an input.");
    }
    return parse_unary(tokens, position, depth + 1);
  }

  Value evaluate_builtin(const std::string& name, int arity,
                         const std::vector<Token>& tokens,
                         std::size_t& position, std::size_t depth,
                         std::size_t line) {
    if (name == "REPCOUNT") {
      return Value(repeat_counts_.empty() ? 0.0 : repeat_counts_.back());
    }
    if (name == "THING") {
      const Value word = parse_arg(tokens, position, depth, line);
      return variable(uppercase(as_word(word, line, "THING")), line);
    }
    if (arity == 0) return Value(false);

    Value first_value = parse_arg(tokens, position, depth, line);
    if (arity == 1) {
      if (name == "RANDOM") return Value(random_below(
          as_number(first_value, line, "RANDOM"), line));
      if (name == "FIRST" || name == "LAST" || name == "BUTFIRST" ||
          name == "BUTLAST" || name == "COUNT" || name == "EMPTY?" ||
          name == "PICK" || name == "REVERSE") {
        return sequence_reporter(name, first_value, line);
      }
      if (name == "WORD?") return Value(std::holds_alternative<std::string>(first_value.data));
      if (name == "LIST?") return Value(std::holds_alternative<Value::List>(first_value.data));
      if (name == "NUMBER?") return Value(std::holds_alternative<double>(first_value.data));
      if (name == "BOOLEAN?") return Value(std::holds_alternative<bool>(first_value.data));
      if (name == "NOT") return Value(!as_boolean(first_value, line, "NOT"));
      const double first = as_number(first_value, line, name);
      const double radians = first * 3.14159265358979323846 / 180.0;
      if (name == "ABS") return Value(std::abs(first));
      if (name == "INT") return Value(std::trunc(first));
      if (name == "ROUND") return Value(std::round(first));
      if (name == "SQRT") {
        if (first < 0.0) fail(line, "SQRT requires a nonnegative number.");
        return Value(std::sqrt(first));
      }
      if (name == "SIN") return Value(std::sin(radians));
      if (name == "COS") return Value(std::cos(radians));
      if (name == "TAN") return Value(std::tan(radians));
      if (name == "EXP") return Value(std::exp(first));
      if (name == "LN") {
        if (first <= 0.0) fail(line, "LN requires a positive number.");
        return Value(std::log(first));
      }
      if (name == "LOG10") {
        if (first <= 0.0) fail(line, "LOG10 requires a positive number.");
        return Value(std::log10(first));
      }
      if (name == "ARCSIN") return Value(std::asin(first) * 180.0 / 3.14159265358979323846);
      if (name == "ARCCOS") return Value(std::acos(first) * 180.0 / 3.14159265358979323846);
      if (name == "ARCTAN") return Value(std::atan(first) * 180.0 / 3.14159265358979323846);
      if (name == "RADARCSIN") return Value(std::asin(first));
      if (name == "RADARCCOS") return Value(std::acos(first));
      if (name == "RADARCTAN") return Value(std::atan(first));
      return Value(false);
    }

    Value second_value = parse_arg(tokens, position, depth, line);
    if (name == "LIST") {
      return Value(Value::List{std::move(first_value), std::move(second_value)});
    }
    if (name == "WORD") {
      if (std::holds_alternative<Value::List>(first_value.data) ||
          std::holds_alternative<Value::List>(second_value.data)) {
        fail(line, "WORD inputs must be words, numbers, or booleans.");
      }
      return Value(value_string(first_value) + value_string(second_value));
    }
    if (name == "SENTENCE" || name == "SE") {
      Value::List result;
      append_sentence(result, first_value, line);
      append_sentence(result, second_value, line);
      return Value(std::move(result));
    }
    if (name == "FPUT" || name == "LPUT") {
      const auto* list = std::get_if<Value::List>(&second_value.data);
      if (list == nullptr) fail(line, name + " expects a list as its second input.");
      if (list->size() >= kMaximumListLength) fail(line, "resulting list is too long.");
      auto result = *list;
      if (name == "FPUT") result.insert(result.begin(), std::move(first_value));
      else result.push_back(std::move(first_value));
      return Value(std::move(result));
    }
    if (name == "ITEM" || name == "MEMBER?") {
      if (name == "ITEM") {
        const auto index_value = as_number(first_value, line, "ITEM");
        const auto index = checked_index(index_value, line);
        return sequence_item(second_value, index, line);
      }
      return Value(is_member(first_value, second_value, line));
    }
    if (name == "AND" || name == "OR") {
      const bool left = as_boolean(first_value, line, name);
      const bool right = as_boolean(second_value, line, name);
      return Value(name == "AND" ? left && right : left || right);
    }
    if (name == "EQUAL?") return Value(equal_values(first_value, second_value));
    if (name == "NOTEQUAL?") return Value(!equal_values(first_value, second_value));
    if (name == "LESS?" || name == "GREATER?") {
      bool result = false;
      if (const auto* left_number = std::get_if<double>(&first_value.data)) {
        const auto* right_number = std::get_if<double>(&second_value.data);
        if (right_number == nullptr) fail(line, name + " inputs must have matching types.");
        result = name == "LESS?" ? *left_number < *right_number
                                 : *left_number > *right_number;
      } else if (const auto* left_word = std::get_if<std::string>(&first_value.data)) {
        const auto* right_word = std::get_if<std::string>(&second_value.data);
        if (right_word == nullptr) fail(line, name + " inputs must have matching types.");
        result = name == "LESS?" ? *left_word < *right_word
                                 : *left_word > *right_word;
      } else {
        fail(line, name + " expects two numbers or two words.");
      }
      return Value(result);
    }
    const double first = as_number(first_value, line, name);
    const double second = as_number(second_value, line, name);
    if (name == "SUM") return Value(first + second);
    if (name == "DIFFERENCE") return Value(first - second);
    if (name == "PRODUCT") return Value(first * second);
    if (name == "QUOTIENT") {
      if (second == 0.0) fail(line, "QUOTIENT cannot divide by zero.");
      return Value(first / second);
    }
    if (name == "REMAINDER") {
      if (second == 0.0) fail(line, "REMAINDER cannot divide by zero.");
      return Value(std::fmod(first, second));
    }
    if (name == "MODULO") {
      if (second == 0.0) fail(line, "MODULO cannot divide by zero.");
      return Value(first - std::floor(first / second) * second);
    }
    if (name == "POWER") return Value(std::pow(first, second));
    if (name == "MIN") return Value(std::min(first, second));
    if (name == "MAX") return Value(std::max(first, second));
    return Value(false);
  }

  void append_sentence(Value::List& destination, const Value& value,
                       std::size_t line) const {
    if (const auto* list = std::get_if<Value::List>(&value.data)) {
      if (destination.size() + list->size() > kMaximumListLength) {
        fail(line, "SENTENCE result exceeds the 4096-item limit.");
      }
      destination.insert(destination.end(), list->begin(), list->end());
      return;
    }
    destination.push_back(value);
  }

  std::size_t checked_index(double value, std::size_t line) const {
    if (value < 1.0 || value > static_cast<double>(kMaximumListLength) ||
        value != std::trunc(value)) {
      fail(line, "item index must be a positive integer.");
    }
    return static_cast<std::size_t>(value - 1.0);
  }

  Value sequence_item(const Value& sequence, std::size_t index,
                      std::size_t line) const {
    if (const auto* list = std::get_if<Value::List>(&sequence.data)) {
      if (index >= list->size()) fail(line, "item index is outside the list.");
      return (*list)[index];
    }
    if (const auto* word = std::get_if<std::string>(&sequence.data)) {
      if (index >= word->size()) fail(line, "item index is outside the word.");
      return Value(word->substr(index, 1));
    }
    fail(line, "expected a word or list.");
  }

  bool is_member(const Value& item, const Value& sequence,
                 std::size_t line) const {
    if (const auto* list = std::get_if<Value::List>(&sequence.data)) {
      return std::find(list->begin(), list->end(), item) != list->end();
    }
    if (const auto* word = std::get_if<std::string>(&sequence.data)) {
      const auto needle = as_word(item, line, "MEMBER?");
      return word->find(needle) != std::string::npos;
    }
    fail(line, "MEMBER? expects a word or list as its second input.");
  }

  Value sequence_reporter(const std::string& name, const Value& sequence,
                          std::size_t line) {
    if (const auto* list = std::get_if<Value::List>(&sequence.data)) {
      if (name == "COUNT") return Value(static_cast<double>(list->size()));
      if (name == "EMPTY?") return Value(list->empty());
      if (name == "FIRST" || name == "LAST" || name == "PICK") {
        if (list->empty()) fail(line, name + " cannot use an empty list.");
        if (name == "FIRST") return list->front();
        if (name == "LAST") return list->back();
        std::uniform_int_distribution<std::size_t> choose(0, list->size() - 1);
        return (*list)[choose(random_)];
      }
      if (name == "BUTFIRST" || name == "BUTLAST") {
        auto result = *list;
        if (!result.empty()) {
          if (name == "BUTFIRST") result.erase(result.begin());
          else result.pop_back();
        }
        return Value(std::move(result));
      }
      if (name == "REVERSE") {
        auto result = *list;
        std::reverse(result.begin(), result.end());
        return Value(std::move(result));
      }
    }
    if (const auto* word = std::get_if<std::string>(&sequence.data)) {
      if (name == "COUNT") return Value(static_cast<double>(word->size()));
      if (name == "EMPTY?") return Value(word->empty());
      if (name == "FIRST" || name == "LAST" || name == "PICK") {
        if (word->empty()) fail(line, name + " cannot use an empty word.");
        if (name == "FIRST") return Value(word->substr(0, 1));
        if (name == "LAST") return Value(word->substr(word->size() - 1, 1));
        std::uniform_int_distribution<std::size_t> choose(0, word->size() - 1);
        return Value(word->substr(choose(random_), 1));
      }
      if (name == "BUTFIRST") {
        return Value(word->empty() ? std::string{} : word->substr(1));
      }
      if (name == "BUTLAST") {
        return Value(word->empty() ? std::string{} :
                                      word->substr(0, word->size() - 1));
      }
      if (name == "REVERSE") {
        auto result = *word;
        std::reverse(result.begin(), result.end());
        return Value(std::move(result));
      }
    }
    fail(line, name + " expects a word or list.");
  }

  double random_below(double limit_value, std::size_t line) {
    if (limit_value < 0.0 ||
        limit_value > static_cast<double>(std::numeric_limits<int>::max()) ||
        limit_value != std::trunc(limit_value)) {
      fail(line, "RANDOM expects a nonnegative integer limit.");
    }
    const int limit = static_cast<int>(limit_value);
    return limit == 0 ? 0.0
                      : std::uniform_int_distribution<int>(0, limit - 1)(
                            random_);
  }

  std::vector<Token> take_block(const std::vector<Token>& tokens,
                                std::size_t& position,
                                std::size_t control_depth,
                                const std::string& name) {
    skip_newlines(tokens, position);
    const auto line = line_at(tokens, position);
    if (peek(tokens, position).text != "[") {
      fail(line, name + " expects a command block in `[ ]`.");
    }
    if (control_depth >= kMaximumNestingDepth) {
      fail(line, "control-flow nesting exceeds the 64-level limit.");
    }
    ++position;
    std::vector<Token> body;
    std::size_t depth = 1;
    while (position < tokens.size() && depth > 0) {
      const auto token = tokens[position++];
      if (token.text == "[") {
        ++depth;
        body.push_back(token);
      } else if (token.text == "]") {
        --depth;
        if (depth > 0) body.push_back(token);
      } else {
        body.push_back(token);
      }
    }
    if (depth != 0) fail(line, name + " is missing a closing `]`.");
    return body;
  }

  Value evaluate_condition_block(const std::vector<Token>& condition,
                                 std::size_t line) {
    std::size_t position = 0;
    auto result = parse_expression(condition, position);
    skip_newlines(condition, position);
    if (position != condition.size()) {
      fail(line, "condition has unexpected extra input.");
    }
    return result;
  }

  Value call_procedure(const std::string& name,
                       const std::vector<Value>& arguments,
                       bool expects_output, std::size_t line) {
    if (frames_.size() >= kMaximumNestingDepth) {
      fail(line, "procedure call depth exceeds 64 levels.");
    }
    const auto& procedure = program_.procedures.at(name);
    if (arguments.size() != procedure.parameters.size()) {
      fail(line, "procedure `" + name + "` received the wrong number of inputs.");
    }
    Frame frame;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
      frame.emplace(procedure.parameters[index], arguments[index]);
    }
    frames_.push_back(std::move(frame));
    struct FrameGuard {
      std::vector<Frame>& frames;
      ~FrameGuard() { frames.pop_back(); }
    } guard{frames_};

    std::size_t position = 0;
    try {
      execute_sequence(procedure.body, position, 0, false);
    } catch (const OutputSignal& output) {
      return output.value;
    } catch (const StopSignal&) {
      if (expects_output) {
        fail(line, "reporter procedure `" + name + "` stopped without OUTPUT.");
      }
      return Value();
    }
    if (expects_output) {
      fail(line, "reporter procedure `" + name + "` finished without OUTPUT.");
    }
    return Value();
  }

  std::vector<int> parse_chord(const std::vector<Token>& tokens,
                               std::size_t& position, std::size_t line) {
    Value value;
    if (peek(tokens, position).text == "[") {
      auto pitch_tokens = take_block(tokens, position, 0, "CHORD");
      std::size_t pitch_position = 0;
      Value::List pitches;
      while (pitch_position < pitch_tokens.size()) {
        skip_newlines(pitch_tokens, pitch_position);
        if (pitch_position >= pitch_tokens.size()) break;
        pitches.push_back(parse_expression(pitch_tokens, pitch_position));
      }
      value = Value(std::move(pitches));
    } else {
      value = parse_expression(tokens, position);
    }
    const auto* list = std::get_if<Value::List>(&value.data);
    if (list == nullptr || list->empty()) {
      fail(line, "CHORD expects a nonempty list of numeric pitches.");
    }
    std::vector<int> pitches;
    pitches.reserve(list->size());
    for (const auto& pitch : *list) {
      const auto number = as_number(pitch, line, "CHORD pitch");
      if (number != std::trunc(number) || number < 0 || number > 127) {
        fail(line, "CHORD pitches must be integers from 0 to 127.");
      }
      pitches.push_back(static_cast<int>(number));
    }
    return pitches;
  }

  void execute_sequence(const std::vector<Token>& tokens,
                       std::size_t& position, std::size_t control_depth,
                       bool inside_block) {
    while (position < tokens.size()) {
      skip_newlines(tokens, position);
      if (position >= tokens.size()) return;
      if (tokens[position].text == "]") {
        if (inside_block) return;
        fail(tokens[position].line, "unexpected closing `]`.");
      }
      const auto line = tokens[position].line;
      const auto command = uppercase(tokens[position++].text);
      if (++executed_commands_ > kMaximumCommands) {
        fail(line, "script exceeds the 4096 executed-command limit.");
      }

      if (command == "REPEAT") {
        const auto count_value =
            as_number(parse_expression(tokens, position), line, "REPEAT");
        if (count_value < 0 || count_value > kMaximumLoopIterations ||
            count_value != std::trunc(count_value)) {
          fail(line, "REPEAT count must be an integer from 0 to 4096.");
        }
        const auto body = take_block(tokens, position, control_depth,
                                     "REPEAT");
        const auto count = static_cast<std::size_t>(count_value);
        for (std::size_t iteration = 1; iteration <= count; ++iteration) {
          repeat_counts_.push_back(static_cast<double>(iteration));
          struct RepeatGuard {
            std::vector<double>& counts;
            ~RepeatGuard() { counts.pop_back(); }
          } repeat_guard{repeat_counts_};
          std::size_t body_position = 0;
          execute_sequence(body, body_position, control_depth + 1, true);
        }
        continue;
      }
      if (command == "IF" || command == "IFELSE") {
        const auto condition =
            parse_expression(tokens, position, control_depth + 1);
        const bool take_true = as_boolean(condition, line, command);
        const auto when_true =
            take_block(tokens, position, control_depth, command);
        std::vector<Token> when_false;
        if (command == "IFELSE") {
          when_false = take_block(tokens, position, control_depth, command);
        }
        auto& body = take_true ? when_true : when_false;
        std::size_t body_position = 0;
        execute_sequence(body, body_position, control_depth + 1, true);
        continue;
      }
      if (command == "WHILE" || command == "UNTIL") {
        const auto condition_tokens =
            take_block(tokens, position, control_depth, command);
        const auto body = take_block(tokens, position, control_depth, command);
        std::size_t iterations = 0;
        while (true) {
          const auto condition =
              evaluate_condition_block(condition_tokens, line);
          const bool truth = as_boolean(condition, line, command);
          if ((command == "WHILE" && !truth) ||
              (command == "UNTIL" && truth)) {
            break;
          }
          if (++iterations > kMaximumLoopIterations) {
            fail(line, command + " exceeds the 4096-iteration limit.");
          }
          std::size_t body_position = 0;
          execute_sequence(body, body_position, control_depth + 1, true);
        }
        continue;
      }
      if (command == "FOREVER") {
        const auto body = take_block(tokens, position, control_depth, command);
        for (std::size_t iteration = 0; iteration < kMaximumLoopIterations;
             ++iteration) {
          std::size_t body_position = 0;
          execute_sequence(body, body_position, control_depth + 1, true);
        }
        fail(line, "FOREVER stopped at the 4096-iteration safety limit.");
      }
      if (command == "MAKE" || command == "LOCALMAKE") {
        if (command == "LOCALMAKE" && frames_.empty()) {
          fail(line, "LOCALMAKE can only be used inside a procedure.");
        }
        const auto& name_token = peek(tokens, position);
        const auto name = uppercase(quoted_word(name_token.text, line));
        if (!is_valid_name(name)) fail(line, "invalid variable name.");
        ++position;
        auto value = parse_expression(tokens, position);
        if (command == "LOCALMAKE") frames_.back()[name] = std::move(value);
        else make_variable(name, std::move(value));
        continue;
      }
      if (command == "LOCAL") {
        if (frames_.empty()) fail(line, "LOCAL can only be used inside a procedure.");
        bool declared = false;
        while (position < tokens.size() && !peek(tokens, position).newline &&
               peek(tokens, position).text != "]") {
          const auto name = uppercase(quoted_word(tokens[position].text, line));
          if (!is_valid_name(name)) fail(line, "invalid local variable name.");
          frames_.back().try_emplace(name, Value());
          ++position;
          declared = true;
        }
        if (!declared) fail(line, "LOCAL expects one or more quoted names.");
        continue;
      }
      if (command == "RERANDOM") {
        if (position >= tokens.size() || peek(tokens, position).newline ||
            peek(tokens, position).text == "]") {
          random_.seed(1);
        } else {
          const auto seed = as_number(parse_expression(tokens, position), line,
                                      "RERANDOM");
          if (seed < 0 || seed > std::numeric_limits<std::uint32_t>::max() ||
              seed != std::trunc(seed)) {
            fail(line, "RERANDOM seed must be a nonnegative integer.");
          }
          random_.seed(static_cast<std::uint32_t>(seed));
        }
        continue;
      }
      if (command == "OUTPUT" || command == "OP") {
        if (frames_.empty()) fail(line, "OUTPUT can only be used inside a procedure.");
        throw OutputSignal{parse_expression(tokens, position)};
      }
      if (command == "STOP") {
        if (frames_.empty()) throw StopSignal{};
        throw StopSignal{};
      }
      if (command == "TEMPO" || command == "WAIT" ||
          command == "VELOCITY" || command == "CHANNEL" ||
          command == "GATE" || command == "REST" || command == "PROGRAM" ||
          command == "STEAL" || command == "ANTICIPATE") {
        const auto value = parse_expression(tokens, position);
        const auto number = as_number(value, line, command);
        if (command == "TEMPO") engine_.set_tempo(number);
        else if (command == "WAIT") engine_.wait(number);
        else if (command == "VELOCITY") engine_.set_velocity(checked_integer(number, line, command));
        else if (command == "CHANNEL") engine_.set_channel(checked_integer(number, line, command));
        else if (command == "GATE") engine_.set_gate(number);
        else if (command == "REST") engine_.rest(number);
        else if (command == "PROGRAM") engine_.program_change(checked_integer(number, line, command));
        else if (command == "STEAL") engine_.steal(number);
        else engine_.anticipate(number);
        continue;
      }
      if (command == "NOTE") {
        const auto pitch = as_number(parse_expression(tokens, position), line, "NOTE pitch");
        const auto duration = as_number(parse_expression(tokens, position), line, "NOTE duration");
        engine_.note(checked_integer(pitch, line, "NOTE pitch"), duration);
        continue;
      }
      if (command == "CHORD") {
        const auto pitches = parse_chord(tokens, position, line);
        const auto duration =
            as_number(parse_expression(tokens, position), line, "CHORD duration");
        engine_.chord(pitches, duration);
        continue;
      }
      if (command == "CC") {
        const auto controller =
            as_number(parse_expression(tokens, position), line, "CC controller");
        const auto value = as_number(parse_expression(tokens, position), line, "CC value");
        engine_.control_change(checked_integer(controller, line, "CC controller"),
                               checked_integer(value, line, "CC value"));
        continue;
      }
      if (command == "SYNC") {
        fail(line, "SYNC is a VST setting; the command-line renderer uses internal tempo.");
      }
      if (program_.procedures.find(command) != program_.procedures.end()) {
        const auto& procedure = program_.procedures.at(command);
        std::vector<Value> arguments;
        arguments.reserve(procedure.parameters.size());
        for (std::size_t index = 0; index < procedure.parameters.size(); ++index) {
          arguments.push_back(parse_expression(tokens, position));
        }
        (void)call_procedure(command, arguments, false, line);
        continue;
      }
      fail(line, "unsupported command `" + command + "`.");
    }
    if (inside_block) return;
  }

  int checked_integer(double value, std::size_t line,
                      const std::string& purpose) const {
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max() ||
        value != std::trunc(value)) {
      fail(line, purpose + " must be an integer.");
    }
    return static_cast<int>(value);
  }

  MusicEngine engine_;
  std::mt19937 random_;
  Program program_;
  std::unordered_map<std::string, Value> globals_;
  Environment frames_;
  std::vector<double> repeat_counts_;
  std::size_t executed_commands_ = 0;
};

}  // namespace

MusicEngine compile_logo_music_script(const std::string& script,
                                      double initial_tempo) {
  return MusicLogoInterpreter(script, initial_tempo).run();
}

}  // namespace logo_midi
