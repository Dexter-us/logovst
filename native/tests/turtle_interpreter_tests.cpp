#include "logo_midi/turtle_interpreter.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

using logo_midi::TurtleGraphicsEngine;

namespace {

void expect_script_error(const std::string& script,
                         TurtleGraphicsEngine& graphics) {
  const auto old_position = graphics.state().position;
  const auto old_command_count = graphics.canvas().commands.size();
  bool threw = false;
  try {
    logo_midi::execute_turtle_script(script, graphics);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  assert(graphics.canvas().commands.size() == old_command_count);
  assert(std::abs(graphics.state().position.x - old_position.x) < 1e-9);
  assert(std::abs(graphics.state().position.y - old_position.y) < 1e-9);
}

}  // namespace

int main() {
  TurtleGraphicsEngine graphics;
  const auto result = logo_midi::execute_turtle_script(
      "SETSCREENCOLOR [12 18 33]\n"
      "SETPC [246 190 82]\n"
      "PENSIZE 3\n"
      "FD 20\n"
      "RT 90\n"
      "FD 10\n"
      "LABEL \"hello; turtle\"\n",
      graphics);

  assert(result.commands_executed == 7);
  assert(std::abs(graphics.state().position.x - 10.0) < 1e-9);
  assert(std::abs(graphics.state().position.y - 20.0) < 1e-9);
  assert(graphics.canvas().background.red == 12);
  assert(graphics.canvas().commands.size() == 3);
  assert(std::get<logo_midi::GraphicsLabel>(graphics.canvas().commands.back())
             .text == "hello; turtle");

  const auto pixel = logo_midi::execute_turtle_script("PIXEL\n", graphics);
  assert(pixel.output.find("PIXEL [12 18 33]") == 0);
  assert(pixel.output.find("approximate") != std::string::npos);

  expect_script_error("FD 20\nUNKNOWN\n", graphics);

  TurtleGraphicsEngine nested_graphics;
  const auto nested = logo_midi::execute_turtle_script(
      "REPEAT 2 [ REPEAT 2 [ FD 5 ] ]\n", nested_graphics);
  assert(nested.commands_executed == 7);
  assert(std::abs(nested_graphics.state().position.y - 20.0) < 1e-9);
  assert(nested_graphics.canvas().commands.size() == 4);

  TurtleGraphicsEngine multiline_loop_graphics;
  const auto multiline_loops = logo_midi::execute_turtle_script(
      "REPEAT 2 [\n"
      "  REPEAT 2 [\n"
      "    FD 1\n"
      "    RT 90\n"
      "  ]\n"
      "]\n",
      multiline_loop_graphics);
  assert(multiline_loops.commands_executed == 11);
  assert(multiline_loop_graphics.canvas().commands.size() == 4);
  assert(std::abs(multiline_loop_graphics.state().position.x) < 1e-9);
  assert(std::abs(multiline_loop_graphics.state().position.y) < 1e-9);

  TurtleGraphicsEngine procedure_graphics;
  const auto procedures = logo_midi::execute_turtle_script(
      "SQUARE\n"
      "TO SQUARE\n"
      "REPEAT 4 [ FD 10 RT 90 ]\n"
      "END\n"
      "square\n",
      procedure_graphics);
  assert(procedures.commands_executed == 20);
  assert(procedure_graphics.canvas().commands.size() == 8);
  assert(std::abs(procedure_graphics.state().position.x) < 1e-9);
  assert(std::abs(procedure_graphics.state().position.y) < 1e-9);

  TurtleGraphicsEngine parameter_graphics;
  logo_midi::execute_turtle_script(
      "TO STEP :DISTANCE\n"
      "  FD :DISTANCE\n"
      "END\n"
      "TO REPEATSTEP :SIZE :COUNT\n"
      "  REPEAT :COUNT [ STEP :SIZE ]\n"
      "END\n"
      "REPEATSTEP 5 3\n",
      parameter_graphics);
  assert(std::abs(parameter_graphics.state().position.y - 15.0) < 1e-9);
  assert(parameter_graphics.canvas().strokes.size() == 3);

  TurtleGraphicsEngine color_parameter_graphics;
  logo_midi::execute_turtle_script(
      "TO COLORLINE :DISTANCE :RED :GREEN :BLUE\n"
      "  SETPC [:RED :GREEN :BLUE]\n"
      "  FD :DISTANCE\n"
      "END\n"
      "COLORLINE 12 46 190 210\n",
      color_parameter_graphics);
  assert(color_parameter_graphics.canvas().strokes.size() == 1);
  const auto parameter_color =
      color_parameter_graphics.canvas().strokes.front().color;
  assert(parameter_color.red == 46);
  assert(parameter_color.green == 190);
  assert(parameter_color.blue == 210);

  TurtleGraphicsEngine conditional_graphics;
  logo_midi::execute_turtle_script(
      "IF 2 > 1 [\n"
      "  FD 10\n"
      "  IFELSE 1 = 2 [ FD 99 ] [ RT 90 ]\n"
      "]\n"
      "IFELSE FALSE [ FD 100 ] [ FD 5 ]\n",
      conditional_graphics);
  assert(conditional_graphics.canvas().strokes.size() == 2);
  assert(std::abs(conditional_graphics.state().position.x - 5.0) < 1e-9);
  assert(std::abs(conditional_graphics.state().position.y - 10.0) < 1e-9);

  TurtleGraphicsEngine parameter_condition_graphics;
  logo_midi::execute_turtle_script(
      "TO DRAWIF :SIZE\n"
      "  IF :SIZE >= 10 [ FD :SIZE ]\n"
      "END\n"
      "DRAWIF 12\n",
      parameter_condition_graphics);
  assert(parameter_condition_graphics.canvas().strokes.size() == 1);
  assert(std::abs(parameter_condition_graphics.state().position.y - 12.0) <
         1e-9);

  TurtleGraphicsEngine malformed_graphics;
  const std::vector<std::string> malformed_scripts = {
      "REPEAT 2 [\nFD 5\n",
      "REPEAT 2 FD 5\n",
      "REPEAT 1 [ FD 5 ] ]\n",
      "TO DRAW\nFD 5\n",
      "END\n",
      "TO FIRST\nTO SECOND\nEND\nEND\n",
      "REPEAT 2 [\nTO INSIDE\nEND\n]\n",
      "REPEAT 1.5 [ FD 1 ]\n",
      "TO 1SHAPE\nEND\n",
      "TO SHAPE\nEND\nTO SHAPE\nEND\n",
      "TO BAD SIZE\nEND\n",
      "TO BAD :SIZE :size\nEND\n",
      "IF 1 < [ FD 1 ]\n",
      "IF 1 = 1 FD 1\n",
      "IFELSE TRUE [ FD 1 ]\n",
      "IF 1 = 1 [ FD 1\n"};
  for (const auto& malformed : malformed_scripts) {
    expect_script_error(malformed, malformed_graphics);
  }

  expect_script_error("TO MOVE :DISTANCE\nFD :DISTANCE\nEND\nMOVE\n",
                      malformed_graphics);
  expect_script_error("TO MOVE :DISTANCE\nFD :DISTANCE\nEND\nMOVE 1 2\n",
                      malformed_graphics);
  expect_script_error("TO MOVE\nFD :DISTANCE\nEND\nMOVE\n",
                      malformed_graphics);
  try {
    logo_midi::execute_turtle_script(
        "FD 4\nIF 1 = [ FD 5 ]\n", malformed_graphics);
    assert(false);
  } catch (const std::runtime_error& error) {
    assert(std::string(error.what()).find("Line 2:") == 0);
  }

  expect_script_error("REPEAT 4097 [ FD 1 ]\n", malformed_graphics);
  expect_script_error("REPEAT 4096 [ FD 1 ]\n", malformed_graphics);
  expect_script_error("TO LOOP\nLOOP\nEND\nLOOP\n", malformed_graphics);
  std::string too_many_procedures;
  for (int index = 0; index < 257; ++index) {
    too_many_procedures += "TO P" + std::to_string(index) + "\nEND\n";
  }
  expect_script_error(too_many_procedures, malformed_graphics);

  return 0;
}
