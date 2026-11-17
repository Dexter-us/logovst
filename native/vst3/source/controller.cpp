#include "controller.hpp"

#include "ids.hpp"
#include "logo_workspace_view.hpp"

#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/vstparameters.h"

#include <cstring>
#include <limits>

namespace LogoMidi {

Steinberg::FUnknown* Controller::createInstance(void*) {
  return static_cast<Steinberg::Vst::IEditController*>(new Controller);
}

Steinberg::tresult PLUGIN_API Controller::initialize(
    Steinberg::FUnknown* context) {
  const auto result = EditControllerEx1::initialize(context);
  if (result != Steinberg::kResultOk) {
    return result;
  }

  using namespace Steinberg::Vst;
  auto* pattern = new StringListParameter(STR16("Pattern"), kPatternId);
  Steinberg::UString128 pattern_name;
  pattern_name.fromAscii("Major scale");
  pattern->appendString(pattern_name);
  pattern_name.fromAscii("Minor scale");
  pattern->appendString(pattern_name);
  pattern_name.fromAscii("Major chords");
  pattern->appendString(pattern_name);
  parameters.addParameter(pattern);

  auto* sync = new StringListParameter(STR16("Tempo Source"), kDawTempoSyncId);
  Steinberg::UString128 sync_name;
  sync_name.fromAscii("Internal");
  sync->appendString(sync_name);
  sync_name.fromAscii("DAW sync");
  sync->appendString(sync_name);
  parameters.addParameter(sync);

  parameters.addParameter(new RangeParameter(
      STR16("Internal Tempo"), kInternalTempoId, STR16("BPM"), 30.0, 300.0,
      120.0, 270, ParameterInfo::kCanAutomate));
  return Steinberg::kResultOk;
}

Steinberg::IPlugView* PLUGIN_API Controller::createView(
    Steinberg::FIDString name) {
  if (name != nullptr &&
      std::strcmp(name, Steinberg::Vst::ViewType::kEditor) == 0) {
    return new VSTGUI::VST3Editor(this, "LogoMidiEditor", "logo-midi.uidesc");
  }
  return nullptr;
}

VSTGUI::CView* Controller::createCustomView(
    VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes&,
    const VSTGUI::IUIDescription*, VSTGUI::VST3Editor*) {
  if (name != nullptr && std::strcmp(name, "LogoWorkspace") == 0) {
    return new LogoWorkspaceView(VSTGUI::CRect(0.0, 0.0, 832.0, 400.0), this);
  }
  return nullptr;
}

Steinberg::tresult Controller::sendMusicScript(const std::string& script) {
  using namespace Steinberg;
  if (script.size() > std::numeric_limits<uint32>::max()) {
    return kResultFalse;
  }
  auto* message = allocateMessage();
  if (message == nullptr) {
    return kResultFalse;
  }
  message->setMessageID("LogoMidiMusicScript");
  const auto attribute_result = message->getAttributes()->setBinary(
      "script", script.data(), static_cast<uint32>(script.size()));
  if (attribute_result != kResultOk) {
    message->release();
    return attribute_result;
  }
  const auto result = sendMessage(message);
  message->release();
  return result;
}

Steinberg::tresult Controller::clearMusicScript() {
  auto* message = allocateMessage();
  if (message == nullptr) {
    return Steinberg::kResultFalse;
  }
  message->setMessageID("LogoMidiClearMusicScript");
  const auto result = sendMessage(message);
  message->release();
  return result;
}

}  // namespace LogoMidi
