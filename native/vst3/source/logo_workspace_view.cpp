#include "logo_workspace_view.hpp"

#include "controller.hpp"
#include "graphics_canvas_view.hpp"
#include "logo_midi/music_script.hpp"
#include "logo_midi/turtle_interpreter.hpp"

#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/ctexteditor.h"
#include "vstgui/lib/vstguibase.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace LogoMidi {
namespace {

constexpr int32_t kRunButtonTag = 1;
constexpr int32_t kLoadButtonTag = 2;
constexpr int32_t kSaveButtonTag = 3;
constexpr std::size_t kMaximumScriptBytes = 64 * 1024;
constexpr double kCanvasWidth = 500.0;
constexpr double kCanvasHeight = 300.0;
constexpr char kStarterProgram[] =
    "; Edit this script, then press Run\n"
    "TO SQUARE :SIZE\n"
    "  IFELSE :SIZE > 60 [ SETPC [46 190 210] ] "
    "[ SETPC [246 190 82] ]\n"
    "  REPEAT 4 [ FD :SIZE RT 90 ]\n"
    "END\n"
    "CS\n"
    "PENSIZE 3\n"
    "SQUARE 90\n"
    "LABEL \"Hello Logo\"";

}  // namespace

LogoWorkspaceView::LogoWorkspaceView(const VSTGUI::CRect& size,
                                     Controller* controller)
    : CViewContainer(size), controller_(controller) {
  setBackgroundColor({16, 24, 39, 255});
  setTransparency(false);
  graphics_.set_viewport(kCanvasWidth, kCanvasHeight);
  graphics_.set_background_color({12, 18, 33});

  const double width = size.getWidth();
  const double height = size.getHeight();
  const double run_left = width - 104.0;
  auto* editor_view = VSTGUI::createNewTextEditor(
      {12.0, 10.0, run_left - 12.0, 146.0}, this);
  if (editor_view != nullptr) {
    addView(editor_view);
  }

  run_button_ = new VSTGUI::CTextButton(
      {run_left, 10.0, width - 12.0, 44.0}, this, kRunButtonTag, "Run");
  run_button_->setTextColor({246, 190, 82, 255});
  run_button_->setFrameColor({49, 68, 94, 255});
  run_button_->setRoundRadius(5.0);
  addView(run_button_);

  load_button_ = new VSTGUI::CTextButton(
      {run_left, 50.0, width - 12.0, 84.0}, this, kLoadButtonTag, "Load");
  load_button_->setTextColor({230, 237, 248, 255});
  load_button_->setFrameColor({49, 68, 94, 255});
  load_button_->setRoundRadius(5.0);
  addView(load_button_);

  save_button_ = new VSTGUI::CTextButton(
      {run_left, 90.0, width - 12.0, 124.0}, this, kSaveButtonTag, "Save");
  save_button_->setTextColor({230, 237, 248, 255});
  save_button_->setFrameColor({49, 68, 94, 255});
  save_button_->setRoundRadius(5.0);
  addView(save_button_);

  status_label_ = new VSTGUI::CMultiLineTextLabel(
      {run_left, 132.0, width - 12.0, height - 12.0});
  status_label_->setBackColor({16, 24, 39, 255});
  status_label_->setFontColor({230, 237, 248, 255});
  status_label_->setFont(VSTGUI::kNormalFontSmall);
  status_label_->setText("Edit the sample, then press Run.");
  addView(status_label_);

  canvas_view_ = new GraphicsCanvasView(
      {12.0, 158.0, run_left - 12.0, height - 12.0}, graphics_.canvas(),
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
  // CTextButton's default kick style reports both activation and reset.
  // Handle only the activation so Load does not open two modal dialogs.
  if (control == nullptr || control->getValue() != control->getMax()) {
    return;
  }

  if (control == run_button_) {
    run_program();
  } else if (control == load_button_) {
    select_script_file(false);
  } else if (control == save_button_) {
    select_script_file(true);
  }
}

void LogoWorkspaceView::select_script_file(bool saving) {
  if (status_label_ == nullptr) {
    return;
  }

  const auto style = saving ? VSTGUI::CNewFileSelector::kSelectSaveFile
                            : VSTGUI::CNewFileSelector::kSelectFile;
  auto selector =
      VSTGUI::owned(VSTGUI::CNewFileSelector::create(getFrame(), style));
  if (!selector) {
    status_label_->setText("File dialogs are unavailable.");
    return;
  }

  const VSTGUI::CFileExtension script_extension(
      "Logo scripts", "lgo", "text/plain");
  selector->addFileExtension(script_extension);
  selector->setDefaultExtension(script_extension);
  selector->setTitle(saving ? "Save Logo script" : "Load Logo script");
  if (saving) {
    selector->setDefaultSaveName("drawing.lgo");
  }

  std::error_code filesystem_error;
  const auto working_directory =
      std::filesystem::current_path(filesystem_error);
  if (!filesystem_error) {
    for (const auto& relative_directory :
         {std::filesystem::path("examples"),
          std::filesystem::path("native") / "examples"}) {
      const auto examples_directory = working_directory / relative_directory;
      filesystem_error.clear();
      if (std::filesystem::is_directory(examples_directory,
                                        filesystem_error)) {
        selector->setInitialDirectory(
            VSTGUI::UTF8String(examples_directory.u8string()));
        break;
      }
    }
  }

  selector->runModal();
  if (selector->getNumSelectedFiles() == 0) {
    return;
  }

  const auto* selected_file = selector->getSelectedFile(0);
  if (selected_file == nullptr) {
    return;
  }

  try {
    const auto file_path = std::filesystem::u8path(selected_file);
    if (saving) {
      if (text_editor_ == nullptr) {
        throw std::runtime_error("The script editor is unavailable.");
      }
      const auto script = text_editor_->getPlainText();
      if (script.size() > kMaximumScriptBytes) {
        throw std::runtime_error("Script exceeds the 64 KiB size limit.");
      }
      std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
      if (!output) {
        throw std::runtime_error("Could not write the selected script file.");
      }
      output.write(script.data(), static_cast<std::streamsize>(script.size()));
      output.flush();
      if (!output) {
        throw std::runtime_error("Failed while saving the script file.");
      }
      output.close();
      if (output.fail()) {
        throw std::runtime_error("Failed while saving the script file.");
      }
      status_label_->setText("Script saved.");
    } else {
      if (text_editor_ == nullptr) {
        throw std::runtime_error("The script editor is unavailable.");
      }
      std::ifstream input(file_path, std::ios::binary);
      if (!input) {
        throw std::runtime_error("Could not open the selected script file.");
      }
      std::string script(kMaximumScriptBytes + 1, '\0');
      input.read(script.data(), static_cast<std::streamsize>(script.size()));
      const auto bytes_read = static_cast<std::size_t>(input.gcount());
      if (bytes_read > kMaximumScriptBytes) {
        throw std::runtime_error("Script exceeds the 64 KiB size limit.");
      }
      if (input.bad()) {
        throw std::runtime_error("Could not read the selected script file.");
      }
      script.resize(bytes_read);
      if (!text_editor_->setPlainText(script, true)) {
        throw std::runtime_error("Could not display the selected script.");
      }
      status_label_->setText("Script loaded.");
    }
  } catch (const std::exception& error) {
    status_label_->setText(error.what());
  }
}

void LogoWorkspaceView::run_program() {
  if (text_editor_ == nullptr || status_label_ == nullptr) {
    return;
  }

  const auto script = text_editor_->getPlainText();
  auto next_graphics = graphics_;
  try {
    const auto result = logo_midi::execute_turtle_script(
        script, next_graphics);
    graphics_ = std::move(next_graphics);
    if (controller_ != nullptr) {
      controller_->clearMusicScript();
    }
    update_canvas();
    std::string status =
        "Ran " + std::to_string(result.commands_executed) + " command(s).";
    if (!result.output.empty()) {
      status += "\n" + result.output;
    }
    status_label_->setText(status.c_str());
    return;
  } catch (const std::exception&) {
    // Music scripts share the editor and Run button but do not affect canvas state.
  }

  try {
    const auto music = logo_midi::compile_music_script(script);
    if (controller_ == nullptr) {
      throw std::runtime_error("The VST MIDI output is unavailable.");
    }
    if (controller_->sendMusicScript(script) != Steinberg::kResultOk) {
      throw std::runtime_error("Could not send the music script to the DAW.");
    }
    status_label_->setText(
        ("Sent " + std::to_string(music.events().size()) +
         " MIDI event(s) to the DAW. Start playback to hear the loop.")
            .c_str());
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
