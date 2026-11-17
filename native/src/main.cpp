#include "logo_midi/graphics_engine.hpp"
#include "logo_midi/music_engine.hpp"
#include "logo_midi/music_script.hpp"
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
#include <iterator>
#include <utility>

namespace {

void print_usage() {
  std::cout
      << logo_midi::kProductName << " command-line prototype by "
      << logo_midi::kManufacturer << '\n'
      << "Usage:\n"
      << "  logo-midi <music-script.lgo> --output <file.mid>\n"
      << "  logo-midi --graphics <turtle-script.lgo> --output <drawing.svg>\n"
      << "      [--size <width>x<height>]\n\n"
      << "Supported music commands (one command per line):\n"
      << "  TEMPO 1..1000 | WAIT ticks (60 = 1 second)\n"
      << "  VELOCITY 1..127 | CHANNEL 1..16 | GATE 0..1\n"
      << "  NOTE pitch beats | CHORD [pitch ...] beats | REST beats\n"
      << "  STEAL beats (extend last note) | ANTICIPATE beats (advance next)\n"
      << "  CC controller value | PROGRAM 0..127\n\n"
      << "  MAKE \"NAME expression | RERANDOM [seed]\n"
      << "Music scripts also support word/list values, reporter procedures,\n"
      << "REPEAT, IF/IFELSE, WHILE, UNTIL, and bounded FOREVER blocks.\n"
      << "Use MAKE \"NAME [60 64 67], then CHORD :NAME 1.\n"
      << "Example:\n"
      << "  MAKE \"PITCH SUM 60 RANDOM 12\n"
      << "  NOTE :PITCH 0.5\n\n"
      << "Graphics mode runs turtle commands, REPEAT, IF/IFELSE, and\n"
      << "parameterized TO/END procedures, then writes a standalone SVG.\n"
      << "The default SVG canvas is 500x500; each custom dimension may be\n"
      << "1..16384.\n"
      << "Logo query results, such as PIXEL, are printed to standard output.\n";
}

logo_midi::MusicEngine run_script(const std::string& script_path) {
  constexpr std::size_t kMaximumScriptBytes = 64 * 1024;
  std::ifstream script(script_path);
  if (!script) {
    throw std::runtime_error("Could not open script: " + script_path);
  }
  std::string contents((std::istreambuf_iterator<char>(script)),
                       std::istreambuf_iterator<char>());
  if (contents.size() > kMaximumScriptBytes) {
    throw std::runtime_error("Script exceeds the 64 KiB size limit.");
  }
  return logo_midi::compile_music_script(contents);
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

std::pair<double, double> parse_canvas_size(const std::string& text) {
  const auto separator = text.find_first_of("xX");
  if (separator == std::string::npos ||
      text.find_first_of("xX", separator + 1) != std::string::npos) {
    throw std::runtime_error("Canvas size must use WIDTHxHEIGHT, e.g. 800x600.");
  }

  const auto parse_dimension = [](const std::string& dimension) {
    std::size_t consumed = 0;
    double value = 0.0;
    try {
      value = std::stod(dimension, &consumed);
    } catch (const std::exception&) {
      throw std::runtime_error(
          "Canvas size must use numeric WIDTHxHEIGHT dimensions.");
    }
    if (consumed != dimension.size() || !std::isfinite(value)) {
      throw std::runtime_error(
          "Canvas size must use numeric WIDTHxHEIGHT dimensions.");
    }
    return value;
  };
  const double width = parse_dimension(text.substr(0, separator));
  const double height = parse_dimension(text.substr(separator + 1));
  constexpr double kMinimumCanvasDimension = 1.0;
  constexpr double kMaximumCanvasDimension = 16384.0;
  if (width < kMinimumCanvasDimension || width > kMaximumCanvasDimension ||
      height < kMinimumCanvasDimension ||
      height > kMaximumCanvasDimension) {
    throw std::runtime_error(
        "Canvas dimensions must each be between 1 and 16384.");
  }
  return {width, height};
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
                         const std::string& output_path, double width,
                         double height) {
  logo_midi::TurtleGraphicsEngine graphics;
  graphics.set_viewport(width, height);
  const auto script = read_turtle_script(script_path);
  const auto execution =
      logo_midi::execute_turtle_script(script, graphics);
  const auto rendered = logo_midi::render_svg(graphics);
  write_svg(output_path, rendered.svg);

  std::cout << "Wrote SVG with " << execution.commands_executed
            << " executed command(s) to " << output_path << '\n';
  if (!execution.output.empty()) {
    std::cout << execution.output << '\n';
  }
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
      (argc == 5 || argc == 7) && std::string(argv[1]) == "--graphics" &&
      std::string(argv[3]) == "--output" &&
      (argc == 5 || std::string(argv[5]) == "--size");
  const bool music_mode =
      argc == 4 && std::string(argv[2]) == "--output";
  if (!graphics_mode && !music_mode) {
    print_usage();
    return 2;
  }

  try {
    if (graphics_mode) {
      double width = 500.0;
      double height = 500.0;
      if (argc == 7) {
        const auto size = parse_canvas_size(argv[6]);
        width = size.first;
        height = size.second;
      }
      run_graphics_script(argv[2], argv[4], width, height);
      return 0;
    }
    auto engine = run_script(argv[1]);
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
