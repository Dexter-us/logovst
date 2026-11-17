#pragma once

#include "logo_midi/music_engine.hpp"

#include <string>

namespace logo_midi {

// Parses music commands with Logo values, reporters, and control flow.
MusicEngine compile_music_script(const std::string& script,
                                double initial_tempo = 120.0);

}  // namespace logo_midi
