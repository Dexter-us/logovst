#include "logo_midi/music_engine.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

int main() {
  logo_midi::MusicEngine engine;
  logo_midi::MusicEngine alternate_engine;
  assert(logo_midi::current_music_command_sink() == nullptr);
  {
    logo_midi::ScopedMusicCommandSink active(&engine);
    assert(logo_midi::current_music_command_sink() == &engine);
    {
      logo_midi::ScopedMusicCommandSink nested(&alternate_engine);
      assert(logo_midi::current_music_command_sink() == &alternate_engine);
    }
    assert(logo_midi::current_music_command_sink() == &engine);
  }
  assert(logo_midi::current_music_command_sink() == nullptr);

  engine.set_tempo(110);
  engine.set_channel(2);
  engine.set_velocity(100);
  engine.set_gate(0.5);
  engine.note(60, 1.0);
  engine.chord({48, 55, 60}, 2.0);
  engine.rest(0.5);
  engine.control_change(1, 64);
  engine.program_change(10);

  assert(engine.events().size() == 10);
  assert(engine.beat_position() == 3.5);
  assert(engine.tempo() == 110);
  assert(engine.events().front().beat == 0.0);
  assert(engine.events().front().status == 0x91);
  assert(engine.events().front().data1 == 60);
  assert(engine.events().front().data2 == 100);
  assert(engine.events()[1].beat == 0.5);
  assert(engine.events()[1].status == 0x81);
  assert(engine.events()[8].status == 0xb1);
  assert(engine.events()[9].status == 0xc1);

  logo_midi::MusicEngine timing_engine;
  timing_engine.set_gate(0.8);
  timing_engine.note(60, 1.0);
  timing_engine.steal(0.25);
  timing_engine.anticipate(0.1);
  timing_engine.note(62, 1.0);
  timing_engine.sort_events();
  assert(timing_engine.events().size() == 4);
  assert(timing_engine.events()[0].beat == 0.0);
  assert(std::abs(timing_engine.events()[1].beat - 0.9) < 1e-9);
  assert(std::abs(timing_engine.events()[2].beat - 1.05) < 1e-9);
  assert(std::abs(timing_engine.events()[3].beat - 1.7) < 1e-9);

  logo_midi::MusicEngine wait_engine;
  wait_engine.set_tempo(120);
  wait_engine.wait(60);
  assert(wait_engine.beat_position() == 2.0);
  wait_engine.set_tempo(60);
  wait_engine.wait(60);
  assert(wait_engine.beat_position() == 3.0);
  assert(wait_engine.tempo_changes().size() == 2);
  assert(wait_engine.tempo_changes()[0].beat == 0.0);
  assert(wait_engine.tempo_changes()[0].beats_per_minute == 120.0);
  assert(wait_engine.tempo_changes()[1].beat == 2.0);
  assert(wait_engine.tempo_changes()[1].beats_per_minute == 60.0);

  bool rejected = false;
  try {
    wait_engine.wait(-1.0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  const auto output =
      std::filesystem::temp_directory_path() / "logo-midi-engine-test.mid";
  engine.write_midi_file(output);
  std::ifstream midi(output, std::ios::binary);
  char header[4] = {};
  midi.read(header, sizeof(header));
  assert(header[0] == 'M' && header[1] == 'T' && header[2] == 'h' &&
         header[3] == 'd');
  midi.close();
  std::filesystem::remove(output);

  const auto wait_output =
      std::filesystem::temp_directory_path() / "logo-midi-wait-test.mid";
  wait_engine.write_midi_file(wait_output);
  std::ifstream wait_midi(wait_output, std::ios::binary);
  const std::vector<char> midi_bytes((std::istreambuf_iterator<char>(wait_midi)),
                                     std::istreambuf_iterator<char>());
  const std::vector<char> tempo_120{char(0xff), char(0x51), char(0x03),
                                    char(0x07), char(0xa1), char(0x20)};
  const std::vector<char> tempo_60{char(0xff), char(0x51), char(0x03),
                                   char(0x0f), char(0x42), char(0x40)};
  assert(std::search(midi_bytes.begin(), midi_bytes.end(), tempo_120.begin(),
                     tempo_120.end()) != midi_bytes.end());
  assert(std::search(midi_bytes.begin(), midi_bytes.end(), tempo_60.begin(),
                     tempo_60.end()) != midi_bytes.end());
  wait_midi.close();
  std::filesystem::remove(wait_output);

  try {
    engine.set_channel(17);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    engine.note(128, 1.0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  return 0;
}
