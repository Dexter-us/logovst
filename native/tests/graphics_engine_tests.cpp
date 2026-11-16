#include "logo_midi/graphics_engine.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <variant>

int main() {
  using namespace logo_midi;

  TurtleGraphicsEngine graphics;
  graphics.forward(10.0);
  assert(std::abs(graphics.state().position.x) < 1e-9);
  assert(std::abs(graphics.state().position.y - 10.0) < 1e-9);
  assert(graphics.canvas().strokes.size() == 1);
  assert(graphics.canvas().strokes[0].from.y == 0.0);
  assert(graphics.canvas().strokes[0].to.y == 10.0);

  graphics.right(90.0);
  graphics.forward(5.0);
  assert(std::abs(graphics.state().position.x - 5.0) < 1e-9);
  assert(std::abs(graphics.state().position.y - 10.0) < 1e-9);
  assert(graphics.canvas().strokes.size() == 2);

  graphics.set_x(8.0);
  graphics.set_y(-2.0);
  assert(graphics.state().position.x == 8.0);
  assert(graphics.state().position.y == -2.0);
  assert(graphics.canvas().strokes.size() == 4);

  graphics.pen_up();
  graphics.back(3.0);
  assert(graphics.canvas().strokes.size() == 4);
  assert(graphics.state().position.x == 5.0);
  graphics.pen_down();

  graphics.set_heading(-90.0);
  assert(graphics.state().heading == 270.0);
  graphics.set_pen_color({220, 30, 40});
  graphics.set_pen_width(3.0);
  graphics.set_pen_mode(GraphicsPenMode::Reverse);
  graphics.forward(2.0);
  const auto& latest_stroke = graphics.canvas().strokes.back();
  assert(latest_stroke.color.red == 220);
  assert(latest_stroke.width == 3.0);
  assert(latest_stroke.mode == GraphicsPenMode::Reverse);

  graphics.set_background_color({12, 34, 56});
  assert(graphics.canvas().background.green == 34);
  const auto before_clean = graphics.state();
  graphics.clean();
  assert(graphics.canvas().strokes.empty());
  assert(graphics.state().position.x == before_clean.position.x);
  assert(graphics.state().position.y == before_clean.position.y);
  assert(graphics.state().heading == before_clean.heading);

  graphics.home();
  assert(graphics.state().position.x == 0.0);
  assert(graphics.state().position.y == 0.0);
  assert(graphics.state().heading == 0.0);
  graphics.forward(4.0);
  graphics.clear_screen();
  assert(graphics.canvas().strokes.empty());
  assert(graphics.canvas().commands.empty());
  assert(graphics.state().position.x == 0.0);
  assert(graphics.state().position.y == 0.0);

  bool rejected = false;
  TurtleGraphicsEngine wrapped;
  wrapped.set_viewport(100.0, 100.0);
  wrapped.set_position(40.0, 0.0);
  wrapped.right(90.0);
  wrapped.forward(20.0);
  assert(wrapped.state().position.x == -40.0);
  assert(wrapped.canvas().strokes.size() == 3);
  assert(wrapped.canvas().strokes[1].to.x == 50.0);
  assert(wrapped.canvas().strokes[2].from.x == -50.0);

  TurtleGraphicsEngine fenced;
  fenced.set_viewport(100.0, 100.0);
  fenced.set_boundary_mode(GraphicsBoundaryMode::Fence);
  rejected = false;
  try {
    fenced.set_position(51.0, 0.0);
  } catch (const std::out_of_range&) {
    rejected = true;
  }
  assert(rejected);
  assert(fenced.state().position.x == 0.0);

  fenced.set_boundary_mode(GraphicsBoundaryMode::Window);
  fenced.set_position(75.0, 0.0);
  assert(fenced.state().position.x == 75.0);

  TurtleGraphicsEngine drawing;
  drawing.start_polygon();
  drawing.forward(10.0);
  drawing.right(120.0);
  drawing.forward(10.0);
  drawing.right(120.0);
  drawing.forward(10.0);
  drawing.end_polygon();
  assert(std::holds_alternative<GraphicsFilledPolygon>(
      drawing.canvas().commands.back()));
  const auto& polygon =
      std::get<GraphicsFilledPolygon>(drawing.canvas().commands.back());
  assert(polygon.vertices.size() == 3);

  drawing.set_flood_color({10, 20, 30});
  drawing.fill(true);
  assert(std::holds_alternative<GraphicsFloodFill>(
      drawing.canvas().commands.back()));
  const auto& flood = std::get<GraphicsFloodFill>(drawing.canvas().commands.back());
  assert(flood.fill_until_pen_color);
  assert(flood.fill_color.green == 20);

  drawing.set_label_font("Mono");
  drawing.label("Logo");
  assert(std::holds_alternative<GraphicsLabel>(
      drawing.canvas().commands.back()));
  const auto& label = std::get<GraphicsLabel>(drawing.canvas().commands.back());
  assert(label.text == "Logo");
  assert(label.font_face == "Mono");
  assert(label.heading == drawing.state().heading);

  TurtleGraphicsEngine ellipse;
  ellipse.set_position(10.0, 20.0);
  ellipse.right(30.0);
  const auto saved_position = ellipse.state().position;
  const auto saved_heading = ellipse.state().heading;
  ellipse.ellipse_arc(90.0, 10.0, 20.0, 0.0);
  assert(!ellipse.canvas().strokes.empty());
  assert(ellipse.state().position.x == saved_position.x);
  assert(ellipse.state().position.y == saved_position.y);
  assert(ellipse.state().heading == saved_heading);

  TurtleGraphicsEngine pixel;
  pixel.set_background_color({255, 255, 255});
  assert(pixel.pixel_at_turtle().red == 255);
  pixel.set_pixel({40, 50, 60});
  assert(pixel.pixel_at_turtle().green == 50);
  pixel.set_x(1.0);
  assert(pixel.pixel_at_turtle().red == 255);

  rejected = false;
  try {
    graphics.set_pen_width(0.0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    graphics.forward(std::numeric_limits<double>::infinity());
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  return 0;
}
