#include "logo_midi/graphics_engine.hpp"
#include "logo_midi/music_engine.hpp"
#include "logo_midi/product_info.hpp"
#include "logo_midi/svg_renderer.hpp"
#include "logo_midi/turtle_interpreter.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void print_usage() {
  std::cout
      << logo_midi::kProductName << " command-line prototype by "
      << logo_midi::kManufacturer << '\n'
      << "Usage:\n"
      << "  logo-midi <music-script.lgo> --output <file.mid>\n"
      << "  logo-midi --graphics <turtle-script.lgo> --output <drawing.svg>\n\n"
      << "Supported music commands (one command per line):\n"
      << "  TEMPO 1..1000 | VELOCITY 1..127 | CHANNEL 1..16 | GATE 0..1\n"
      << "  NOTE pitch beats | CHORD [pitch ...] beats | REST beats\n"
      << "  CC controller value | PROGRAM 0..127\n\n"
      << "Graphics mode runs the supported Logo turtle commands, REPEAT blocks,\n"
      << "and parameterized TO/END procedures, then writes a standalone SVG.\n";
}

double number(const std::string& text, const std::string& line,
              std::size_t line_number) {
  std::size_t consumed = 0;
  double value = 0.0;
  try {
    value = std::stod(text, &consumed);
  } catch (const std::exception&) {
    throw std::runtime_error("Line " + std::to_string(line_number) +
                             ": expected a number in `" + line + "`.");
  }
  if (consumed != text.size()) {
    throw std::runtime_error("Line " + std::to_string(line_number) +
                             ": invalid number `" + text + "`.");
  }
  return value;
}

int integer(const std::string& text, const std::string& line,
            std::size_t line_number) {
  const double value = number(text, line, line_number);
  if (!std::isfinite(value) || value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max() ||
      value != std::trunc(value)) {
    throw std::runtime_error("Line " + std::to_string(line_number) +
                             ": expected an integer in `" + line + "`.");
  }
  return static_cast<int>(value);
}

void require_no_extra(std::istringstream& words, std::size_t line_number) {
  std::string extra;
  if (words >> extra) {
    throw std::runtime_error("Line " + std::to_string(line_number) +
                             ": unexpected extra input `" + extra + "`.");
  }
}

void run_script(const std::string& script_path, logo_midi::MusicEngine& engine) {
  std::ifstream script(script_path);
  if (!script) {
    throw std::runtime_error("Could not open script: " + script_path);
  }

  std::string line;
  std::size_t line_number = 0;
  while (std::getline(script, line)) {
    ++line_number;
    const auto comment = line.find(';');
    if (comment != std::string::npos) {
      line.erase(comment);
    }
    std::istringstream words(line);
    std::string command;
    if (!(words >> command)) {
      continue;
    }
    for (char& character : command) {
      if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - 'a' + 'A');
      }
    }

    std::string first;
    std::string second;
    if (command == "TEMPO" || command == "VELOCITY" || command == "CHANNEL" ||
        command == "GATE" || command == "REST" || command == "PROGRAM") {
      if (!(words >> first)) {
        throw std::runtime_error("Line " + std::to_string(line_number) +
                                 ": missing argument for " + command + ".");
      }
      const auto value = number(first, line, line_number);
      if (command == "TEMPO") engine.set_tempo(value);
      if (command == "VELOCITY") engine.set_velocity(integer(first, line, line_number));
      if (command == "CHANNEL") engine.set_channel(integer(first, line, line_number));
      if (command == "GATE") engine.set_gate(value);
      if (command == "REST") engine.rest(value);
      if (command == "PROGRAM") engine.program_change(integer(first, line, line_number));
      require_no_extra(words, line_number);
    } else if (command == "NOTE") {
      if (!(words >> first >> second)) {
        throw std::runtime_error("Line " + std::to_string(line_number) +
                                 ": NOTE requires pitch and duration.");
      }
      engine.note(integer(first, line, line_number), number(second, line, line_number));
      require_no_extra(words, line_number);
    } else if (command == "CHORD") {
      std::string chord_text;
      std::getline(words, chord_text);
      const auto open = chord_text.find('[');
      const auto close = chord_text.find(']', open == std::string::npos ? 0 : open + 1);
      if (open == std::string::npos || close == std::string::npos) {
        throw std::runtime_error("Line " + std::to_string(line_number) +
                                 ": CHORD expects a list such as [60 64 67].");
      }
      std::vector<int> pitches;
      const auto pitch_text = chord_text.substr(open + 1, close - open - 1);
      std::istringstream pitch_words(pitch_text);
      std::string pitch;
      while (pitch_words >> pitch) {
        pitches.push_back(integer(pitch, line, line_number));
      }
      if (pitches.empty()) {
        throw std::runtime_error("Line " + std::to_string(line_number) +
                                 ": chord list cannot be empty.");
      }
      std::istringstream remainder(chord_text.substr(close + 1));
      if (!(remainder >> second)) {
        throw std::runtime_error("Line " + std::to_string(line_number) +
                                 ": CHORD requires a duration.");
      }
      engine.chord(pitches, number(second, line, line_number));
      require_no_extra(remainder, line_number);
    } else if (command == "CC") {
      std::string value;
      if (!(words >> first >> value)) {
        throw std::runtime_error("Line " + std::to_string(line_number) +
                                 ": CC requires controller and value.");
      }
      engine.control_change(integer(first, line, line_number),
                            integer(value, line, line_number));
      require_no_extra(words, line_number);
    } else if (command == "SYNC") {
      throw std::runtime_error(
          "SYNC is a VST setting; the command-line renderer always uses its internal tempo.");
    } else {
      throw std::runtime_error("Line " + std::to_string(line_number) +
                               ": unsupported command `" + command + "`.");
    }
  }
}

std::string read_turtle_script(const std::string& script_path) {
  constexpr std::size_t kMaximumScriptBytes = 64 * 1024;
  std::ifstream script(script_path, std::ios::binary);
  if (!script) {
    throw std::runtime_error("Could not open script: " + script_path);
  }

  std::string contents(kMaximumScriptBytes + 1, '\0');
  script.read(contents.data(), static_cast<std::streamsize>(contents.size()));
  const auto bytes_read = static_cast<std::size_t>(script.gcount());
  if (bytes_read > kMaximumScriptBytes) {
    throw std::runtime_error("Script exceeds the 64 KiB size limit.");
  }
  if (script.bad()) {
    throw std::runtime_error("Could not read script: " + script_path);
  }
  contents.resize(bytes_read);
  return contents;
}

void write_svg(const std::string& output_path, const std::string& svg) {
  std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw std::runtime_error("Could not write SVG: " + output_path);
  }
  output.write(svg.data(), static_cast<std::streamsize>(svg.size()));
  if (!output) {
    throw std::runtime_error("Failed while writing SVG: " + output_path);
  }
}

void run_graphics_script(const std::string& script_path,
                         const std::string& output_path) {
  logo_midi::TurtleGraphicsEngine graphics;
  const auto script = read_turtle_script(script_path);
  const auto execution =
      logo_midi::execute_turtle_script(script, graphics);
  const auto rendered = logo_midi::render_svg(graphics);
  write_svg(output_path, rendered.svg);

  std::cout << "Wrote SVG with " << execution.commands_executed
            << " executed command(s) to " << output_path << '\n';
  if (rendered.omitted_flood_fills > 0) {
    std::cerr << "logo-midi: warning: " << rendered.omitted_flood_fills
              << " flood-fill operation(s) were omitted; raster flood-fill "
                 "rendering is not implemented.\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && (std::string(argv[1]) == "--help" ||
                    std::string(argv[1]) == "-h")) {
    print_usage();
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << logo_midi::kProductName << " prototype by "
              << logo_midi::kManufacturer << '\n';
    return 0;
  }
  const bool graphics_mode =
      argc == 5 && std::string(argv[1]) == "--graphics" &&
      std::string(argv[3]) == "--output";
  const bool music_mode =
      argc == 4 && std::string(argv[2]) == "--output";
  if (!graphics_mode && !music_mode) {
    print_usage();
    return 2;
  }

  try {
    if (graphics_mode) {
      run_graphics_script(argv[2], argv[4]);
      return 0;
    }
    logo_midi::MusicEngine engine;
    run_script(argv[1], engine);
    engine.write_midi_file(argv[3]);
    std::cout << "Wrote " << engine.events().size() << " MIDI events across "
              << engine.beat_position() << " beats at " << engine.tempo()
              << " BPM to " << argv[3] << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "logo-midi: " << error.what() << '\n';
    return 1;
  }
}
