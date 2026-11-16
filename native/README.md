# Logo MIDI

**Manufacturer: Dexter U.S.**

This is the native Windows x64 adaptation workspace for the FMSLogo-based
MIDI instrument. Dexter U.S. is the product manufacturer and should be named
as the VST3 vendor/manufacturer. The upstream FMSLogo 8.5.0 source is preserved in
`../vendor/fmslogo-upstream` with its original license and notices.

## Current prototype

The host-independent MIDI timeline, Standard MIDI File renderer, turtle
interpreter, and SVG exporter are built and tested. `logo-midi` has two modes:
music scripts produce MIDI files, and Logo turtle scripts produce standalone
SVG drawings. Both use the shared native C++ libraries; graphics scripts support
the same turtle commands, loops, conditionals, and parameterized procedures as
the VST editor.

```text
logo-midi examples/scale.lgo --output scale.mid
logo-midi --graphics examples/turtle-square.lgo --output square.svg
```

The SVG export uses a 500-by-500 view and includes the visible turtle marker.
Flood-fill commands are accepted but not rasterized; the CLI prints a warning
when the drawing contains them.

The adapted FMSLogo source now has a separate `logo-midi64.exe` target. It
loads a real Logo script through FMSLogo's parser/evaluator with the main window
hidden, and routes the new music primitives into the shared MIDI timeline:

```text
logo-midi64.exe -M output.mid -L composition.lgo
```

This Windows target has not yet been compiled or tested here. Durations are
quarter-note beats.

The original interpreter's primitive table now includes `NOTE`, `CHORD`,
`REST`, `VELOCITY`, `CHANNEL`, `GATE`, `CC`, `PROGRAM`, and `TEMPO`. They send
events through a scoped host interface instead of talking to a MIDI device.
`SYNC` is deliberately a plugin/DAW setting, not a command-line MIDI-file
setting.

## VST3 MIDI effect

The VST3 target outputs MIDI events from the shared timeline, with selectable
major-scale, minor-scale, and major-chord patterns. Its exposed parameters offer
internal tempo or DAW tempo sync. DAW-synced playback requires a valid playing
transport from the host.

The VST3 editor runs the supported native turtle subset, including multiline
`REPEAT` blocks, `IF`/`IFELSE`, and parameterized procedures, but does **not**
embed the full FMSLogo evaluator. The separate FMSLogo-based Windows CLI target
evaluates original Logo scripts; the CMake `logo-midi` prototype exports its
supported turtle subset as SVG.

The editor's Load and Save buttons use native `.lgo` file dialogs. When an
`examples` folder is available from the DAW's working directory, it is used as
the initial location; otherwise the dialog can browse to the folder beside the
extracted Windows package. Scripts are saved as portable files outside the DAW
session and are loaded into the editor before pressing Run.

Build with CMake 3.25+ and a local Steinberg VST3 SDK checkout (3.8.1 was used
for verification):

```powershell
cmake -S native -B native/build-vst -A x64 -DCMAKE_BUILD_TYPE=Release -DLOGO_MIDI_BUILD_VST3=ON -DVST3_SDK_ROOT="C:/path/to/vst3sdk"
cmake --build native/build-vst --config Release --target logo-midi-vst3
```

The plugin was built and passed Steinberg's validator on Linux x64. A Windows
x64 build and DAW check are still pending.

## Build the CMake prototype

With a 64-bit C++17 compiler and CMake 3.20 or newer:

```powershell
cmake -S native -B native/build -A x64
cmake --build native/build --config Release
ctest --test-dir native/build -C Release --output-on-failure
native\build\Release\logo-midi.exe native\examples\scale.lgo --output scale.mid
native\build\Release\logo-midi.exe --graphics native\examples\turtle-square.lgo --output square.svg
```

## Build the FMSLogo-based Windows CLI

The upstream Makefile expects 64-bit MinGW and a built static wxWidgets tree.
Set its wxWidgets source/build paths for your setup, then build from
`vendor/fmslogo-upstream`:

```powershell
make logo-midi64.exe wx_top_srcdir="C:/path/to/wxWidgets-3.1.3-src" wx_top_builddir="C:/path/to/wxWidgets-build"
```

## Compatibility and licensing

Both the upstream primitive table and its Windows build selection have been
updated to omit DLL, hardware/port, direct-device MIDI, and media-playback
commands and implementation units. The removed command bodies are retained in
the upstream snapshot for traceability, but are not registered or selected by
the adapted build.

FMSLogo's existing `MIDIOPEN`/`MIDIMESSAGE` implementation sends directly to
Windows MIDI devices, so it cannot be used unchanged as a DAW's plugin output
path. The new MIDI timeline emits host-neutral events instead.
The VST3 host is not connected to the full original interpreter. Neither
Windows binaries nor DAW playback have been verified.

FMSLogo is GPL-2.0-or-later. Keep its copyright and license notices and
distribute source under compatible terms when shipping a derivative.
