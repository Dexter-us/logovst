#include "logo_midi/music_api.hpp"

namespace logo_midi {
namespace {
thread_local MusicCommandSink* active_sink = nullptr;
}

ScopedMusicCommandSink::ScopedMusicCommandSink(
    MusicCommandSink* sink) noexcept
    : previous_(active_sink) {
  active_sink = sink;
}

ScopedMusicCommandSink::~ScopedMusicCommandSink() {
  active_sink = previous_;
}

MusicCommandSink* current_music_command_sink() noexcept {
  return active_sink;
}

}  // namespace logo_midi
