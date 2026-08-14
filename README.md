# BPM Clock Nano R4 Eurorack Module

Arduino Nano R4 firmware and build documentation for a 4-output Eurorack BPM clock module with an OLED/encoder UI.

## Current Build

Stable firmware package: `CDK7` compatible, 2026-08-11 swing-toggle update

Core behavior:
- 4 buffered 5V clock/gate outputs: A, B, C, D.
- Same time-division options on all 4 outputs.
- Normal, probability, and slicer pattern modes per output.
- Global BPM control from the encoder.
- Global swing amount from the home screen while the clock is running.
- Per-output swing on/off toggle in each channel menu.
- 8 full-system preset slots.
- OLED home screen with BPM, animated sprite, output summaries, and swing amount.

## Folder Layout

```text
firmware/
  BPMClockNanoR4/
    BPMClockNanoR4.ino
hardware/
  BPM_Clock_Nano_R4_schematic.svg
  BPM Clock Eurorack Module.pdf
  examples/
    build-photos/
docs/
  manual.md
```

## Firmware

Open this sketch in Arduino IDE:

```text
firmware/BPMClockNanoR4/BPMClockNanoR4.ino
```

Board target:

```text
arduino:renesas_uno:nanor4
```

Libraries used:
- `Arduino.h`
- `EEPROM.h`
- `Wire.h`

No third-party Arduino libraries are required.

## Outputs

The firmware intentionally maps C and D in swapped order to match the current jack wiring:

```cpp
const uint8_t outputPins[] = {OUT_A, OUT_B, OUT_D, OUT_C};
```

If you build with straight A/B/C/D jack wiring, update this pin map before flashing.

## Time Divisions

Available on every output:

```text
/8, /4, /2, x1, x2, x3, x4, x6, x8, x12, x16, x24
```

The global swing amount affects only outputs with `SWING` set to `ON`.
Use `SWING OFF` for any output that should stay straight, including `x1` or `x24` timing anchors.

`x24` is intended for Gris/Grids-style clock input.

## Documentation

Start with:

```text
docs/manual.md
```

Hardware references are in:

```text
hardware/
```

Completed build reference photos are in:

```text
hardware/examples/build-photos/
```

The primary schematic is:

```text
hardware/BPM_Clock_Nano_R4_schematic.svg
```

## Compile Check

Last verified compile:

```text
Sketch uses 68984 bytes (26%) of program storage space.
Global variables use 8020 bytes (24%) of dynamic memory.
```

## License

This project is released under the Unlicense. See [LICENSE](LICENSE).

## Acknowledgments

Documentation, cleanup, and release organization were assisted by OpenAI Codex.
