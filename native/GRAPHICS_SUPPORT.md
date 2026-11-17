# VST Turtle Graphics Support Plan

The FMSLogo-inspired turtle primitives use a reusable, framework-neutral C++
graphics engine, keeping wxWidgets out of the DAW process. The VSTGUI editor
contains a multiline Logo command editor, Run button, status display, and live
turtle canvas. The `logo-midi --graphics` CLI mode runs the same interpreter
and exports the scene as SVG.

The VST editor and `logo-midi --graphics` CLI mode accept case-insensitive
Logo commands, ignore blank lines and semicolon comments, and support
parameterized `TO`/`END` procedures, multiline `REPEAT` blocks, and `IF`/
`IFELSE` conditionals. Scripts are limited to 64 KiB, 4096 executed commands,
4096 repetitions per loop, 256 procedures, and 64 nested loops, conditional
blocks, or procedure calls. A failed script does not partially change the scene.

Example:

```logo
CS
SETPC [46 190 210]
PENSIZE 3
FD 90
RT 90
FD 90
LABEL "Hello Logo"
```

`REPEAT` accepts a count from 0 to 4096 and a bracketed command list. Blocks can
nest, and multiple fixed-argument commands may share a line:

```logo
REPEAT 4 [
  FD 90
  RT 90
]
```

Procedures can be defined with `TO` and `END`, then called by name. Parameters
are declared with a leading colon and referenced the same way in the body.
Calls pass one value per parameter; values can be forwarded to nested procedures
and used anywhere a supported command expects an argument. Definitions may
appear before or after their calls. Expressions and interpolation inside quoted
strings are not supported. Nested loops and procedure calls share a 64-level
limit, and the overall command limit also bounds repeated or recursive work.

```logo
TO SQUARE :SIZE
  REPEAT 4 [ FD :SIZE RT 90 ]
END
SQUARE 90
SQUARE 45
```

`IF` and `IFELSE` conditions accept `TRUE`, `FALSE`, or a numeric comparison
using `=`, `<>`, `!=`, `<`, `<=`, `>`, or `>=`. Put spaces around the operator.
Only the selected block runs, and conditionals can be nested:

```logo
IFELSE :SIZE > 60 [
  SETPC [46 190 210]
] [
  SETPC [246 190 82]
]
```

Malformed conditions and blocks report their source line and leave the previous
drawing unchanged.

This is a documented subset, not a full FMSLogo interpreter. General Logo
expressions are not supported in graphics scripts. The separate music dialect
supports number, boolean, word, and list values; list/word and numeric
reporters; `TO`/`END` procedures with `OUTPUT`; and bounded `REPEAT`, `IF`,
`IFELSE`, `WHILE`, `UNTIL`, and `FOREVER` blocks. It remains a music-focused
subset and does not provide the complete FMSLogo primitive library or workspace.

## Batch 1 — 2D turtle state and strokes

**Engine, supported command subset, and VST editor wiring implemented.**

- Movement: `FORWARD`/`FD`, `BACK`/`BK`, `LEFT`/`LT`, `RIGHT`/`RT`, `HOME`
- Position and heading: `SETPOS`, `SETXY`, `SETX`, `SETY`, `SETHEADING`/`SETH`
- Pen state: `PENUP`/`PU`, `PENDOWN`/`PD`, `PENCOLOR`/`SETPENCOLOR`,
  `PENSIZE`/`SETPENSIZE`
- Pen mode state: paint, erase, reverse
- Canvas state: screen background color, `CLEAN`, `CLEARSCREEN`/`CS`
- Turtle visibility state: show/hide

The scene model retains line segments with their color, width, and pen mode for
the VST editor to draw. `CLEAN` clears the drawing without moving the turtle;
`CLEARSCREEN` clears and returns it to the origin with heading zero.

## Batch 2 — additional 2D drawing

**Scene-engine support implemented; interpreter covers most listed primitives.**

- Boundaries: `WRAP`, `FENCE`, `WINDOW`
- Drawing: `FILL`, `POLYSTART`, `POLYEND`, `ELLIPSEARC`
- Text: `LABEL` with current position, heading, color, and font settings
- Pixels: `SETPIXEL`; `PIXEL` currently reads explicit pixel writes or the
  background, not rasterized strokes or text

`FILL` is retained as a flood-fill operation and reported as not yet rendered.
`POLYEND` retains a filled polygon from pen-down movement endpoints.
`ELLIPSEARC` is converted to line segments and leaves the turtle at its original
position, matching FMSLogo's behavior.

Still to implement in the 2D family: `POLYVIEW`, `LABELSIZE`, `SCRUNCH`,
`SETSCRUNCH`, `SCROLLX`, `SCROLLY`, `ZOOM`, palette support, and raster-accurate
pixel/flood-fill queries. Common Logo-library helpers such as `ARC`, `CIRCLE`,
and `ELLIPSE` can then be expressed using the supported primitives.

## Batch 3 — turtles, bitmap drawing, and image files

- Multi-turtle state: `TURTLE`, `TURTLES`, `SETTURTLE`, `HASOWNPEN?`
- Turtle display: `SHOWTURTLE`, `HIDETURTLE`, `SHOWN?`
- Bitmap turtle modes: `BITMAPTURTLE`, `NOBITMAPTURTLE`, `TURTLEMODE`,
  `SETTURTLEMODE`
- Bitmap operations: `BITLOAD`, `BITLOADSIZE`, `BITSAVE`, `BITSIZE`, `BITCUT`,
  `BITCOPY`, `BITPASTE`, `BITPASTETOINDEX`, `BITFIT`, `BITBLOCK`, `BITMODE`,
  `SETBITMODE`, `BITINDEX`, `SETBITINDEX`
- GIF operations: `GIFLOAD`, `GIFSIZE`, `GIFSAVE`

File-backed image commands will need host-safe file handling before they can be
enabled inside a VST.

## Batch 4 — 3D turtle graphics

- Orientation: `UP`/`UPPITCH`, `DOWN`/`DOWNPITCH`, `LEFTROLL`/`LR`,
  `RIGHTROLL`/`RR`, `PITCH`, `SETPITCH`, `ROLL`, `SETROLL`
- Position and targeting: `POSXYZ`, `SETPOSXYZ`, `SETXYZ`, `SETZ`,
  `TOWARDSXYZ`
- Scene mode and lighting: `PERSPECTIVE`, `LIGHT`, `SETLIGHT`, `SETCLIP`
- Library helpers: `DISTANCE`, `DISTANCEXYZ`, `ORIENTATION`, `SETORIENTATION`

## Batch 5 — application windows and interaction

These are not canvas-drawing commands. They need VST-editor equivalents or
should be documented as unsupported:

- Screen layout: `FULLSCREEN`, `SPLITSCREEN`, `TEXTSCREEN`, `STATUS`, `NOSTATUS`
- Input: `MOUSEPOS`, `KEYBOARDVALUE`, `BUTTONP`
- Window and widget APIs: button, checkbox, combo-box, dialog, group-box,
  list-box, radio-button, scrollbar, static-label, and custom-window commands

## Compatibility tracking

- The VST editor uses Steinberg VSTGUI.
- The Windows x64 command-line build exports SVG from the same turtle scene
  model as the VST editor.
- Supported movement, pen, boundary, polygon, label, ellipse-arc, and pixel
  commands are sent to the shared scene model and rendered in the editor.
- `PIXEL` currently reports approximate vector-scene sampling; it cannot sample
  antialiased strokes, labels, or flood fills.
- DLL calls, direct hardware access, serial/parallel ports, and CD/media control
  remain outside the supported command set.
