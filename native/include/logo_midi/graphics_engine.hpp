#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace logo_midi {

struct GraphicsPoint {
  double x = 0.0;
  double y = 0.0;
};

struct GraphicsColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
};

enum class GraphicsPenMode {
  Paint,
  Erase,
  Reverse,
};

struct GraphicsStroke {
  GraphicsPoint from;
  GraphicsPoint to;
  GraphicsColor color;
  double width = 1.0;
  GraphicsPenMode mode = GraphicsPenMode::Paint;
};

struct GraphicsFilledPolygon {
  std::vector<GraphicsPoint> vertices;
  GraphicsColor color;
};

struct GraphicsFloodFill {
  GraphicsPoint seed;
  GraphicsColor fill_color;
  GraphicsColor border_color;
  bool fill_until_pen_color = false;
};

struct GraphicsLabel {
  GraphicsPoint position;
  std::string text;
  std::string font_face;
  double heading = 0.0;
  double font_size = 12.0;
  GraphicsColor color;
};

struct GraphicsPixel {
  GraphicsPoint position;
  GraphicsColor color;
};

using GraphicsCommand =
    std::variant<GraphicsStroke, GraphicsFilledPolygon, GraphicsFloodFill,
                 GraphicsLabel, GraphicsPixel>;

struct TurtleGraphicsState {
  GraphicsPoint position;
  double heading = 0.0;
  GraphicsColor pen_color{0, 0, 0};
  double pen_width = 1.0;
  GraphicsPenMode pen_mode = GraphicsPenMode::Paint;
  bool pen_down = true;
  bool visible = true;
};

struct GraphicsCanvas {
  GraphicsColor background{255, 255, 255};
  std::vector<GraphicsStroke> strokes;
  // Ordered drawing operations preserve the order needed by a VST editor.
  std::vector<GraphicsCommand> commands;
};

enum class GraphicsBoundaryMode {
  Wrap,
  Fence,
  Window,
};

// Framework-neutral subset of FMSLogo's turtle drawing state. The VST editor
// can render this scene without linking wxWidgets into a DAW process.
class TurtleGraphicsEngine final {
 public:
  const TurtleGraphicsState& state() const noexcept { return state_; }
  const GraphicsCanvas& canvas() const noexcept { return canvas_; }
  GraphicsBoundaryMode boundary_mode() const noexcept { return boundary_mode_; }
  double viewport_width() const noexcept { return viewport_width_; }
  double viewport_height() const noexcept { return viewport_height_; }
  const std::string& label_font_face() const noexcept { return label_font_face_; }
  double label_font_size() const noexcept { return label_font_size_; }

  void forward(double distance);
  void back(double distance);
  void left(double degrees);
  void right(double degrees);
  void home();
  void set_heading(double degrees);
  void set_position(double x, double y);
  void set_x(double x);
  void set_y(double y);

  void set_boundary_mode(GraphicsBoundaryMode mode) noexcept {
    boundary_mode_ = mode;
  }
  void set_viewport(double width, double height);

  void pen_up() noexcept { state_.pen_down = false; }
  void pen_down() noexcept { state_.pen_down = true; }
  void show_turtle() noexcept { state_.visible = true; }
  void hide_turtle() noexcept { state_.visible = false; }
  void set_pen_color(GraphicsColor color) noexcept { state_.pen_color = color; }
  void set_pen_width(double width);
  void set_pen_mode(GraphicsPenMode mode) noexcept { state_.pen_mode = mode; }
  void set_background_color(GraphicsColor color) noexcept {
    canvas_.background = color;
  }
  void set_flood_color(GraphicsColor color) noexcept { flood_color_ = color; }
  void set_label_font(std::string font_face) {
    label_font_face_ = std::move(font_face);
  }
  void set_label_font_size(double size);

  void start_polygon();
  void end_polygon();
  void fill(bool fill_until_pen_color = false);
  void label(std::string text);
  void ellipse_arc(double sweep_degrees, double radius_x, double radius_y,
                   double start_angle_degrees);
  void set_pixel(GraphicsColor color);
  // Until a raster renderer is connected, this reads explicit SETPIXEL writes
  // and the background; it does not sample antialiased strokes or text.
  GraphicsColor pixel_at_turtle() const noexcept;

  // CLEAN erases the drawing but preserves turtle position and heading.
  // CLEARSCREEN also homes the turtle and resets its heading.
  void clean() noexcept;
  void clear_screen() noexcept;

 private:
  void move_to(GraphicsPoint destination);
  void add_stroke(GraphicsPoint from, GraphicsPoint to);
  GraphicsPoint wrap_point(GraphicsPoint point) const noexcept;

  TurtleGraphicsState state_;
  GraphicsCanvas canvas_;
  GraphicsBoundaryMode boundary_mode_ = GraphicsBoundaryMode::Wrap;
  double viewport_width_ = 500.0;
  double viewport_height_ = 500.0;
  GraphicsColor flood_color_{0, 0, 0};
  std::string label_font_face_;
  double label_font_size_ = 12.0;
  bool polygon_active_ = false;
  std::vector<GraphicsPoint> polygon_vertices_;
};

}  // namespace logo_midi
