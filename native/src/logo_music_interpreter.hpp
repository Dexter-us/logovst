#pragma once

#include "logo_midi/music_engine.hpp"

#include <string>

namespace logo_midi {

MusicEngine compile_logo_music_script(const std::string& script,
                                      double initial_tempo);

}  // namespace logo_midi
