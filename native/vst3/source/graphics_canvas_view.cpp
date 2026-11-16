#include "graphics_canvas_view.hpp"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cpoint.h"
#include "vstgui/lib/crect.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace LogoMidi {
namespace {

VSTGUI::CColor to_vst_color(logo_midi::GraphicsColor color) {
  return {color.red, color.green, color.blue, 255};
}

logo_midi::GraphicsColor rendered_color(const logo_midi::GraphicsStroke& stroke,
                                       logo_midi::GraphicsColor background) {
  if (stroke.mode == logo_midi::GraphicsPenMode::Erase) {
    return background;
  }
  if (stroke.mode == logo_midi::GraphicsPenMode::Reverse) {
    return {static_cast<std::uint8_t>(255 - stroke.color.red),
            static_cast<std::uint8_t>(255 - stroke.color.green),
            static_cast<std::uint8_t>(255 - stroke.color.blue)};
  }
  return stroke.color;
}

}  // namespace

GraphicsCanvasView::GraphicsCanvasView(
    const VSTGUI::CRect& size, logo_midi::GraphicsCanvas canvas,
    logo_midi::TurtleGraphicsState turtle, double world_width,
    double world_height)
    : CView(size),
      canvas_(std::move(canvas)),
      turtle_(turtle),
      world_width_(world_width),
      world_height_(world_height) {
  setMouseEnabled(false);
}

void GraphicsCanvasView::set_scene(logo_midi::GraphicsCanvas canvas,
                                   logo_midi::TurtleGraphicsState turtle,
                                   double world_width, double world_height) {
  canvas_ = std::move(canvas);
  turtle_ = turtle;
  world_width_ = world_width;
  world_height_ = world_height;
  invalid();
}

VSTGUI::CPoint GraphicsCanvasView::to_view(logo_midi::GraphicsPoint point,
                                           double scale) const {
  const auto rect = getViewSize();
  return {rect.left + rect.getWidth() / 2.0 + point.x * scale,
          rect.top + rect.getHeight() / 2.0 - point.y * scale};
}

void GraphicsCanvasView::draw(VSTGUI::CDrawContext* context) {
  if (context == nullptr || world_width_ <= 0.0 || world_height_ <= 0.0) {
    return;
  }

  const auto rect = getViewSize();
  context->setFillColor(to_vst_color(canvas_.background));
  context->drawRect(rect, VSTGUI::kDrawFilled);

  const double scale =
      std::min(rect.getWidth() / world_width_, rect.getHeight() / world_height_);
  const double line_scale = std::max(0.5, scale);

  for (const auto& command : canvas_.commands) {
    if (const auto* stroke = std::get_if<logo_midi::GraphicsStroke>(&command)) {
      const auto color = rendered_color(*stroke, canvas_.background);
      context->setFrameColor(to_vst_color(color));
      context->setLineWidth(std::max(0.75, stroke->width * line_scale));
      context->drawLine(to_view(stroke->from, scale), to_view(stroke->to, scale));
    } else if (const auto* polygon =
                   std::get_if<logo_midi::GraphicsFilledPolygon>(&command)) {
      VSTGUI::PointList points;
      points.reserve(polygon->vertices.size());
      for (const auto vertex : polygon->vertices) {
        points.push_back(to_view(vertex, scale));
      }
      if (points.size() >= 3) {
        context->setFillColor(to_vst_color(polygon->color));
        context->setFrameColor(to_vst_color(polygon->color));
        context->drawPolygon(points, VSTGUI::kDrawFilled);
      }
    } else if (const auto* label =
                   std::get_if<logo_midi::GraphicsLabel>(&command)) {
      context->setFontColor(to_vst_color(label->color));
      context->setFont(VSTGUI::kSystemFont, label->font_size);
      context->drawString(label->text.c_str(), to_view(label->position, scale));
    } else if (const auto* pixel =
                   std::get_if<logo_midi::GraphicsPixel>(&command)) {
      const auto point = to_view(pixel->position, scale);
      context->setFillColor(to_vst_color(pixel->color));
      context->drawRect(
          VSTGUI::CRect(point.x, point.y, point.x + 1.0, point.y + 1.0),
          VSTGUI::kDrawFilled);
    }
    // Flood fill remains a retained command until a raster canvas is added.
  }

  draw_turtle(context, scale);
}

void GraphicsCanvasView::draw_turtle(VSTGUI::CDrawContext* context,
                                     double scale) const {
  if (!turtle_.visible) {
    return;
  }

  constexpr double kPi = 3.14159265358979323846;
  const double angle = turtle_.heading * kPi / 180.0;
  const auto center = to_view(turtle_.position, scale);
  const double radius = 8.0;
  const auto forward = VSTGUI::CPoint(std::sin(angle) * radius,
                                      -std::cos(angle) * radius);
  const auto side = VSTGUI::CPoint(std::cos(angle) * radius * 0.65,
                                   std::sin(angle) * radius * 0.65);
  VSTGUI::PointList points{
      center + forward,
      center - forward * 0.55 + side,
      center - forward * 0.55 - side,
  };
  context->setFillColor({246, 190, 82, 255});
  context->setFrameColor({12, 18, 33, 255});
  context->setLineWidth(1.0);
  context->drawPolygon(points, VSTGUI::kDrawFilledAndStroked);
}

}  // namespace LogoMidi
