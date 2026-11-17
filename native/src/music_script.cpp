#include "logo_midi/music_script.hpp"

#include "logo_music_interpreter.hpp"

namespace logo_midi {

MusicEngine compile_music_script(const std::string& script,
                                double initial_tempo) {
  return compile_logo_music_script(script, initial_tempo);
}

}  // namespace logo_midi
