#include "processor.hpp"

#include "ids.hpp"

#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

#include <algorithm>
#include <cmath>

namespace LogoMidi {
namespace {

double normalized_to_tempo(double normalized) {
  return 30.0 + std::clamp(normalized, 0.0, 1.0) * 270.0;
}

}  // namespace

Processor::Processor() {
  setControllerClass(kControllerUid);
  processContextRequirements.flags =
      Steinberg::Vst::IProcessContextRequirements::kNeedProjectTimeMusic |
      Steinberg::Vst::IProcessContextRequirements::kNeedTempo |
      Steinberg::Vst::IProcessContextRequirements::kNeedTransportState;
  buildPattern(0);
}

Steinberg::FUnknown* Processor::createInstance(void*) {
  return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor);
}

Steinberg::tresult PLUGIN_API Processor::initialize(
    Steinberg::FUnknown* context) {
  const auto result = AudioEffect::initialize(context);
  if (result != Steinberg::kResultOk) {
    return result;
  }
  addEventOutput(STR16("MIDI Out"), kMidiChannelCount);
  return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Processor::setupProcessing(
    Steinberg::Vst::ProcessSetup& setup) {
  const auto result = AudioEffect::setupProcessing(setup);
  if (result == Steinberg::kResultOk && setup.sampleRate > 0.0) {
    sample_rate_ = setup.sampleRate;
  }
  return result;
}

void Processor::buildPattern(int pattern) {
  pattern_ = std::clamp(pattern, 0, 2);
  engine_ = logo_midi::MusicEngine{};
  engine_.set_velocity(96);
  engine_.set_gate(0.85);
  engine_.set_channel(1);
  engine_.set_tempo(internal_tempo_);

  if (pattern_ == 0) {
    constexpr int scale[] = {60, 62, 64, 65, 67, 69, 71, 72};
    for (const int pitch : scale) {
      engine_.note(pitch, 0.5);
    }
  } else if (pattern_ == 1) {
    constexpr int scale[] = {60, 63, 65, 67, 70, 72, 70, 67};
    for (const int pitch : scale) {
      engine_.note(pitch, 0.5);
    }
  } else {
    constexpr int chords[][3] = {
        {60, 64, 67}, {65, 69, 72}, {67, 71, 74}, {60, 64, 67}};
    for (const auto& chord : chords) {
      engine_.chord(chord, 3, 1.0);
    }
  }
  cycle_beats_ = engine_.beat_position();
}

void Processor::emitEvent(Steinberg::Vst::IEventList* output,
                          const logo_midi::MidiEvent& source,
                          Steinberg::int32 offset) {
  using namespace Steinberg;
  using namespace Steinberg::Vst;
  const auto family = source.status & 0xf0;
  const auto channel = static_cast<int16>(source.status & 0x0f);

  Event event{};
  event.busIndex = 0;
  event.sampleOffset = offset;
  event.flags = 0;
  if (family == 0x90 && source.data2 > 0) {
    event.type = Event::kNoteOnEvent;
    event.noteOn.channel = channel;
    event.noteOn.pitch = source.data1;
    event.noteOn.tuning = 0.f;
    event.noteOn.velocity = static_cast<float>(source.data2) / 127.f;
    event.noteOn.length = 0;
    event.noteOn.noteId = -1;
  } else if (family == 0x80 || (family == 0x90 && source.data2 == 0)) {
    event.type = Event::kNoteOffEvent;
    event.noteOff.channel = channel;
    event.noteOff.pitch = source.data1;
    event.noteOff.velocity = 0.f;
    event.noteOff.noteId = -1;
    event.noteOff.tuning = 0.f;
  } else if (family == 0xb0 || family == 0xc0) {
    event.type = Event::kLegacyMIDICCOutEvent;
    event.midiCCOut.channel = static_cast<int8>(channel);
    event.midiCCOut.controlNumber =
        family == 0xc0 ? kCtrlProgramChange : source.data1;
    event.midiCCOut.value =
        static_cast<int8>(family == 0xc0 ? source.data1 : source.data2);
    event.midiCCOut.value2 = 0;
  } else {
    return;
  }
  output->addEvent(event);
}

Steinberg::tresult PLUGIN_API Processor::process(
    Steinberg::Vst::ProcessData& data) {
  using namespace Steinberg::Vst;
  if (data.inputParameterChanges != nullptr) {
    const Steinberg::int32 count =
        data.inputParameterChanges->getParameterCount();
    int selected_pattern = pattern_;
    for (Steinberg::int32 i = 0; i < count; ++i) {
      auto* queue = data.inputParameterChanges->getParameterData(i);
      if (queue == nullptr || queue->getPointCount() == 0) {
        continue;
      }
      Steinberg::int32 sample_offset = 0;
      ParamValue value = 0.0;
      if (queue->getPoint(queue->getPointCount() - 1, sample_offset, value) !=
          Steinberg::kResultTrue) {
        continue;
      }
      switch (queue->getParameterId()) {
        case kPatternId:
          selected_pattern =
              std::clamp(static_cast<int>(std::lround(value * 2.0)), 0, 2);
          break;
        case kDawTempoSyncId:
          sync_to_daw_ = value >= 0.5;
          break;
        case kInternalTempoId:
          internal_tempo_ = normalized_to_tempo(value);
          break;
      }
    }
    if (selected_pattern != pattern_) {
      buildPattern(selected_pattern);
    }
  }

  if (data.outputEvents == nullptr || data.numSamples <= 0) {
    return Steinberg::kResultOk;
  }

  double start_beat = 0.0;
  double beats_per_sample = 0.0;
  const auto* context = data.processContext;
  if (context != nullptr &&
      (context->state & ProcessContext::kPlaying) == 0) {
    return Steinberg::kResultOk;
  }
  if (sync_to_daw_ && context != nullptr &&
      (context->state & ProcessContext::kPlaying) != 0 &&
      (context->state & ProcessContext::kProjectTimeMusicValid) != 0 &&
      (context->state & ProcessContext::kTempoValid) != 0 &&
      context->tempo > 0.0) {
    const double sample_rate =
        context->sampleRate > 0.0 ? context->sampleRate : sample_rate_;
    start_beat = context->projectTimeMusic;
    beats_per_sample = context->tempo / (60.0 * sample_rate);
  } else if (!sync_to_daw_) {
    const double sample_rate =
        context != nullptr && context->sampleRate > 0.0
            ? context->sampleRate
            : sample_rate_;
    const auto absolute_sample =
        context != nullptr ? context->projectTimeSamples : fallback_samples_;
    start_beat = static_cast<double>(absolute_sample) / sample_rate *
                 internal_tempo_ / 60.0;
    beats_per_sample = internal_tempo_ / (60.0 * sample_rate);
  } else {
    return Steinberg::kResultOk;
  }

  if (context == nullptr ||
      (context->state & ProcessContext::kProjectTimeMusicValid) == 0) {
    fallback_samples_ += data.numSamples;
  }
  if (beats_per_sample <= 0.0 || cycle_beats_ <= 0.0) {
    return Steinberg::kResultOk;
  }

  const double end_beat = start_beat + data.numSamples * beats_per_sample;
  const auto first_cycle = static_cast<Steinberg::int64>(
      std::max(0.0, std::floor(start_beat / cycle_beats_)));
  const auto last_cycle = static_cast<Steinberg::int64>(
      std::max(0.0, std::floor(end_beat / cycle_beats_)));

  for (Steinberg::int64 cycle = first_cycle; cycle <= last_cycle; ++cycle) {
    const double cycle_start = static_cast<double>(cycle) * cycle_beats_;
    for (const auto& midi_event : engine_.events()) {
      const double event_beat = cycle_start + midi_event.beat;
      if (event_beat < start_beat || event_beat >= end_beat) {
        continue;
      }
      const auto offset = static_cast<Steinberg::int32>(
          std::floor((event_beat - start_beat) / beats_per_sample));
      if (offset >= 0 && offset < data.numSamples) {
        emitEvent(data.outputEvents, midi_event, offset);
      }
    }
  }
  return Steinberg::kResultOk;
}

}  // namespace LogoMidi
