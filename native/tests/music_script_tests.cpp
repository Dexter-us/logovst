#include "logo_midi/music_script.hpp"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void compiles_music_commands_and_waits() {
  const auto engine = logo_midi::compile_music_script(
      "TEMPO 120\n"
      "CHANNEL 2\n"
      "VELOCITY 88\n"
      "GATE 0.75\n"
      "PROGRAM 10\n"
      "CC 1 20\n"
      "NOTE 60 1\n"
      "CHORD [64 67] 0.5\n"
      "REST 0.5\n"
      "WAIT 30\n");
  require(engine.events().size() == 8, "expected notes and controller events");
  require(engine.beat_position() == 3.0,
          "WAIT at 120 BPM should add one beat after the note, chord and rest");
  require(engine.channel() == 1, "Logo channels are one-based");
  require(engine.tempo_changes().size() == 1 &&
              engine.tempo_changes().front().beats_per_minute == 120.0,
          "the active script tempo should be recorded");
}

void rejects_invalid_commands_with_line_number() {
  try {
    (void)logo_midi::compile_music_script("NOTE 60 1\nBAD 2\n");
  } catch (const std::runtime_error& error) {
    require(std::string(error.what()).find("Line 2:") == 0,
            "parser error should identify the source line");
    return;
  }
  throw std::runtime_error("unsupported command should fail");
}

void records_tempo_changes_for_realtime_playback() {
  const auto engine = logo_midi::compile_music_script(
      "TEMPO 120\nNOTE 60 1\nTEMPO 60\nWAIT 60\nNOTE 62 1\n");
  require(engine.tempo_changes().size() == 2,
          "tempo changes should be retained in the compiled script");
  require(engine.tempo_changes()[1].beat == 1.0 &&
              engine.tempo_changes()[1].beats_per_minute == 60.0,
          "tempo change should keep its script position and value");
  require(engine.beat_position() == 3.0,
          "WAIT should convert to beats using the tempo active at that point");
}

void compiles_steal_and_anticipate_commands() {
  const auto engine = logo_midi::compile_music_script(
      "GATE 0.8\n"
      "NOTE 60 1\n"
      "STEAL 0.25\n"
      "ANTICIPATE 0.1\n"
      "NOTE 62 1\n");
  require(engine.events().size() == 4, "expected two notes");
  require(std::abs(engine.events()[1].beat - 0.9) < 1e-9,
          "ANTICIPATE should move the next note and its release earlier");
  require(std::abs(engine.events()[2].beat - 1.05) < 1e-9,
          "STEAL should lengthen the previous note's release");
  require(std::abs(engine.events()[3].beat - 1.7) < 1e-9,
          "the anticipated note duration should start from its earlier onset");
}

void supports_random_notes_in_numeric_expressions() {
  const auto engine = logo_midi::compile_music_script(
      "NOTE (60 + RANDOM 12) 0.5\n"
      "NOTE random 12 0.5\n"
      "CHORD [60 (60 + RANDOM 12)] 1\n");
  require(engine.events().size() == 8,
          "expected two notes and a two-note chord");
  require(engine.events()[0].data1 >= 60 && engine.events()[0].data1 <= 71,
          "RANDOM pitch expression should stay within its requested range");
  require(engine.events()[2].data1 >= 0 && engine.events()[2].data1 <= 11,
          "standalone RANDOM should return a value below its limit");
  require(engine.events()[5].data1 >= 60 && engine.events()[5].data1 <= 71,
          "RANDOM expressions should work inside chord pitch lists");
}

void supports_numeric_variables_and_logo_reporters() {
  const std::string script =
      "RERANDOM 25\n"
      "MAKE \"BASE 60\n"
      "MAKE \"MID SUM :BASE RANDOM 12\n"
      "MAKE \"POWERED POWER 2 3\n"
      "NOTE :MID 0.5\n"
      "NOTE (SUM :BASE (INT 2.9)) 0.25\n"
      "NOTE (MAX :POWERED (SQRT 144)) 0.5\n"
      "NOTE (LESS? 2 3) 0.5\n"
      "NOTE (THING \"BASE) 0.5\n";
  const auto first = logo_midi::compile_music_script(script);
  const auto repeated = logo_midi::compile_music_script(script);
  require(first.events().size() == 10,
          "expected five variable/reporter-driven notes");
  require(first.events()[0].data1 >= 60 && first.events()[0].data1 <= 71,
          "MAKE should store evaluated RANDOM reporter results");
  require(first.events()[2].data1 == 62,
          "arithmetic and INT should compose inside an expression");
  require(first.events()[4].data1 == 12,
          "MAX and SQRT should evaluate as numeric reporters");
  require(first.events()[6].data1 == 1,
          "comparison reporters should return numeric booleans");
  require(first.events()[8].data1 == 60,
          "THING should read a variable by its quoted name");
  for (std::size_t index = 0; index < first.events().size(); index += 2) {
    require(first.events()[index].data1 == repeated.events()[index].data1,
            "RERANDOM with the same seed should reproduce the same notes");
  }
}

void supports_words_lists_reporter_procedures_and_control_flow() {
  const auto engine = logo_midi::compile_music_script(
      "TO TRANSPOSE :PITCH :AMOUNT\n"
      "OUTPUT SUM :PITCH :AMOUNT\n"
      "END\n"
      "TO SCALE :BASE\n"
      "OUTPUT LIST :BASE SUM :BASE 2\n"
      "END\n"
      "TO NAME4 :PITCH\n"
      "OUTPUT WORD \"C :PITCH\n"
      "END\n"
      "TO PLAY :PITCH\n"
      "NOTE :PITCH 0.25\n"
      "END\n"
      "MAKE \"MOTIF SENTENCE SCALE 60 [64]\n"
      "MAKE \"VOICES SENTENCE :MOTIF [67 71]\n"
      "MAKE \"VOICES LPUT TRANSPOSE LAST :MOTIF 8 :VOICES\n"
      "MAKE \"NAME NAME4 4\n"
      "REPEAT 3 [ NOTE ITEM REPCOUNT :MOTIF 0.25 ]\n"
      "PLAY FIRST :MOTIF\n"
      "MAKE \"INDEX 0\n"
      "WHILE [ :INDEX < 2 ] [\n"
      "  NOTE ITEM SUM :INDEX 1 :MOTIF 0.5\n"
      "  MAKE \"INDEX SUM :INDEX 1\n"
      "]\n"
      "UNTIL [ :INDEX >= 3 ] [\n"
      "  NOTE 67 0.25\n"
      "  MAKE \"INDEX SUM :INDEX 1\n"
      "]\n"
      "IFELSE AND MEMBER? 72 :VOICES EQUAL? FIRST :NAME \"C [\n"
      "  NOTE TRANSPOSE FIRST :MOTIF 12 0.5\n"
      "] [ REST 0.5 ]\n");
  require(engine.events().size() == 16,
          "expected notes from REPEAT, WHILE, procedure calls, and IFELSE");
  const int expected_pitches[] = {60, 62, 64, 60, 60, 62, 67, 72};
  for (std::size_t note = 0; note < 8; ++note) {
    require(engine.events()[note * 2].data1 == expected_pitches[note],
            "Logo values and control flow should produce the expected note sequence");
  }
  require(engine.beat_position() == 2.75,
          "only selected branches and loop iterations should advance time");
}

void bounds_forever_loops_for_offline_rendering() {
  try {
    (void)logo_midi::compile_music_script("NOTE 60 0.25\nFOREVER []\n");
  } catch (const std::runtime_error& error) {
    require(std::string(error.what()).find("4096-iteration safety limit") !=
                std::string::npos,
            "FOREVER should stop with an explicit safety-limit error");
    return;
  }
  throw std::runtime_error("FOREVER should not run without a safety limit");
}

}  // namespace

int main() {
  try {
    compiles_music_commands_and_waits();
    rejects_invalid_commands_with_line_number();
    records_tempo_changes_for_realtime_playback();
    compiles_steal_and_anticipate_commands();
    supports_random_notes_in_numeric_expressions();
    supports_numeric_variables_and_logo_reporters();
    supports_words_lists_reporter_procedures_and_control_flow();
    bounds_forever_loops_for_offline_rendering();
  } catch (const std::exception& error) {
    std::cerr << "Music script test failed: " << error.what() << '\n';
    return 1;
  }
  std::cout << "Music script tests passed.\n";
  return 0;
}
