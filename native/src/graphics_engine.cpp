#include "logo_midi/graphics_engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace logo_midi {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kCoordinateEpsilon = 1e-10;
constexpr std::size_t kMaximumWrapSegments = 10000;
constexpr std::size_t kMaximumArcSegments = 10000;

void require_finite(double value, const char* label) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(label) + " must be finite.");
  }
}

double normalized_heading(double degrees) {
  require_finite(degrees, "Heading");
  double normalized = std::fmod(degrees, 360.0);
  if (normalized < 0.0) {
    normalized += 360.0;
  }
  return normalized;
}

bool same_point(GraphicsPoint left, GraphicsPoint right) noexcept {
  return left.x == right.x && left.y == right.y;
}

double ellipse_parameter(double radius_x, double radius_y,
                         double circle_angle) {
  if (radius_x == 0.0 || radius_y == 0.0) {
    return circle_angle;
  }
  return std::atan2(std::abs(radius_y) * std::sin(circle_angle),
                    std::abs(radius_x) * std::cos(circle_angle));
}

}  // namespace

void TurtleGraphicsEngine::forward(double distance) {
  require_finite(distance, "Distance");
  const double radians = state_.heading * kPi / 180.0;
  move_to({state_.position.x + std::sin(radians) * distance,
           state_.position.y + std::cos(radians) * distance});
}

void TurtleGraphicsEngine::back(double distance) {
  require_finite(distance, "Distance");
  forward(-distance);
}

void TurtleGraphicsEngine::left(double degrees) {
  require_finite(degrees, "Turn angle");
  set_heading(state_.heading - degrees);
}

void TurtleGraphicsEngine::right(double degrees) {
  require_finite(degrees, "Turn angle");
  set_heading(state_.heading + degrees);
}

void TurtleGraphicsEngine::home() {
  set_position(0.0, 0.0);
  state_.heading = 0.0;
}

void TurtleGraphicsEngine::set_heading(double degrees) {
  state_.heading = normalized_heading(degrees);
}

void TurtleGraphicsEngine::set_position(double x, double y) {
  require_finite(x, "X coordinate");
  require_finite(y, "Y coordinate");
  move_to({x, y});
}

void TurtleGraphicsEngine::set_x(double x) {
  set_position(x, state_.position.y);
}

void TurtleGraphicsEngine::set_y(double y) {
  set_position(state_.position.x, y);
}

void TurtleGraphicsEngine::set_viewport(double width, double height) {
  require_finite(width, "Viewport width");
  require_finite(height, "Viewport height");
  if (width <= 0.0 || height <= 0.0) {
    throw std::invalid_argument("Viewport dimensions must be greater than zero.");
  }
  viewport_width_ = width;
  viewport_height_ = height;
}

void TurtleGraphicsEngine::set_pen_width(double width) {
  require_finite(width, "Pen width");
  if (width <= 0.0) {
    throw std::invalid_argument("Pen width must be greater than zero.");
  }
  state_.pen_width = width;
}

void TurtleGraphicsEngine::set_label_font_size(double size) {
  require_finite(size, "Label font size");
  if (size <= 0.0) {
    throw std::invalid_argument("Label font size must be greater than zero.");
  }
  label_font_size_ = size;
}

void TurtleGraphicsEngine::start_polygon() {
  if (polygon_active_) {
    throw std::logic_error("A polygon is already being collected.");
  }
  polygon_active_ = true;
  polygon_vertices_.clear();
}

void TurtleGraphicsEngine::end_polygon() {
  if (!polygon_active_) {
    throw std::logic_error("POLYEND requires POLYSTART.");
  }
  polygon_active_ = false;
  if (polygon_vertices_.size() < 3) {
    polygon_vertices_.clear();
    throw std::logic_error("A filled polygon requires at least three vertices.");
  }
  canvas_.commands.emplace_back(
      GraphicsFilledPolygon{std::move(polygon_vertices_), state_.pen_color});
  polygon_vertices_.clear();
}

void TurtleGraphicsEngine::fill(bool fill_until_pen_color) {
  canvas_.commands.emplace_back(GraphicsFloodFill{
      state_.position, flood_color_, state_.pen_color, fill_until_pen_color});
}

void TurtleGraphicsEngine::label(std::string text) {
  canvas_.commands.emplace_back(GraphicsLabel{
      state_.position, std::move(text), label_font_face_, state_.heading,
      label_font_size_, state_.pen_color});
}

void TurtleGraphicsEngine::ellipse_arc(double sweep_degrees, double radius_x,
                                       double radius_y,
                                       double start_angle_degrees) {
  require_finite(sweep_degrees, "Ellipse arc angle");
  require_finite(radius_x, "Ellipse X radius");
  require_finite(radius_y, "Ellipse Y radius");
  require_finite(start_angle_degrees, "Ellipse start angle");

  if (std::abs(sweep_degrees) > 360.0) {
    sweep_degrees = std::copysign(360.0, sweep_degrees);
  }

  const double start_radians = start_angle_degrees * kPi / 180.0;
  const double sweep_radians = sweep_degrees * kPi / 180.0;
  const double t_start = ellipse_parameter(radius_x, radius_y, start_radians);
  const double t_end =
      ellipse_parameter(radius_x, radius_y, start_radians + sweep_radians);
  double t_sweep = t_end - t_start;
  if (sweep_radians > 0.0 && t_end <= t_start) {
    t_sweep += 2.0 * kPi;
  } else if (sweep_radians < 0.0 && t_start <= t_end) {
    t_sweep -= 2.0 * kPi;
  }

  const double maximum_radius = std::max(std::abs(radius_x), std::abs(radius_y));
  const double segment_estimate =
      std::abs(sweep_degrees) * maximum_radius / 200.0;
  const auto segments = static_cast<std::size_t>(std::clamp(
      std::floor(segment_estimate), 1.0,
      static_cast<double>(kMaximumArcSegments)));
  const double t_delta = t_sweep / static_cast<double>(segments);
  const double heading = state_.heading * kPi / 180.0;
  const double cos_heading = std::cos(heading);
  const double sin_heading = std::sin(heading);
  const GraphicsPoint center = state_.position;
  const GraphicsPoint original_position = state_.position;

  auto point_at = [&](double parameter) {
    const double x = -std::sin(parameter) * radius_x;
    const double y = -std::cos(parameter) * radius_y;
    return GraphicsPoint{
        center.x + x * cos_heading + y * sin_heading,
        center.y + y * cos_heading - x * sin_heading};
  };

  state_.position = point_at(t_start);
  try {
    for (std::size_t i = 1; i <= segments; ++i) {
      move_to(point_at(t_start + t_delta * static_cast<double>(i)));
    }
  } catch (...) {
    state_.position = original_position;
    throw;
  }
  state_.position = original_position;
}

void TurtleGraphicsEngine::set_pixel(GraphicsColor color) {
  canvas_.commands.emplace_back(GraphicsPixel{state_.position, color});
}

GraphicsColor TurtleGraphicsEngine::pixel_at_turtle() const noexcept {
  for (auto command = canvas_.commands.rbegin();
       command != canvas_.commands.rend(); ++command) {
    const auto* pixel = std::get_if<GraphicsPixel>(&*command);
    if (pixel != nullptr && same_point(pixel->position, state_.position)) {
      return pixel->color;
    }
  }
  return canvas_.background;
}

void TurtleGraphicsEngine::clean() noexcept {
  canvas_.strokes.clear();
  canvas_.commands.clear();
  polygon_active_ = false;
  polygon_vertices_.clear();
}

void TurtleGraphicsEngine::clear_screen() noexcept {
  clean();
  state_.position = {0.0, 0.0};
  state_.heading = 0.0;
}

GraphicsPoint TurtleGraphicsEngine::wrap_point(GraphicsPoint point) const noexcept {
  const double half_width = viewport_width_ / 2.0;
  const double half_height = viewport_height_ / 2.0;
  auto wrap_coordinate = [](double value, double half_extent) {
    if (value >= -half_extent && value <= half_extent) {
      return value;
    }
    const double span = half_extent * 2.0;
    double wrapped = std::fmod(value + half_extent, span);
    if (wrapped < 0.0) {
      wrapped += span;
    }
    return wrapped - half_extent;
  };
  point.x = wrap_coordinate(point.x, half_width);
  point.y = wrap_coordinate(point.y, half_height);
  return point;
}

void TurtleGraphicsEngine::add_stroke(GraphicsPoint from, GraphicsPoint to) {
  if (same_point(from, to)) {
    return;
  }
  GraphicsStroke stroke{from, to, state_.pen_color, state_.pen_width,
                        state_.pen_mode};
  canvas_.strokes.push_back(stroke);
  canvas_.commands.emplace_back(stroke);
}

void TurtleGraphicsEngine::move_to(GraphicsPoint destination) {
  require_finite(destination.x, "X coordinate");
  require_finite(destination.y, "Y coordinate");

  if (boundary_mode_ == GraphicsBoundaryMode::Window) {
    if (state_.pen_down) {
      add_stroke(state_.position, destination);
    }
    state_.position = destination;
  } else if (boundary_mode_ == GraphicsBoundaryMode::Fence) {
    const double half_width = viewport_width_ / 2.0;
    const double half_height = viewport_height_ / 2.0;
    if (destination.x < -half_width || destination.x > half_width ||
        destination.y < -half_height || destination.y > half_height) {
      throw std::out_of_range("Turtle movement crossed the FENCE boundary.");
    }
    if (state_.pen_down) {
      add_stroke(state_.position, destination);
    }
    state_.position = destination;
  } else {
    GraphicsPoint current = wrap_point(state_.position);
    double remaining_x = destination.x - state_.position.x;
    double remaining_y = destination.y - state_.position.y;
    if (!state_.pen_down) {
      state_.position = wrap_point(destination);
    } else {
      const double half_width = viewport_width_ / 2.0;
      const double half_height = viewport_height_ / 2.0;
      std::size_t steps = 0;
      while (std::abs(remaining_x) > kCoordinateEpsilon ||
             std::abs(remaining_y) > kCoordinateEpsilon) {
        if (++steps > kMaximumWrapSegments) {
          throw std::overflow_error(
              "Turtle movement crosses too many WRAP boundaries.");
        }
        double crossing = 1.0;
        double cross_x = std::numeric_limits<double>::infinity();
        double cross_y = std::numeric_limits<double>::infinity();
        if (remaining_x > 0.0 && current.x + remaining_x > half_width) {
          cross_x = (half_width - current.x) / remaining_x;
        } else if (remaining_x < 0.0 &&
                   current.x + remaining_x < -half_width) {
          cross_x = (-half_width - current.x) / remaining_x;
        }
        if (remaining_y > 0.0 && current.y + remaining_y > half_height) {
          cross_y = (half_height - current.y) / remaining_y;
        } else if (remaining_y < 0.0 &&
                   current.y + remaining_y < -half_height) {
          cross_y = (-half_height - current.y) / remaining_y;
        }
        crossing = std::min({1.0, cross_x, cross_y});

        const GraphicsPoint end{current.x + remaining_x * crossing,
                                current.y + remaining_y * crossing};
        add_stroke(current, end);
        if (crossing >= 1.0) {
          current = end;
          break;
        }

        const double untraveled = 1.0 - crossing;
        remaining_x *= untraveled;
        remaining_y *= untraveled;
        current = end;
        if (cross_x <= crossing + kCoordinateEpsilon) {
          current.x = remaining_x >= 0.0 ? -half_width : half_width;
        }
        if (cross_y <= crossing + kCoordinateEpsilon) {
          current.y = remaining_y >= 0.0 ? -half_height : half_height;
        }
      }
      state_.position = wrap_point(destination);
    }
  }

  if (polygon_active_ && state_.pen_down) {
    polygon_vertices_.push_back(state_.position);
  }
}

}  // namespace logo_midi
