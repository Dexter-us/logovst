#pragma once

#include "logo_midi/graphics_engine.hpp"
#include "vstgui/lib/ctexteditor.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/controls/icontrollistener.h"

#include <string>

namespace VSTGUI {
class CControl;
class CMultiLineTextLabel;
class CTextButton;
class CView;
struct ITextEditor;
}  // namespace VSTGUI

namespace LogoMidi {

class GraphicsCanvasView;

class LogoWorkspaceView final : public VSTGUI::CViewContainer,
                                public VSTGUI::IControlListener,
                                public VSTGUI::TextEditorControllerAdapter {
 public:
  explicit LogoWorkspaceView(const VSTGUI::CRect& size);
  ~LogoWorkspaceView() noexcept override;

  void valueChanged(VSTGUI::CControl* control) override;
  void onTextEditorCreated(const VSTGUI::ITextEditor& editor) override;
  void onTextEditorDestroyed(const VSTGUI::ITextEditor& editor) override;
  void onTextEditorTextChanged(const VSTGUI::ITextEditor&) override {}

 private:
  void run_program();
  void update_canvas();

  logo_midi::TurtleGraphicsEngine graphics_;
  const VSTGUI::ITextEditor* text_editor_ = nullptr;
  GraphicsCanvasView* canvas_view_ = nullptr;
  VSTGUI::CMultiLineTextLabel* status_label_ = nullptr;
  VSTGUI::CTextButton* run_button_ = nullptr;
};

}  // namespace LogoMidi
