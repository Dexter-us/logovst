#include "logo_midi/turtle_interpreter.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace logo_midi {
namespace {

constexpr std::size_t kMaximumScriptBytes = 64 * 1024;
constexpr std::size_t kMaximumCommands = 4096;
constexpr std::size_t kMaximumRepeatCount = 4096;
constexpr std::size_t kMaximumProcedures = 256;
constexpr std::size_t kMaximumNestingDepth = 64;

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string uppercase(std::string value) {
  for (char& character : value) {
    if (character >= 'a' && character <= 'z') {
      character = static_cast<char>(character - 'a' + 'A');
    }
  }
  return value;
}

std::string without_comment(const std::string& line) {
  bool in_quotes = false;
  for (std::size_t index = 0; index < line.size(); ++index) {
    if (line[index] == '"') {
      in_quotes = !in_quotes;
    } else if (line[index] == ';' && !in_quotes) {
      return line.substr(0, index);
    }
  }
  return line;
}

std::vector<std::string> tokenize(std::string_view text) {
  std::vector<std::string> tokens;
  std::string token;
  bool in_quotes = false;
  for (const char character : text) {
    if (character == '"') {
      in_quotes = !in_quotes;
      token.push_back(character);
    } else if (!in_quotes && (character == '[' || character == ']')) {
      if (!token.empty()) {
        tokens.push_back(std::move(token));
        token.clear();
      }
      tokens.emplace_back(1, character);
    } else if (!in_quotes &&
               std::isspace(static_cast<unsigned char>(character))) {
      if (!token.empty()) {
        tokens.push_back(std::move(token));
        token.clear();
      }
    } else {
      token.push_back(character);
    }
  }
  if (!token.empty()) {
    tokens.push_back(std::move(token));
  }
  return tokens;
}

[[noreturn]] void script_error(std::size_t line_number,
                               const std::string& message) {
  throw std::runtime_error("Line " + std::to_string(line_number) + ": " +
                           message);
}

double parse_number(const std::string& token, std::size_t line_number) {
  std::size_t consumed = 0;
  double value = 0.0;
  try {
    value = std::stod(token, &consumed);
  } catch (const std::exception&) {
    script_error(line_number, "expected a number, got `" + token + "`.");
  }
  if (consumed != token.size() || !std::isfinite(value)) {
    script_error(line_number, "invalid finite number `" + token + "`.");
  }
  return value;
}

int parse_integer(const std::string& token, std::size_t line_number) {
  const double value = parse_number(token, line_number);
  if (value < 0 || value > 255 || value != std::trunc(value)) {
    script_error(line_number, "color channels must be integers from 0 to 255.");
  }
  return static_cast<int>(value);
}

GraphicsColor take_color(const std::vector<std::string>& tokens,
                         std::size_t& index, std::size_t line_number,
                         const std::string& command) {
  if (index >= tokens.size() || tokens[index++] != "[") {
    script_error(line_number, command + " expects a color list [red green blue].");
  }
  const auto red = parse_integer(
      index < tokens.size() ? tokens[index++] : std::string{}, line_number);
  const auto green = parse_integer(
      index < tokens.size() ? tokens[index++] : std::string{}, line_number);
  const auto blue = parse_integer(
      index < tokens.size() ? tokens[index++] : std::string{}, line_number);
  if (index >= tokens.size() || tokens[index++] != "]") {
    script_error(line_number, command + " expects a closing `]`.");
  }
  return {static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green),
          static_cast<std::uint8_t>(blue)};
}

void require_end(const std::vector<std::string>& tokens, std::size_t index,
                 std::size_t line_number) {
  if (index < tokens.size()) {
    script_error(line_number, "unexpected extra input `" + tokens[index] +
                                  "`.");
  }
}

void require_argument_count(const std::vector<std::string>& tokens,
                            std::size_t expected, std::size_t line_number,
                            const std::string& command) {
  if (tokens.size() != expected) {
    script_error(line_number, command + " expects " +
                                  std::to_string(expected) + " argument(s).");
  }
}

std::string unquote_label(std::string text, std::size_t line_number) {
  if (text.empty()) {
    script_error(line_number, "LABEL requires text.");
  }
  if (text.front() == '"') {
    if (text.size() < 2 || text.back() != '"') {
      script_error(line_number, "LABEL has an unterminated quoted string.");
    }
    text = text.substr(1, text.size() - 2);
  }
  return text;
}

std::string color_text(GraphicsColor color) {
  return "PIXEL [" + std::to_string(color.red) + " " +
         std::to_string(color.green) + " " + std::to_string(color.blue) + "]";
}

void append_output(std::string& output, const std::string& message) {
  if (!output.empty()) {
    output += '\n';
  }
  output += message;
}

struct SourceLine {
  std::string text;
  std::size_t line_number = 0;
};

struct Token {
  std::string text;
  std::size_t line_number = 0;
  bool newline = false;
};

struct Command {
  std::string name;
  std::vector<std::string> arguments;
  std::vector<Command> body;
  std::size_t line_number = 0;
};

bool is_supported_command(const std::string& command) {
  static const std::vector<std::string> commands = {
      "FORWARD", "FD", "BACK", "BK", "LEFT", "LT", "RIGHT", "RT",
      "HOME", "SETPOS", "SETXY", "SETX", "SETY", "SETHEADING", "SETH",
      "PENUP", "PU", "PENDOWN", "PD", "SHOWTURTLE", "ST", "HIDETURTLE",
      "HT", "SETPENCOLOR", "SETPC", "SETSCREENCOLOR", "SETBACKGROUND",
      "SETFLOODCOLOR", "PENSIZE", "SETPENSIZE", "PENNORMAL", "PENERASE",
      "PENREVERSE", "WRAP", "FENCE", "WINDOW", "CLEAN", "CLEARSCREEN",
      "CS", "POLYSTART", "POLYEND", "FILL", "ELLIPSEARC", "LABEL",
      "LABELSIZE", "SETLABELSIZE", "SETLABELFONT", "SETPIXEL", "PIXEL",
      "REPEAT", "TO", "END"};
  return std::find(commands.begin(), commands.end(), command) != commands.end();
}

bool is_valid_procedure_name(const std::string& name) {
  if (name.empty()) {
    return false;
  }
  for (std::size_t index = 0; index < name.size(); ++index) {
    const auto character = static_cast<unsigned char>(name[index]);
    const bool letter = (character >= 'A' && character <= 'Z') ||
                        (character >= 'a' && character <= 'z');
    const bool digit = character >= '0' && character <= '9';
    if (index == 0) {
      if (!letter && character != '_') {
        return false;
      }
    } else if (!letter && !digit && character != '_' && character != '-' &&
               character != '?' && character != '!') {
      return false;
    }
  }
  return true;
}

std::vector<Token> make_tokens(const std::vector<SourceLine>& lines) {
  std::vector<Token> tokens;
  for (const auto& line : lines) {
    for (auto& token : tokenize(line.text)) {
      tokens.push_back({std::move(token), line.line_number, false});
    }
    tokens.push_back({"", line.line_number, true});
  }
  return tokens;
}

class ScriptParser {
 public:
  ScriptParser(std::vector<Token> tokens,
               const std::vector<std::string>& procedure_names)
      : tokens_(std::move(tokens)), procedure_names_(procedure_names) {}

  std::vector<Command> parse() {
    auto commands = parse_sequence(false);
    skip_newlines();
    if (position_ != tokens_.size()) {
      script_error(tokens_[position_].line_number, "unexpected `]`.");
    }
    return commands;
  }

 private:
  bool is_procedure(const std::string& name) const {
    return std::find(procedure_names_.begin(), procedure_names_.end(), name) !=
           procedure_names_.end();
  }

  bool is_command_start(const std::string& name) const {
    return is_supported_command(name) || is_procedure(name);
  }

  void skip_newlines() {
    while (position_ < tokens_.size() && tokens_[position_].newline) {
      ++position_;
    }
  }

  std::string take_argument(std::size_t line_number,
                            const std::string& command,
                            bool allow_closing_bracket = false) {
    if (position_ >= tokens_.size() || tokens_[position_].newline ||
        (tokens_[position_].text == "]" && !allow_closing_bracket)) {
      script_error(line_number, command + " has too few arguments.");
    }
    return tokens_[position_++].text;
  }

  std::vector<Command> parse_sequence(bool inside_repeat,
                                      std::size_t repeat_depth = 0) {
    std::vector<Command> commands;
    while (position_ < tokens_.size()) {
      skip_newlines();
      if (position_ >= tokens_.size()) {
        break;
      }
      if (tokens_[position_].text == "]") {
        if (!inside_repeat) {
          script_error(tokens_[position_].line_number, "unexpected `]`.");
        }
        ++position_;
        return commands;
      }

      const auto line_number = tokens_[position_].line_number;
      const auto name = uppercase(tokens_[position_++].text);
      Command command{name, {}, {}, line_number};
      if (name == "REPEAT") {
        const auto count_token = take_argument(line_number, name);
        const double count_value = parse_number(count_token, line_number);
        if (count_value < 0 || count_value > kMaximumRepeatCount ||
            count_value != std::trunc(count_value)) {
          script_error(line_number,
                       "REPEAT count must be an integer from 0 to 4096.");
        }
        if (position_ >= tokens_.size() || tokens_[position_].text != "[") {
          script_error(line_number, "REPEAT expects a command list in `[ ]`.");
        }
        if (repeat_depth >= kMaximumNestingDepth) {
          script_error(line_number, "REPEAT nesting exceeds the 64-level limit.");
        }
        ++position_;
        command.arguments.push_back(count_token);
        command.body = parse_sequence(true, repeat_depth + 1);
        commands.push_back(std::move(command));
        continue;
      }

      if (name == "LABEL" || name == "SETLABELFONT") {
        while (position_ < tokens_.size() && !tokens_[position_].newline &&
               tokens_[position_].text != "]") {
          command.arguments.push_back(tokens_[position_++].text);
        }
        if (command.arguments.empty()) {
          script_error(line_number, name + " requires text.");
        }
      } else if (is_procedure(name)) {
        // Procedures in this subset take no parameters.
      } else if (name == "ELLIPSEARC") {
        for (int i = 0; i < 3; ++i) {
          command.arguments.push_back(take_argument(line_number, name));
        }
        if (position_ < tokens_.size() && !tokens_[position_].newline &&
            tokens_[position_].text != "]" &&
            !is_command_start(uppercase(tokens_[position_].text))) {
          command.arguments.push_back(tokens_[position_++].text);
        }
      } else {
        std::size_t argument_count = 0;
        if (name == "FORWARD" || name == "FD" || name == "BACK" ||
            name == "BK" || name == "LEFT" || name == "LT" ||
            name == "RIGHT" || name == "RT" || name == "SETX" ||
            name == "SETY" || name == "SETHEADING" || name == "SETH" ||
            name == "PENSIZE" || name == "SETPENSIZE" ||
            name == "LABELSIZE" || name == "SETLABELSIZE") {
          argument_count = 1;
        } else if (name == "SETPOS" || name == "SETXY") {
          argument_count = 2;
        } else if (name == "SETPENCOLOR" || name == "SETPC" ||
                   name == "SETSCREENCOLOR" || name == "SETBACKGROUND" ||
                   name == "SETFLOODCOLOR" || name == "SETPIXEL") {
          argument_count = 5;
        }
        for (std::size_t i = 0; i < argument_count; ++i) {
          const bool color_list =
              name == "SETPENCOLOR" || name == "SETPC" ||
              name == "SETSCREENCOLOR" || name == "SETBACKGROUND" ||
              name == "SETFLOODCOLOR" || name == "SETPIXEL";
          command.arguments.push_back(take_argument(
              line_number, name, color_list && i == argument_count - 1));
        }
      }
      commands.push_back(std::move(command));
    }
    if (inside_repeat) {
      const auto line_number =
          tokens_.empty() ? 1 : tokens_.back().line_number;
      script_error(line_number, "REPEAT is missing a closing `]`.");
    }
    return commands;
  }

  std::vector<Token> tokens_;
  const std::vector<std::string>& procedure_names_;
  std::size_t position_ = 0;
};

std::vector<SourceLine> read_source_lines(std::string_view script) {
  std::vector<SourceLine> lines;
  std::istringstream reader{std::string(script)};
  std::string raw_line;
  std::size_t line_number = 0;
  while (std::getline(reader, raw_line)) {
    ++line_number;
    auto line = trim(without_comment(raw_line));
    if (!line.empty()) {
      lines.push_back({std::move(line), line_number});
    }
  }
  return lines;
}

struct ParsedScripts {
  std::vector<Command> main;
  std::unordered_map<std::string, std::vector<Command>> procedures;
};

ParsedScripts parse_source(
    const std::vector<SourceLine>& lines) {
  std::vector<SourceLine> main_lines;
  std::unordered_map<std::string, std::vector<SourceLine>> procedure_sources;
  std::string current_procedure;
  std::size_t procedure_start_line = 0;
  std::size_t main_repeat_depth = 0;

  for (const auto& line : lines) {
    const auto line_tokens = tokenize(line.text);
    const auto first = line_tokens.empty() ? std::string{} :
                                            uppercase(line_tokens.front());
    if (current_procedure.empty()) {
      if (first == "END") {
        script_error(line.line_number, "END has no matching TO.");
      }
      if (first == "TO") {
        if (main_repeat_depth != 0) {
          script_error(line.line_number,
                       "procedure definitions must be outside REPEAT blocks.");
        }
        if (line_tokens.size() != 2) {
          script_error(line.line_number,
                       "TO expects one procedure name and no parameters.");
        }
        current_procedure = uppercase(line_tokens[1]);
        if (!is_valid_procedure_name(current_procedure)) {
          script_error(line.line_number, "invalid procedure name `" +
                                             line_tokens[1] + "`.");
        }
        if (is_supported_command(current_procedure)) {
          script_error(line.line_number, "procedure name `" +
                                             current_procedure + "` is reserved.");
        }
        if (procedure_sources.size() >= kMaximumProcedures) {
          script_error(line.line_number,
                       "script exceeds the 256 procedure limit.");
        }
        if (procedure_sources.find(current_procedure) !=
            procedure_sources.end()) {
          script_error(line.line_number, "procedure `" + current_procedure +
                                             "` is already defined.");
        }
        procedure_start_line = line.line_number;
        procedure_sources.emplace(current_procedure,
                                   std::vector<SourceLine>{});
      } else {
        main_lines.push_back(line);
        bool in_quotes = false;
        for (const char character : line.text) {
          if (character == '"') {
            in_quotes = !in_quotes;
          } else if (!in_quotes && character == '[') {
            ++main_repeat_depth;
          } else if (!in_quotes && character == ']' &&
                     main_repeat_depth > 0) {
            --main_repeat_depth;
          }
        }
      }
      continue;
    }

    if (first == "TO") {
      script_error(line.line_number, "procedure definitions cannot be nested.");
    }
    if (first == "END") {
      if (line_tokens.size() != 1) {
        script_error(line.line_number, "END takes no arguments.");
      }
      current_procedure.clear();
    } else {
      procedure_sources[current_procedure].push_back(line);
    }
  }

  if (!current_procedure.empty()) {
    script_error(procedure_start_line,
                 "procedure `" + current_procedure + "` is missing END.");
  }

  std::vector<std::string> procedure_names;
  procedure_names.reserve(procedure_sources.size());
  for (const auto& procedure : procedure_sources) {
    procedure_names.push_back(procedure.first);
  }
  ParsedScripts parsed;
  for (const auto& procedure : procedure_sources) {
    parsed.procedures.emplace(
        procedure.first,
        ScriptParser(make_tokens(procedure.second), procedure_names).parse());
  }
  parsed.main = ScriptParser(make_tokens(main_lines), procedure_names).parse();
  return parsed;
}

}  // namespace

TurtleScriptResult execute_turtle_script(std::string_view script,
                                         TurtleGraphicsEngine& graphics) {
  if (script.size() > kMaximumScriptBytes) {
    throw std::runtime_error("Script exceeds the 64 KiB size limit.");
  }

  TurtleGraphicsEngine candidate = graphics;
  TurtleScriptResult result;
  const auto parsed = parse_source(read_source_lines(script));
  std::function<void(const std::vector<Command>&, std::size_t)> execute_commands;
  execute_commands = [&](const std::vector<Command>& commands,
                         std::size_t nesting_depth) {
    for (const auto& instruction : commands) {
      if (result.commands_executed >= kMaximumCommands) {
        script_error(instruction.line_number,
                     "script exceeds the 4096 command limit.");
      }
      ++result.commands_executed;
      const auto& command = instruction.name;
      const auto& tokens = instruction.arguments;
      std::string argument_text;
      for (const auto& token : tokens) {
        if (!argument_text.empty()) {
          argument_text += ' ';
        }
        argument_text += token;
      }
      std::size_t index = 0;

      if (command == "REPEAT") {
        if (nesting_depth >= kMaximumNestingDepth) {
          script_error(instruction.line_number,
                       "maximum REPEAT/procedure nesting depth is 64.");
        }
        const auto count = static_cast<std::size_t>(
            parse_number(tokens.front(), instruction.line_number));
        for (std::size_t iteration = 0; iteration < count; ++iteration) {
          execute_commands(instruction.body, nesting_depth + 1);
        }
        continue;
      }
      if (const auto procedure = parsed.procedures.find(command);
          procedure != parsed.procedures.end()) {
        if (nesting_depth >= kMaximumNestingDepth) {
          script_error(instruction.line_number,
                       "maximum REPEAT/procedure nesting depth is 64.");
        }
        execute_commands(procedure->second, nesting_depth + 1);
        continue;
      }

      if (command == "FORWARD" || command == "FD") {
      require_argument_count(tokens, 1, instruction.line_number, command);
      candidate.forward(parse_number(tokens[0], instruction.line_number));
    } else if (command == "BACK" || command == "BK") {
      require_argument_count(tokens, 1, instruction.line_number, command);
      candidate.back(parse_number(tokens[0], instruction.line_number));
    } else if (command == "LEFT" || command == "LT") {
      require_argument_count(tokens, 1, instruction.line_number, command);
      candidate.left(parse_number(tokens[0], instruction.line_number));
    } else if (command == "RIGHT" || command == "RT") {
      require_argument_count(tokens, 1, instruction.line_number, command);
      candidate.right(parse_number(tokens[0], instruction.line_number));
    } else if (command == "HOME") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.home();
    } else if (command == "SETPOS" || command == "SETXY") {
      require_argument_count(tokens, 2, instruction.line_number, command);
      candidate.set_position(parse_number(tokens[0], instruction.line_number),
                             parse_number(tokens[1], instruction.line_number));
    } else if (command == "SETX") {
      require_argument_count(tokens, 1, instruction.line_number, command);
      candidate.set_x(parse_number(tokens[0], instruction.line_number));
    } else if (command == "SETY") {
      require_argument_count(tokens, 1, instruction.line_number, command);
      candidate.set_y(parse_number(tokens[0], instruction.line_number));
    } else if (command == "SETHEADING" || command == "SETH") {
      require_argument_count(tokens, 1, instruction.line_number, command);
      candidate.set_heading(parse_number(tokens[0], instruction.line_number));
    } else if (command == "PENUP" || command == "PU") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.pen_up();
    } else if (command == "PENDOWN" || command == "PD") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.pen_down();
    } else if (command == "SHOWTURTLE" || command == "ST") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.show_turtle();
    } else if (command == "HIDETURTLE" || command == "HT") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.hide_turtle();
    } else if (command == "SETPENCOLOR" || command == "SETPC") {
      const auto color = take_color(tokens, index, instruction.line_number, command);
      require_end(tokens, index, instruction.line_number);
      candidate.set_pen_color(color);
    } else if (command == "SETSCREENCOLOR" || command == "SETBACKGROUND") {
      const auto color = take_color(tokens, index, instruction.line_number, command);
      require_end(tokens, index, instruction.line_number);
      candidate.set_background_color(color);
    } else if (command == "SETFLOODCOLOR") {
      const auto color = take_color(tokens, index, instruction.line_number, command);
      require_end(tokens, index, instruction.line_number);
      candidate.set_flood_color(color);
    } else if (command == "PENSIZE" || command == "SETPENSIZE") {
      require_argument_count(tokens, 1, instruction.line_number, command);
      candidate.set_pen_width(parse_number(tokens[0], instruction.line_number));
    } else if (command == "PENNORMAL") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.set_pen_mode(GraphicsPenMode::Paint);
    } else if (command == "PENERASE") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.set_pen_mode(GraphicsPenMode::Erase);
    } else if (command == "PENREVERSE") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.set_pen_mode(GraphicsPenMode::Reverse);
    } else if (command == "WRAP" || command == "FENCE" ||
               command == "WINDOW") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.set_boundary_mode(
          command == "WRAP"
              ? GraphicsBoundaryMode::Wrap
              : (command == "FENCE" ? GraphicsBoundaryMode::Fence
                                     : GraphicsBoundaryMode::Window));
    } else if (command == "CLEAN") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.clean();
    } else if (command == "CLEARSCREEN" || command == "CS") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.clear_screen();
    } else if (command == "POLYSTART") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.start_polygon();
    } else if (command == "POLYEND") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.end_polygon();
    } else if (command == "FILL") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      candidate.fill();
      append_output(result.output,
                    "FILL recorded; flood-fill rendering is not implemented yet.");
    } else if (command == "ELLIPSEARC") {
      if (tokens.size() != 3 && tokens.size() != 4) {
        script_error(instruction.line_number,
                     "ELLIPSEARC expects sweep, x-radius, y-radius, and "
                     "optional start angle.");
      }
      const auto sweep = parse_number(tokens[0], instruction.line_number);
      const auto radius_x = parse_number(tokens[1], instruction.line_number);
      const auto radius_y = parse_number(tokens[2], instruction.line_number);
      const auto start =
          tokens.size() == 4 ? parse_number(tokens[3], instruction.line_number) : 0.0;
      candidate.ellipse_arc(sweep, radius_x, radius_y, start);
    } else if (command == "LABEL") {
      candidate.label(unquote_label(argument_text, instruction.line_number));
    } else if (command == "LABELSIZE" || command == "SETLABELSIZE") {
      require_argument_count(tokens, 1, instruction.line_number, command);
      candidate.set_label_font_size(parse_number(tokens[0], instruction.line_number));
    } else if (command == "SETLABELFONT") {
      candidate.set_label_font(unquote_label(argument_text, instruction.line_number));
    } else if (command == "SETPIXEL") {
      const auto color = take_color(tokens, index, instruction.line_number, command);
      require_end(tokens, index, instruction.line_number);
      candidate.set_pixel(color);
    } else if (command == "PIXEL") {
      require_argument_count(tokens, 0, instruction.line_number, command);
      append_output(result.output, color_text(candidate.pixel_at_turtle()) +
                                       " (vector-scene sampling is approximate)");
    } else {
      script_error(instruction.line_number, "unsupported turtle command `" + command +
                                    "`.");
    }
    }
  };
  execute_commands(parsed.main, 0);

  graphics = std::move(candidate);
  return result;
}

}  // namespace logo_midi
