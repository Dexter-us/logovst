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
logo-midi --graphics examples/turtle-square.lgo --output wide.svg --size 800x450
```

The SVG export defaults to a 500-by-500 view and includes the visible turtle
marker. Use `--size WIDTHxHEIGHT` to choose dimensions from 1 to 16384. Logo
query output such as `PIXEL` is printed to the command line. Flood-fill commands
are accepted but not rasterized; the CLI prints a warning when the drawing
contains them.

The music CLI also supports the original FMSLogo `WAIT` command. Its argument
is measured in 1/60-second ticks; in a MIDI file it becomes a rest measured at
the tempo in effect at that point. `REST` remains available for beat-based
silence. Example scripts are in `native/examples`: load `graphics-command-tour.lgo`
or `turtle-square.lgo` in the VST editor, and run `music-command-tour.lgo` with
the CLI. The VST editor currently runs turtle/graphics scripts, not music scripts.

```text
logo-midi native/examples/music-command-tour.lgo --output tour.mid
logo-midi --graphics native/examples/graphics-command-tour.lgo --output tour.svg
```

The adapted FMSLogo source now has a separate `logo-midi64.exe` target. It
loads a real Logo script through FMSLogo's parser/evaluator with the main window
hidden, and routes the new music primitives into the shared MIDI timeline:

```text
logo-midi64.exe -M output.mid -L composition.lgo
```

This Windows target has not yet been compiled or tested here. Durations are
quarter-note beats.

The music commands are `TEMPO`, `WAIT`, `NOTE`, `CHORD`, `REST`, `STEAL`,
`ANTICIPATE`, `VELOCITY`, `CHANNEL`, `GATE`, `CC`, and `PROGRAM`. They send
events through a scoped host interface instead of talking to a MIDI device.
Music scripts support number, boolean, word, and list values. Use `MAKE "NAME
value` to assign a value, `:NAME` to read it, and `THING "NAME` as a reporter.
List and word reporters include `WORD`, `LIST`, `SENTENCE`, `FIRST`, `LAST`,
`ITEM`, `COUNT`, `EMPTY?`, `MEMBER?`, `FPUT`, `LPUT`, `BUTFIRST`, `BUTLAST`,
`PICK`, and `REVERSE`. Numeric reporters include `RANDOM`, `SUM`,
`DIFFERENCE`, `PRODUCT`, `QUOTIENT`, `REMAINDER`, `MODULO`, `POWER`, math and
comparison reporters. `RERANDOM [seed]` makes random sequences repeatable.

User procedures can return reporter values with `OUTPUT`:

```logo
TO TRANSPOSE :PITCH :STEPS
  OUTPUT SUM :PITCH :STEPS
END
NOTE TRANSPOSE 60 12 1
```

Music scripts support `REPEAT`, `IF`, `IFELSE`, `WHILE`, `UNTIL`, and bounded
`FOREVER` blocks. Script execution is limited to 64 KiB, 4096 tokens and
executed commands, 4096 loop iterations, and 64 nested blocks or procedure
calls. This is a useful music-making subset, not the entire FMSLogo language:
unimplemented primitives and general Logo workspace features are not available.
`STEAL beats` lengthens the previous note or chord release without moving the
timeline cursor. `ANTICIPATE beats` starts the next note or chord early without
moving that cursor. `SYNC` is a plugin/DAW setting, not a command-line MIDI-file
setting.

```text
TEMPO 120               ; BPM, 1..1000
WAIT 60                 ; Logo ticks, 60 = 1 second
NOTE 60 0.5             ; pitch 0..127, duration in beats
MAKE "BASE 60
MAKE "PITCH SUM :BASE RANDOM 12
NOTE :PITCH 0.5
NOTE (60 + RANDOM 12) 0.5 ; random pitch from 60 through 71
MAKE "SCALE [60 62 64 67]
REPEAT COUNT :SCALE [
  NOTE ITEM REPCOUNT :SCALE 0.25
]
CHORD [60 64 67] 1      ; pitches and duration in beats
STEAL 0.15              ; lengthen the previous note or chord
ANTICIPATE 0.1          ; start the next note or chord early
REST 1                  ; silence in beats
VELOCITY 96             ; 1..127
CHANNEL 1               ; 1..16
GATE 0.8                ; note length fraction, greater than 0 through 1
CC 1 32                 ; controller 0..127, value 0..127
PROGRAM 0               ; instrument program 0..127
```

## VST3 MIDI effect

The VST3 editor can load a music script and press Run to send its events to the
plugin's MIDI output; the script repeats while the DAW transport is playing.
`native/examples/music-commands-reference.lgo` demonstrates each supported
music command and can be loaded directly. Script tempo changes apply when using
internal tempo; optional DAW tempo sync uses the host's tempo instead.
`native/examples/timing-ornaments.lgo` shows the two time-stealing commands.
`native/examples/music-expressions.lgo` demonstrates variables, arithmetic,
reporters, `RANDOM`, and `RERANDOM`.
`native/examples/music-logo-language.lgo` demonstrates words, lists,
user-defined reporters, and control-flow blocks.

The VST3 editor runs the supported native turtle subset, including multiline
`REPEAT` blocks, `IF`/`IFELSE`, and parameterized procedures, but does **not**
embed the full FMSLogo evaluator. The separate FMSLogo-based Windows CLI target
evaluates original Logo scripts; the CMake `logo-midi` prototype exports its
supported turtle subset as SVG.

The editor's Load and Save buttons use native `.lgo` file dialogs. When an
`examples` folder is available from the DAW's working directory, it is used as
the initial location; otherwise the dialog can browse to the folder beside the
extracted Windows package. Both graphics and music scripts are saved as
portable files outside the DAW session and are loaded into the editor before
pressing Run.

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
The VST3 host is not connected to the full original interpreter. The Windows
build and real DAW playback have not been verified in this environment.

FMSLogo is GPL-2.0-or-later. Keep its copyright and license notices and
distribute source under compatible terms when shipping a derivative.
