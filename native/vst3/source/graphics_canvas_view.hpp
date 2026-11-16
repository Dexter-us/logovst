#pragma once

#include "logo_midi/graphics_engine.hpp"
#include "vstgui/lib/cview.h"

namespace LogoMidi {

class GraphicsCanvasView final : public VSTGUI::CView {
 public:
  GraphicsCanvasView(const VSTGUI::CRect& size, logo_midi::GraphicsCanvas canvas,
                     logo_midi::TurtleGraphicsState turtle,
                     double world_width, double world_height);

  void set_scene(logo_midi::GraphicsCanvas canvas,
                 logo_midi::TurtleGraphicsState turtle, double world_width,
                 double world_height);
  void draw(VSTGUI::CDrawContext* context) override;

 private:
  VSTGUI::CPoint to_view(logo_midi::GraphicsPoint point,
                         double scale) const;
  void draw_turtle(VSTGUI::CDrawContext* context, double scale) const;

  logo_midi::GraphicsCanvas canvas_;
  logo_midi::TurtleGraphicsState turtle_;
  double world_width_;
  double world_height_;
};

}  // namespace LogoMidi
