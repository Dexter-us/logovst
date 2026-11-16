#include "logo_midi/music_engine.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace logo_midi {
namespace {

void write_u16(std::ostream& output, std::uint16_t value) {
  output.put(static_cast<char>((value >> 8) & 0xff));
  output.put(static_cast<char>(value & 0xff));
}

void write_u32(std::ostream& output, std::uint32_t value) {
  output.put(static_cast<char>((value >> 24) & 0xff));
  output.put(static_cast<char>((value >> 16) & 0xff));
  output.put(static_cast<char>((value >> 8) & 0xff));
  output.put(static_cast<char>(value & 0xff));
}

void write_fourcc(std::ostream& output, const char* value) {
  output.write(value, 4);
}

void append_variable_length(std::vector<std::uint8_t>& bytes,
                            std::uint32_t value) {
  std::uint8_t buffer[4];
  std::size_t count = 0;
  buffer[count++] = static_cast<std::uint8_t>(value & 0x7f);
  while ((value >>= 7) != 0) {
    buffer[count++] = static_cast<std::uint8_t>((value & 0x7f) | 0x80);
  }
  while (count > 0) {
    bytes.push_back(buffer[--count]);
  }
}

std::uint32_t beat_to_tick(double beat) {
  const double ticks = beat * MusicEngine::kTicksPerBeat;
  if (ticks > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
    throw std::overflow_error("MIDI event is too far into the future.");
  }
  return static_cast<std::uint32_t>(std::llround(ticks));
}

}  // namespace

void MusicEngine::require_range(int value, int minimum, int maximum,
                                const char* label) {
  if (value < minimum || value > maximum) {
    throw std::invalid_argument(std::string(label) + " must be between " +
                                std::to_string(minimum) + " and " +
                                std::to_string(maximum) + ".");
  }
}

void MusicEngine::require_duration(double duration) {
  if (!std::isfinite(duration) || duration <= 0.0) {
    throw std::invalid_argument("Duration must be a finite number greater than zero.");
  }
}

void MusicEngine::add_event(double beat, std::uint8_t message, int data1,
                            int data2) {
  events_.push_back(
      {beat, static_cast<std::uint8_t>(message | channel_),
       static_cast<std::uint8_t>(data1), static_cast<std::uint8_t>(data2)});
}

void MusicEngine::advance(double duration) {
  beat_position_ += duration;
  if (!std::isfinite(beat_position_)) {
    throw std::overflow_error("Musical timeline exceeded its supported range.");
  }
}

void MusicEngine::note(int pitch, double duration) {
  require_range(pitch, 0, 127, "Pitch");
  require_duration(duration);
  add_event(beat_position_, 0x90, pitch, velocity_);
  add_event(beat_position_ + duration * gate_, 0x80, pitch, 0);
  advance(duration);
}

void MusicEngine::chord(const std::vector<int>& pitches, double duration) {
  if (pitches.empty()) {
    throw std::invalid_argument("A chord must contain at least one pitch.");
  }
  require_duration(duration);
  for (const int pitch : pitches) {
    require_range(pitch, 0, 127, "Pitch");
  }
  for (const int pitch : pitches) {
    add_event(beat_position_, 0x90, pitch, velocity_);
  }
  for (const int pitch : pitches) {
    add_event(beat_position_ + duration * gate_, 0x80, pitch, 0);
  }
  advance(duration);
}

void MusicEngine::chord(const int* pitches, int count, double duration) {
  if (pitches == nullptr || count <= 0) {
    throw std::invalid_argument("A chord must contain at least one pitch.");
  }
  chord(std::vector<int>(pitches, pitches + count), duration);
}

void MusicEngine::rest(double duration) {
  require_duration(duration);
  advance(duration);
}

void MusicEngine::control_change(int controller, int value) {
  require_range(controller, 0, 127, "Controller");
  require_range(value, 0, 127, "Control value");
  add_event(beat_position_, 0xb0, controller, value);
}

void MusicEngine::program_change(int program) {
  require_range(program, 0, 127, "Program");
  add_event(beat_position_, 0xc0, program, 0);
}

void MusicEngine::set_velocity(int value) {
  require_range(value, 1, 127, "Velocity");
  velocity_ = value;
}

void MusicEngine::set_channel(int one_based_channel) {
  require_range(one_based_channel, 1, 16, "Channel");
  channel_ = one_based_channel - 1;
}

void MusicEngine::set_gate(double proportion) {
  if (!std::isfinite(proportion) || proportion <= 0.0 || proportion > 1.0) {
    throw std::invalid_argument("Gate must be greater than 0 and at most 1.");
  }
  gate_ = proportion;
}

void MusicEngine::set_tempo(double beats_per_minute) {
  if (!std::isfinite(beats_per_minute) || beats_per_minute < 1.0 ||
      beats_per_minute > 1000.0) {
    throw std::invalid_argument("Tempo must be between 1 and 1000 BPM.");
  }
  tempo_bpm_ = beats_per_minute;
}

void MusicEngine::write_midi_file(const std::filesystem::path& path) const {
  struct EncodedEvent {
    std::uint32_t tick;
    std::uint8_t priority;
    std::vector<std::uint8_t> bytes;
  };

  std::vector<EncodedEvent> encoded;
  encoded.reserve(events_.size() + 2);

  const auto micros_per_quarter =
      static_cast<std::uint32_t>(std::llround(60000000.0 / tempo_bpm_));
  encoded.push_back(
      {0, 0, {0xff, 0x51, 0x03,
              static_cast<std::uint8_t>((micros_per_quarter >> 16) & 0xff),
              static_cast<std::uint8_t>((micros_per_quarter >> 8) & 0xff),
              static_cast<std::uint8_t>(micros_per_quarter & 0xff)}});

  for (const auto& event : events_) {
    const auto is_note_off =
        (event.status & 0xf0) == 0x80 ||
        ((event.status & 0xf0) == 0x90 && event.data2 == 0);
    std::vector<std::uint8_t> message{event.status, event.data1};
    if ((event.status & 0xf0) != 0xc0) {
      message.push_back(event.data2);
    }
    encoded.push_back(
        {beat_to_tick(event.beat), static_cast<std::uint8_t>(is_note_off ? 1 : 2),
         std::move(message)});
  }

  std::stable_sort(encoded.begin(), encoded.end(),
                   [](const EncodedEvent& left, const EncodedEvent& right) {
                     if (left.tick != right.tick) {
                       return left.tick < right.tick;
                     }
                     return left.priority < right.priority;
                   });

  std::vector<std::uint8_t> track;
  std::uint32_t previous_tick = 0;
  for (const auto& event : encoded) {
    append_variable_length(track, event.tick - previous_tick);
    track.insert(track.end(), event.bytes.begin(), event.bytes.end());
    previous_tick = event.tick;
  }
  append_variable_length(track, 0);
  track.insert(track.end(), {0xff, 0x2f, 0x00});

  std::ofstream output(path, std::ios::binary);
  if (!output) {
    throw std::runtime_error("Could not open MIDI output file: " + path.string());
  }
  write_fourcc(output, "MThd");
  write_u32(output, 6);
  write_u16(output, 0);  // Single-track Standard MIDI File.
  write_u16(output, 1);
  write_u16(output, kTicksPerBeat);
  write_fourcc(output, "MTrk");
  if (track.size() > std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("MIDI track is too large.");
  }
  write_u32(output, static_cast<std::uint32_t>(track.size()));
  output.write(reinterpret_cast<const char*>(track.data()),
               static_cast<std::streamsize>(track.size()));
  if (!output) {
    throw std::runtime_error("Failed while writing MIDI output file: " +
                             path.string());
  }
}

}  // namespace logo_midi
