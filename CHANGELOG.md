# Changelog

## CDK7 swing-toggle update - 2026-08-11

- Time-division labels now use the same notation on every channel: `/8`, `/4`, `/2`, `x1`, `x2`, `x3`, `x4`, `x6`, `x8`, `x12`, `x16`, `x24`.
- Added a per-output `SWING` menu item with `ON` and `OFF` choices.
- Global swing amount now applies only to outputs with `SWING ON`.
- Existing `CDK7` EEPROM/preset slots are preserved; old saved outputs load with swing enabled until resaved.

## CDK7 - 2026-07-23

- Stable 4-output BPM clock build.
- A, B, C, and D all default to `x1`.
- All outputs share the same time-division table.
- Original swing behavior ignored `x1` and `x24`.
- Pattern modes: normal, probability, slicer.
- Slicer presets expanded to `S1` through `S16`.
- 8 full-system preset slots.
- C/D output swap retained in firmware for the current physical jack wiring.
