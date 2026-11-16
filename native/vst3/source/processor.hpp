#pragma once

#include "logo_midi/music_engine.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"

namespace LogoMidi {

class Processor final : public Steinberg::Vst::AudioEffect {
 public:
  Processor();

  static Steinberg::FUnknown* createInstance(void*);
  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context)
      SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setupProcessing(
      Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API process(
      Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;

 private:
  void buildPattern(int pattern);
  void emitEvent(Steinberg::Vst::IEventList* output,
                 const logo_midi::MidiEvent& source, Steinberg::int32 offset);

  logo_midi::MusicEngine engine_;
  double cycle_beats_ = 4.0;
  double sample_rate_ = 44100.0;
  double internal_tempo_ = 120.0;
  Steinberg::int32 pattern_ = 0;
  bool sync_to_daw_ = false;
  Steinberg::int64 fallback_samples_ = 0;
};

}  // namespace LogoMidi
