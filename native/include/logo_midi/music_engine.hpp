#pragma once

#include "logo_midi/music_api.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace logo_midi {

struct MidiEvent {
  double beat = 0.0;
  std::uint8_t status = 0;
  std::uint8_t data1 = 0;
  std::uint8_t data2 = 0;
};

class MusicEngine final : public MusicCommandSink {
 public:
  static constexpr std::uint16_t kTicksPerBeat = 480;

  void note(int pitch, double duration) override;
  void chord(const std::vector<int>& pitches, double duration);
  void chord(const int* pitches, int count, double duration) override;
  void rest(double duration) override;
  void control_change(int controller, int value) override;
  void program_change(int program) override;

  void set_velocity(int value) override;
  void set_channel(int one_based_channel) override;
  void set_gate(double proportion) override;
  void set_tempo(double beats_per_minute) override;

  double beat_position() const noexcept { return beat_position_; }
  double tempo() const noexcept { return tempo_bpm_; }
  int velocity() const noexcept { return velocity_; }
  int channel() const noexcept { return channel_; }
  double gate() const noexcept { return gate_; }
  const std::vector<MidiEvent>& events() const noexcept { return events_; }

  void write_midi_file(const std::filesystem::path& path) const;

 private:
  static void require_range(int value, int minimum, int maximum,
                            const char* label);
  static void require_duration(double duration);
  void add_event(double beat, std::uint8_t message, int data1, int data2);
  void advance(double duration);

  std::vector<MidiEvent> events_;
  double beat_position_ = 0.0;
  double tempo_bpm_ = 120.0;
  double gate_ = 0.8;
  int velocity_ = 96;
  int channel_ = 0;
};

}  // namespace logo_midi
