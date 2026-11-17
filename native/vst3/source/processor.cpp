#include "processor.hpp"

#include "ids.hpp"
#include "logo_midi/music_script.hpp"

#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

namespace LogoMidi {
namespace {

double normalized_to_tempo(double normalized) {
  return 30.0 + std::clamp(normalized, 0.0, 1.0) * 270.0;
}

double beat_to_seconds(const logo_midi::MusicEngine& engine, double beat) {
  const auto& changes = engine.tempo_changes();
  if (changes.empty()) {
    return beat * 0.5;
  }

  double elapsed = 0.0;
  double previous_beat = changes.front().beat;
  double tempo = changes.front().beats_per_minute;
  for (std::size_t i = 1; i < changes.size(); ++i) {
    const auto& change = changes[i];
    if (beat <= change.beat) {
      break;
    }
    elapsed += (change.beat - previous_beat) * 60.0 / tempo;
    previous_beat = change.beat;
    tempo = change.beats_per_minute;
  }
  return elapsed + std::max(0.0, beat - previous_beat) * 60.0 / tempo;
}

}  // namespace

Processor::Processor() {
  setControllerClass(kControllerUid);
  processContextRequirements.flags =
      Steinberg::Vst::IProcessContextRequirements::kNeedProjectTimeMusic |
      Steinberg::Vst::IProcessContextRequirements::kNeedProjectTimeSamples |
      Steinberg::Vst::IProcessContextRequirements::kNeedTempo |
      Steinberg::Vst::IProcessContextRequirements::kNeedTransportState;
  scheduled_events_.reserve(32768);
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

Steinberg::tresult PLUGIN_API Processor::notify(
    Steinberg::Vst::IMessage* message) {
  if (message == nullptr || message->getMessageID() == nullptr) {
    return AudioEffect::notify(message);
  }
  const std::string message_id(message->getMessageID());
  if (message_id == "LogoMidiClearMusicScript") {
    auto update = std::make_shared<const ScriptUpdate>(
        ScriptUpdate{true, std::shared_ptr<const logo_midi::MusicEngine>{}});
    std::atomic_store_explicit(&pending_update_, std::move(update),
                               std::memory_order_release);
    return Steinberg::kResultOk;
  }
  if (message_id != "LogoMidiMusicScript") {
    return AudioEffect::notify(message);
  }

  const void* bytes = nullptr;
  Steinberg::uint32 byte_count = 0;
  if (message->getAttributes() == nullptr ||
      message->getAttributes()->getBinary("script", bytes, byte_count) !=
          Steinberg::kResultOk ||
      (bytes == nullptr && byte_count != 0)) {
    return Steinberg::kResultFalse;
  }

  try {
    const std::string script =
        byte_count == 0
            ? std::string{}
            : std::string(static_cast<const char*>(bytes), byte_count);
    auto compiled =
        std::make_shared<const logo_midi::MusicEngine>(
            logo_midi::compile_music_script(
                script, internal_tempo_.load(std::memory_order_relaxed)));
    auto update = std::make_shared<const ScriptUpdate>(
        ScriptUpdate{false, std::move(compiled)});
    std::atomic_store_explicit(&pending_update_, std::move(update),
                               std::memory_order_release);
    return Steinberg::kResultOk;
  } catch (...) {
    return Steinberg::kResultFalse;
  }
}

void Processor::buildPattern(int pattern) {
  pattern_ = std::clamp(pattern, 0, 2);
  engine_ = logo_midi::MusicEngine{};
  engine_.set_velocity(96);
  engine_.set_gate(0.85);
  engine_.set_channel(1);
  engine_.set_tempo(internal_tempo_.load(std::memory_order_relaxed));

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

Steinberg::tresult Processor::flushScheduledEvents(
    Steinberg::Vst::IEventList* output) {
  std::sort(scheduled_events_.begin(), scheduled_events_.end(),
            [](const ScheduledEvent& left, const ScheduledEvent& right) {
              if (left.sample_offset != right.sample_offset) {
                return left.sample_offset < right.sample_offset;
              }
              return left.order < right.order;
            });
  for (const auto& scheduled : scheduled_events_) {
    emitEvent(output, *scheduled.event, scheduled.sample_offset);
  }
  scheduled_events_.clear();
  return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Processor::process(
    Steinberg::Vst::ProcessData& data) {
  using namespace Steinberg::Vst;
  if (auto update = std::atomic_exchange_explicit(
          &pending_update_, std::shared_ptr<const ScriptUpdate>{},
          std::memory_order_acquire)) {
    if (update->clear) {
      active_script_.reset();
    } else {
      active_script_ = update->engine;
    }
  }
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
      active_script_.reset();
    }
  }

  if (data.outputEvents == nullptr || data.numSamples <= 0) {
    return Steinberg::kResultOk;
  }
  scheduled_events_.clear();

  const auto* context = data.processContext;
  if (context != nullptr &&
      (context->state & ProcessContext::kPlaying) == 0) {
    return Steinberg::kResultOk;
  }
  const auto& scheduled_engine = active_script_ ? *active_script_ : engine_;
  const double cycle_beats =
      active_script_ ? active_script_->beat_position() : cycle_beats_;
  if (cycle_beats <= 0.0) {
    return Steinberg::kResultOk;
  }

  const double sample_rate =
      context != nullptr && context->sampleRate > 0.0 ? context->sampleRate
                                                       : sample_rate_;
  if (sample_rate <= 0.0) {
    return Steinberg::kResultOk;
  }

  if (sync_to_daw_) {
    if (context == nullptr ||
        (context->state & ProcessContext::kProjectTimeMusicValid) == 0 ||
        (context->state & ProcessContext::kTempoValid) == 0 ||
        context->tempo <= 0.0) {
      return Steinberg::kResultOk;
    }
    const double start_beat = context->projectTimeMusic;
    const double beats_per_sample = context->tempo / (60.0 * sample_rate);
    const double end_beat = start_beat + data.numSamples * beats_per_sample;
    const auto first_cycle = static_cast<Steinberg::int64>(
        std::max(0.0, std::floor(start_beat / cycle_beats) - 1.0));
    const auto last_cycle = static_cast<Steinberg::int64>(
        std::max(0.0, std::floor(end_beat / cycle_beats)));

    std::size_t order = 0;
    for (Steinberg::int64 cycle = first_cycle; cycle <= last_cycle; ++cycle) {
      const double cycle_start = static_cast<double>(cycle) * cycle_beats;
      for (const auto& midi_event : scheduled_engine.events()) {
        const auto event_order = order++;
        const double event_beat = cycle_start + midi_event.beat;
        if (event_beat < start_beat || event_beat >= end_beat) {
          continue;
        }
        const auto offset = static_cast<Steinberg::int32>(
            std::floor((event_beat - start_beat) / beats_per_sample));
        if (offset >= 0 && offset < data.numSamples) {
          if (scheduled_events_.size() == scheduled_events_.capacity()) {
            scheduled_events_.clear();
            return Steinberg::kResultFalse;
          }
          scheduled_events_.push_back({offset, &midi_event, event_order});
        }
      }
    }
    return flushScheduledEvents(data.outputEvents);
  }

  const bool project_samples_valid =
      context != nullptr &&
      (context->state & ProcessContext::kProjectTimeSamplesValid) != 0;
  const auto absolute_sample =
      project_samples_valid ? context->projectTimeSamples : fallback_samples_;
  if (!project_samples_valid) {
    fallback_samples_ += data.numSamples;
  }
  const double start_seconds =
      static_cast<double>(absolute_sample) / sample_rate;
  const double end_seconds =
      start_seconds + static_cast<double>(data.numSamples) / sample_rate;
  const double cycle_seconds = beat_to_seconds(scheduled_engine, cycle_beats);
  if (cycle_seconds <= 0.0) {
    return Steinberg::kResultOk;
  }
  const auto first_cycle = static_cast<Steinberg::int64>(
      std::max(0.0, std::floor(start_seconds / cycle_seconds) - 1.0));
  const auto last_cycle = static_cast<Steinberg::int64>(
      std::max(0.0, std::floor(end_seconds / cycle_seconds)));

  std::size_t order = 0;
  for (Steinberg::int64 cycle = first_cycle; cycle <= last_cycle; ++cycle) {
    const double cycle_start = static_cast<double>(cycle) * cycle_seconds;
    for (const auto& midi_event : scheduled_engine.events()) {
      const auto event_order = order++;
      const double event_seconds =
          cycle_start + beat_to_seconds(scheduled_engine, midi_event.beat);
      if (event_seconds < start_seconds || event_seconds >= end_seconds) {
        continue;
      }
      const auto offset = static_cast<Steinberg::int32>(
          std::floor((event_seconds - start_seconds) * sample_rate));
      if (offset >= 0 && offset < data.numSamples) {
        if (scheduled_events_.size() == scheduled_events_.capacity()) {
          scheduled_events_.clear();
          return Steinberg::kResultFalse;
        }
        scheduled_events_.push_back({offset, &midi_event, event_order});
      }
    }
  }
  return flushScheduledEvents(data.outputEvents);
}

}  // namespace LogoMidi
