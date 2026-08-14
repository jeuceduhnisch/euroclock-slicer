# Changelog

## CDK7 swing-toggle update - 2026-08-11

- Time-division labels now use the same notation on every channel: `/8`, `/4`, `/2`, `x1`, `x2`, `x3`, `x4`, `x6`, `x8`, `x12`, `x16`, `x24`.
- Added a per-output `SWING` menu item with `ON` and `OFF` choices.
- Global swing amount now applies only to outputs with `SWING ON`.
- Existing `CDK7` EEPROM/preset slots are preserved; old saved outputs load with swing enabled until resaved.
