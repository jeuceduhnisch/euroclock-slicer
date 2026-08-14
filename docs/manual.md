# BPM Clock Nano R4 Module Manual

GitHub package date: 2026-08-11

## Package Contents

- `firmware/BPMClockNanoR4/BPMClockNanoR4.ino` - stable Arduino Nano R4 firmware.
- `hardware/BPM_Clock_Nano_R4_schematic.svg` - proper vector electrical schematic.
- `hardware/BPM Clock Eurorack Module.pdf` - original board/reference PDF.
- `hardware/examples/build-photos/` - completed module photos for physical layout reference.

## Hardware Summary

Board target:
- Arduino Nano R4
- Arduino CLI/IDE board: `arduino:renesas_uno:nanor4`

OLED/encoder board pins:
- `COM -> GND`
- `SDA -> A4`
- `SCL -> A5`
- `PSH -> D4`
- `TRA -> D2`
- `TRB -> D3`
- `GND -> GND`
- `VCC -> +5V`

Clock output pins into the SN74AHCT125N buffer:
- Output A: Arduino `D8`
- Output B: Arduino `D9`
- Output C jack: Arduino `D11`
- Output D jack: Arduino `D10`
- Buffer enable: Arduino `D12`, active low

Firmware note:
- C and D are intentionally swapped in firmware to match the current jack wiring.
- The output pin map in code is `A, B, D, C`.

Build photo examples:
- The photos in `hardware/examples/build-photos/` show one completed hand-built module.
- Use them for physical placement, panel, jack, and board-stack reference.
- Use the schematic for the actual electrical connections.

## Home Screen

The home screen shows:
- BPM number
- Dancing sprite
- Compact output status for A, B, C, and D
- Swing amount
- Each output summary ends with `S` for swing enabled or `-` for swing disabled

The BPM label text is intentionally hidden to keep the screen centered in the round faceplate window.

## Home Controls

Clock stopped or running:
- Turn encoder: adjust BPM by 1.
- Short press encoder: start or stop clock immediately.
- Hold encoder for 2 seconds: enter the menu.

Clock running:
- Hold encoder and turn: adjust global swing amount.

Clock stopped:
- Hold encoder and turn: adjust BPM in steps of 10.

## Menu Controls

In the menu:
- Turn encoder: move through options.
- Short press: select the highlighted option.
- Hold for 2 seconds: go back one screen.

After choosing a final value in a menu tree, the module returns to the main menu page.

## Main Menu

Main menu items:
- `A`
- `B`
- `C`
- `D`
- `PRE`

Each output has:
- `TIME`
- `PATT`
- `SWING`

`PRE` has:
- `LOAD`
- `SAVE`

## Output Time Divisions

Available for each output:
- `/8`
- `/4`
- `/2`
- `x1`
- `x2`
- `x3`
- `x4`
- `x6`
- `x8`
- `x12`
- `x16`
- `x24`

Default output setup:
- A, B, C, and D all default to `x1`.
- For Gris/Grids-style clock input, set the desired output, usually A, to `x24`.

## Pattern Modes

Pattern choices:
- `NORMAL`
- `PROB`
- `SLICER`

### NORMAL

The output fires every step according to its selected time division.

### PROB

The output randomly decides whether to fire on each step.

Probability options:
- `10%`
- `25%`
- `50%`
- `75%`

### SLICER

The slicer uses 16-step gate masks for rhythmic chopping.

Slicer presets:
- `S1 ALT` - alternating hits.
- `S2 PAIR` - paired hits.
- `S3 GATE` - repeating three-on style gate.
- `S4 STAB` - sparse stabs.
- `S5 SYNC` - syncopated pulse.
- `S6 CHOP` - chopped rhythm.
- `S7 BAR` - half-bar blocks.
- `S8 FLIP` - flipped syncopation.
- `S9 OFF` - offbeat 8th-note hat.
- `S10 GAL` - galloping 16ths.
- `S11 SKP` - dense hats with downbeat holes.
- `S12 ROL` - alternating hat rolls.
- `S13 TRS` - tresillo-style accents.
- `S14 ZIG` - zig-zag syncopation.
- `S15 LAT` - late/pushed hat feel.
- `S16 DNB` - busy broken-beat hats.

## Swing

Swing options:
- `0`
- `10`
- `25`
- `50`
- `75`
- `90`

Swing amount is global and is adjusted from the home screen while the clock is running by holding the encoder button and turning.

Each output also has its own `SWING` setting:
- `ON` - this output follows the global swing amount.
- `OFF` - this output stays straight even when global swing is above `0`.

Use `SWING OFF` for steady timing anchors such as `x1` or `x24`.

## Presets

There are 8 full-system preset slots.

Preset contents:
- BPM
- A/B/C/D time divisions
- A/B/C/D pattern modes and pattern values
- A/B/C/D swing on/off settings
- Global swing amount

Preset menu:
- Choose `PRE`
- Choose `LOAD` or `SAVE`
- Choose slot `1` through `8`
- The module returns to the main menu after the slot is selected

The module also saves the last-used settings and restores them on startup.

## Flashing

Open this sketch:

`firmware/BPMClockNanoR4/BPMClockNanoR4.ino`

Board:

`Arduino Nano R4`

Arduino CLI compile target:

```text
arduino:renesas_uno:nanor4
```

The firmware uses only built-in Arduino libraries:
- `Arduino.h`
- `EEPROM.h`
- `Wire.h`

## Current Stable Compile

Last checked compile result:

```text
Sketch uses 68984 bytes (26%) of program storage space.
Global variables use 8020 bytes (24%) of dynamic memory.
```

## Practical Notes

- The clock will not output while inside the menu.
- Output A at `x24` is intended for Gris/Grids-style clock input.
- B, C, and D can be used as normal clock, trigger, probability, or slicer rhythm outputs.
- If an output feels too busy, lower its time division or switch from slicer/probability back to normal.
- If a downstream module expects a steady external clock, use `x24` or `x1` with `NORMAL` and `SWING OFF`.
