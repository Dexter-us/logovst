#include "logo_midi/svg_renderer.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace logo_midi {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTurtleRadius = 8.0;

std::string number(double value) {
  std::ostringstream output;
  output << std::setprecision(12) << value;
  return output.str();
}

std::string color_hex(GraphicsColor color) {
  std::ostringstream output;
  output << '#' << std::hex << std::setfill('0') << std::setw(2)
         << static_cast<int>(color.red) << std::setw(2)
         << static_cast<int>(color.green) << std::setw(2)
         << static_cast<int>(color.blue);
  return output.str();
}

std::string xml_escape(std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (const unsigned char character : text) {
    switch (character) {
      case '&':
        escaped += "&amp;";
        break;
      case '<':
        escaped += "&lt;";
        break;
      case '>':
        escaped += "&gt;";
        break;
      case '"':
        escaped += "&quot;";
        break;
      case '\'':
        escaped += "&apos;";
        break;
      default:
        // XML 1.0 excludes control bytes other than tab and line endings.
        if (character >= 0x20 || character == '\t' || character == '\n' ||
            character == '\r') {
          escaped.push_back(static_cast<char>(character));
        }
        break;
    }
  }
  return escaped;
}

GraphicsColor rendered_color(const GraphicsStroke& stroke,
                             GraphicsColor background) {
  if (stroke.mode == GraphicsPenMode::Erase) {
    return background;
  }
  if (stroke.mode == GraphicsPenMode::Reverse) {
    return {static_cast<std::uint8_t>(255 - stroke.color.red),
            static_cast<std::uint8_t>(255 - stroke.color.green),
            static_cast<std::uint8_t>(255 - stroke.color.blue)};
  }
  return stroke.color;
}

double svg_x(GraphicsPoint point, double width) {
  return width / 2.0 + point.x;
}

double svg_y(GraphicsPoint point, double height) {
  return height / 2.0 - point.y;
}

void append_turtle(std::ostringstream& output,
                   const TurtleGraphicsState& turtle, double width,
                   double height) {
  if (!turtle.visible) {
    return;
  }

  const double angle = turtle.heading * kPi / 180.0;
  const GraphicsPoint center{svg_x(turtle.position, width),
                             svg_y(turtle.position, height)};
  const GraphicsPoint forward{std::sin(angle) * kTurtleRadius,
                              -std::cos(angle) * kTurtleRadius};
  const GraphicsPoint side{std::cos(angle) * kTurtleRadius * 0.65,
                           std::sin(angle) * kTurtleRadius * 0.65};
  const GraphicsPoint tip{center.x + forward.x, center.y + forward.y};
  const GraphicsPoint rear_left{center.x - forward.x * 0.55 + side.x,
                                center.y - forward.y * 0.55 + side.y};
  const GraphicsPoint rear_right{center.x - forward.x * 0.55 - side.x,
                                 center.y - forward.y * 0.55 - side.y};
  output << "<polygon points=\"" << number(tip.x) << ',' << number(tip.y)
         << ' ' << number(rear_left.x) << ',' << number(rear_left.y) << ' '
         << number(rear_right.x) << ',' << number(rear_right.y)
         << "\" fill=\"#f6be52\" stroke=\"#0c1221\" stroke-width=\"1\"/>\n";
}

}  // namespace

SvgRenderResult render_svg(const TurtleGraphicsEngine& graphics) {
  const double width = graphics.viewport_width();
  const double height = graphics.viewport_height();
  const auto& canvas = graphics.canvas();
  SvgRenderResult result;
  std::ostringstream output;
  output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
         << number(width) << "\" height=\"" << number(height)
         << "\" viewBox=\"0 0 " << number(width) << ' ' << number(height)
         << "\">\n"
         << "<rect width=\"100%\" height=\"100%\" fill=\""
         << color_hex(canvas.background) << "\"/>\n";

  for (const auto& command : canvas.commands) {
    if (const auto* stroke = std::get_if<GraphicsStroke>(&command)) {
      output << "<line x1=\"" << number(svg_x(stroke->from, width))
             << "\" y1=\"" << number(svg_y(stroke->from, height))
             << "\" x2=\"" << number(svg_x(stroke->to, width))
             << "\" y2=\"" << number(svg_y(stroke->to, height))
             << "\" stroke=\""
             << color_hex(rendered_color(*stroke, canvas.background))
             << "\" stroke-width=\"" << number(stroke->width)
             << "\" stroke-linecap=\"round\"/>\n";
    } else if (const auto* polygon =
                   std::get_if<GraphicsFilledPolygon>(&command)) {
      if (polygon->vertices.size() < 3) {
        continue;
      }
      output << "<polygon points=\"";
      bool first = true;
      for (const auto vertex : polygon->vertices) {
        if (!first) {
          output << ' ';
        }
        first = false;
        output << number(svg_x(vertex, width)) << ','
               << number(svg_y(vertex, height));
      }
      output << "\" fill=\"" << color_hex(polygon->color) << "\"/>\n";
    } else if (const auto* label =
                   std::get_if<GraphicsLabel>(&command)) {
      const double x = svg_x(label->position, width);
      const double y = svg_y(label->position, height);
      output << "<text x=\"" << number(x) << "\" y=\"" << number(y)
             << "\" fill=\"" << color_hex(label->color)
             << "\" font-size=\"" << number(label->font_size)
             << "\" font-family=\""
             << xml_escape(label->font_face.empty() ? "sans-serif"
                                                    : label->font_face)
             << "\"";
      if (label->heading != 0.0) {
        output << " transform=\"rotate(" << number(label->heading) << ' '
               << number(x) << ' ' << number(y) << ")\"";
      }
      output << '>' << xml_escape(label->text) << "</text>\n";
    } else if (const auto* pixel = std::get_if<GraphicsPixel>(&command)) {
      output << "<rect x=\"" << number(svg_x(pixel->position, width))
             << "\" y=\"" << number(svg_y(pixel->position, height))
             << "\" width=\"1\" height=\"1\" fill=\""
             << color_hex(pixel->color) << "\"/>\n";
    } else if (std::holds_alternative<GraphicsFloodFill>(command)) {
      ++result.omitted_flood_fills;
    }
  }

  append_turtle(output, graphics.state(), width, height);
  if (result.omitted_flood_fills > 0) {
    output << "<!-- Omitted " << result.omitted_flood_fills
           << " flood-fill operation(s): raster flood-fill rendering is not "
              "implemented. -->\n";
  }
  output << "</svg>\n";
  result.svg = output.str();
  return result;
}

}  // namespace logo_midi
