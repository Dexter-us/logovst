#pragma once

#include "logo_midi/graphics_engine.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace logo_midi {

struct TurtleScriptResult {
  std::size_t commands_executed = 0;
  std::string output;
};

// Executes the supported Logo turtle subset, including bounded multiline
// REPEAT blocks and parameterless TO/END procedures. A failed script leaves
// the caller's graphics state unchanged.
TurtleScriptResult execute_turtle_script(std::string_view script,
                                         TurtleGraphicsEngine& graphics);

}  // namespace logo_midi
