#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"

namespace LogoMidi {

class Controller final : public Steinberg::Vst::EditControllerEx1,
                         public VSTGUI::VST3EditorDelegate {
 public:
  static Steinberg::FUnknown* createInstance(void*);
  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context)
      SMTG_OVERRIDE;
  Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name)
      SMTG_OVERRIDE;

  VSTGUI::CView* createCustomView(
      VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes& attributes,
      const VSTGUI::IUIDescription* description,
      VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;
};

}  // namespace LogoMidi
