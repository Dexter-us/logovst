#pragma once

#include "logo_midi/music_engine.hpp"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

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
  Steinberg::tresult PLUGIN_API notify(
      Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

 private:
  struct ScriptUpdate {
    bool clear = false;
    std::shared_ptr<const logo_midi::MusicEngine> engine;
  };
  struct ScheduledEvent {
    Steinberg::int32 sample_offset = 0;
    const logo_midi::MidiEvent* event = nullptr;
    std::size_t order = 0;
  };

  void buildPattern(int pattern);
  Steinberg::tresult flushScheduledEvents(Steinberg::Vst::IEventList* output);
  void emitEvent(Steinberg::Vst::IEventList* output,
                 const logo_midi::MidiEvent& source, Steinberg::int32 offset);

  logo_midi::MusicEngine engine_;
  std::shared_ptr<const logo_midi::MusicEngine> active_script_;
  std::shared_ptr<const ScriptUpdate> pending_update_;
  double cycle_beats_ = 4.0;
  double sample_rate_ = 44100.0;
  std::atomic<double> internal_tempo_{120.0};
  Steinberg::int32 pattern_ = 0;
  bool sync_to_daw_ = false;
  Steinberg::int64 fallback_samples_ = 0;
  std::vector<ScheduledEvent> scheduled_events_;
};

}  // namespace LogoMidi
