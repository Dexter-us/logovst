#pragma once

namespace logo_midi {

class MusicCommandSink {
 public:
  virtual ~MusicCommandSink() = default;

  virtual void note(int pitch, double duration) = 0;
  virtual void chord(const int* pitches, int count, double duration) = 0;
  virtual void steal(double beats) = 0;
  virtual void anticipate(double beats) = 0;
  virtual void rest(double duration) = 0;
  virtual void control_change(int controller, int value) = 0;
  virtual void program_change(int program) = 0;
  virtual void set_velocity(int value) = 0;
  virtual void set_channel(int one_based_channel) = 0;
  virtual void set_gate(double proportion) = 0;
  virtual void set_tempo(double beats_per_minute) = 0;
};

class ScopedMusicCommandSink {
 public:
  explicit ScopedMusicCommandSink(MusicCommandSink* sink) noexcept;
  ~ScopedMusicCommandSink();

  ScopedMusicCommandSink(const ScopedMusicCommandSink&) = delete;
  ScopedMusicCommandSink& operator=(const ScopedMusicCommandSink&) = delete;

 private:
  MusicCommandSink* previous_;
};

MusicCommandSink* current_music_command_sink() noexcept;

}  // namespace logo_midi
