#pragma once

#include "logo_midi/graphics_engine.hpp"

#include <cstddef>
#include <string>

namespace logo_midi {

struct SvgRenderResult {
  std::string svg;
  std::size_t omitted_flood_fills = 0;
};

// Exports the retained turtle scene as a standalone SVG document.
SvgRenderResult render_svg(const TurtleGraphicsEngine& graphics);

}  // namespace logo_midi
