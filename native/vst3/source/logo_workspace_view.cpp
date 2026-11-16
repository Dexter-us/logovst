#include "logo_workspace_view.hpp"

#include "graphics_canvas_view.hpp"
#include "logo_midi/turtle_interpreter.hpp"

#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/ctexteditor.h"

#include <exception>
#include <string>

namespace LogoMidi {
namespace {

constexpr int32_t kRunButtonTag = 1;
constexpr double kCanvasWidth = 500.0;
constexpr double kCanvasHeight = 300.0;
constexpr char kStarterProgram[] =
    "; Edit these Logo commands and press Run\n"
    "TO SQUARE\n"
    "  REPEAT 4 [\n"
    "    FD 90\n"
    "    RT 90\n"
    "  ]\n"
    "END\n"
    "CS\n"
    "SETPC [46 190 210]\n"
    "PENSIZE 3\n"
    "SQUARE\n"
    "LABEL \"Hello Logo\"";

}  // namespace

LogoWorkspaceView::LogoWorkspaceView(const VSTGUI::CRect& size)
    : CViewContainer(size) {
  setBackgroundColor({16, 24, 39, 255});
  setTransparency(false);
  graphics_.set_viewport(kCanvasWidth, kCanvasHeight);
  graphics_.set_background_color({12, 18, 33});

  const double width = size.getWidth();
  const double height = size.getHeight();
  const double run_left = width - 104.0;
  auto* editor_view = VSTGUI::createNewTextEditor(
      {12.0, 10.0, run_left - 12.0, 104.0}, this);
  if (editor_view != nullptr) {
    addView(editor_view);
  }

  run_button_ = new VSTGUI::CTextButton(
      {run_left, 10.0, width - 12.0, 46.0}, this, kRunButtonTag, "Run");
  run_button_->setTextColor({246, 190, 82, 255});
  run_button_->setFrameColor({49, 68, 94, 255});
  run_button_->setRoundRadius(5.0);
  addView(run_button_);

  status_label_ = new VSTGUI::CMultiLineTextLabel(
      {run_left, 60.0, width - 12.0, 104.0});
  status_label_->setBackColor({16, 24, 39, 255});
  status_label_->setFontColor({230, 237, 248, 255});
  status_label_->setFont(VSTGUI::kNormalFontSmall);
  status_label_->setText("Edit the sample, then press Run.");
  addView(status_label_);

  canvas_view_ = new GraphicsCanvasView(
      {12.0, 116.0, width - 12.0, height - 12.0}, graphics_.canvas(),
      graphics_.state(), graphics_.viewport_width(),
      graphics_.viewport_height());
  addView(canvas_view_);
}

LogoWorkspaceView::~LogoWorkspaceView() noexcept {
  if (text_editor_ != nullptr) {
    text_editor_->resetController();
  }
}

void LogoWorkspaceView::onTextEditorCreated(
    const VSTGUI::ITextEditor& editor) {
  text_editor_ = &editor;
  VSTGUI::ITextEditor::Style style;
  style.backColor = {16, 24, 39, 255};
  style.textColor = {230, 237, 248, 255};
  style.cursorColor = {246, 190, 82, 255};
  style.selectionBackColor = {49, 68, 94, 255};
  style.frameColor = {49, 68, 94, 255};
  style.lineNumberTextColor = {132, 150, 177, 200};
  style.lineNumberTextSelectedColor = {246, 190, 82, 255};
  editor.setStyle(style);
  editor.setPlainText(kStarterProgram, true);
}

void LogoWorkspaceView::onTextEditorDestroyed(
    const VSTGUI::ITextEditor& editor) {
  if (text_editor_ == &editor) {
    text_editor_ = nullptr;
  }
}

void LogoWorkspaceView::valueChanged(VSTGUI::CControl* control) {
  if (control == run_button_) {
    run_program();
  }
}

void LogoWorkspaceView::run_program() {
  if (text_editor_ == nullptr || status_label_ == nullptr) {
    return;
  }

  auto next_graphics = graphics_;
  try {
    const auto result = logo_midi::execute_turtle_script(
        text_editor_->getPlainText(), next_graphics);
    graphics_ = std::move(next_graphics);
    update_canvas();
    std::string status =
        "Ran " + std::to_string(result.commands_executed) + " command(s).";
    if (!result.output.empty()) {
      status += "\n" + result.output;
    }
    status_label_->setText(status.c_str());
  } catch (const std::exception& error) {
    status_label_->setText(error.what());
  }
}

void LogoWorkspaceView::update_canvas() {
  if (canvas_view_ == nullptr) {
    return;
  }
  canvas_view_->set_scene(graphics_.canvas(), graphics_.state(),
                          graphics_.viewport_width(),
                          graphics_.viewport_height());
}

}  // namespace LogoMidi
