#include "logo_midi/svg_renderer.hpp"

#include <cassert>
#include <string>

int main() {
  logo_midi::TurtleGraphicsEngine graphics;
  graphics.set_viewport(200.0, 100.0);
  graphics.set_background_color({10, 20, 30});
  graphics.set_pen_color({255, 0, 0});
  graphics.forward(20.0);
  graphics.right(90.0);
  graphics.forward(10.0);
  graphics.set_label_font("Arial & Sans");
  graphics.set_label_font_size(16.0);
  graphics.label("Logo & <graphics>");
  graphics.set_pixel({1, 2, 3});
  graphics.fill();

  const auto rendered = logo_midi::render_svg(graphics);
  assert(rendered.omitted_flood_fills == 1);
  assert(rendered.svg.find("width=\"200\" height=\"100\"") !=
         std::string::npos);
  assert(rendered.svg.find("fill=\"#0a141e\"") != std::string::npos);
  assert(rendered.svg.find(
             "<line x1=\"100\" y1=\"50\" x2=\"100\" y2=\"30\" "
             "stroke=\"#ff0000\"") != std::string::npos);
  assert(rendered.svg.find("Logo &amp; &lt;graphics&gt;") !=
         std::string::npos);
  assert(rendered.svg.find("Arial &amp; Sans") != std::string::npos);
  assert(rendered.svg.find("rotate(90 110 30)") != std::string::npos);
  assert(rendered.svg.find("Omitted 1 flood-fill operation") !=
         std::string::npos);
  assert(rendered.svg.find("</svg>") != std::string::npos);
  return 0;
}
