#include "logo_midi/music_engine.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
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

  bool rejected = false;
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
