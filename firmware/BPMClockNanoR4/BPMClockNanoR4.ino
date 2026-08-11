#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>

// OLED/encoder board pins.
const uint8_t ENC_A = 2;   // TRA
const uint8_t ENC_B = 3;   // TRB
const uint8_t ENC_SW = 4;  // PSH

// Buffered outputs into SN74AHCT125N.
const uint8_t OUT_A = 8;
const uint8_t OUT_B = 9;
const uint8_t OUT_C = 10;
const uint8_t OUT_D = 11;
const uint8_t BUF_EN = 12; // AHCT enable node, active low.

const uint8_t OLED_ADDR = 0x3C;
const uint8_t OLED_WIDTH = 128;
const uint8_t OLED_HEIGHT = 64;

const float MIN_BPM = 20.0;
const float MAX_BPM = 300.0;
const uint32_t MAX_PULSE_WIDTH_US = 5000;
const uint16_t LONG_PRESS_MS = 2000;
const uint16_t SETTINGS_SAVE_DELAY_MS = 1000;

uint32_t maxU32(uint32_t a, uint32_t b) {
  return a > b ? a : b;
}

enum Screen {
  SCREEN_HOME,
  SCREEN_PICK_ITEM,
  SCREEN_PICK_EDIT,
  SCREEN_PICK_VALUE,
  SCREEN_PICK_PATTERN_MODE,
  SCREEN_PICK_PATTERN_VALUE,
  SCREEN_PICK_SWING,
  SCREEN_PICK_PRESET_SLOT,
  SCREEN_PICK_PRESET_ACTION
};

enum PatternMode {
  PATTERN_NORMAL,
  PATTERN_SLICER,
  PATTERN_PROB
};

struct ClockOutput {
  uint8_t divisionIndex;
  PatternMode patternMode;
  uint8_t patternIndex;
  uint8_t swingEnabled;
};

struct TimeDivisionPreset {
  const char *label;
  uint8_t numerator;
  uint8_t denominator;
};

struct SlicerPreset {
  uint16_t mask;
  uint8_t steps;
  const char *label;
};

// C and D are intentionally swapped here to match the current jack wiring.
const uint8_t outputPins[] = {OUT_A, OUT_B, OUT_D, OUT_C};
const TimeDivisionPreset timeDivisions[] = {
  {"/8", 8, 1},
  {"/4", 4, 1},
  {"/2", 2, 1},
  {"x1", 1, 1},
  {"x2", 1, 2},
  {"x3", 1, 3},
  {"x4", 1, 4},
  {"x6", 1, 6},
  {"x8", 1, 8},
  {"x12", 1, 12},
  {"x16", 1, 16},
  {"x24", 1, 24}
};
const SlicerPreset slicerPresets[] = {
  {0x5555, 16, "S1 ALT"},   // 1010 1010 1010 1010
  {0x3333, 16, "S2 PAIR"},  // 1100 1100 1100 1100
  {0x7777, 16, "S3 GATE"},  // 1110 1110 1110 1110
  {0x1111, 16, "S4 STAB"},  // 1000 1000 1000 1000
  {0x6DB6, 16, "S5 SYNC"},
  {0x5AD6, 16, "S6 CHOP"},
  {0x0F0F, 16, "S7 BAR"},
  {0x9669, 16, "S8 FLIP"},
  {0x4444, 16, "S9 OFF"},   // Offbeat 8th-note hat.
  {0x9999, 16, "S10 GAL"},  // Galloping 16ths.
  {0xEEEE, 16, "S11 SKP"},  // Dense hats with downbeat holes.
  {0xD7D7, 16, "S12 ROL"},  // Alternating hat rolls.
  {0x2449, 16, "S13 TRS"},  // Tresillo-style accents.
  {0xA5A5, 16, "S14 ZIG"},  // Zig-zag syncopation.
  {0x5A5A, 16, "S15 LAT"},  // Late/pushed hat feel.
  {0xDDAD, 16, "S16 DNB"}   // Busy broken-beat hats.
};
const uint8_t probabilityPresets[] = {10, 25, 50, 75};
const uint8_t swingPresets[] = {0, 10, 25, 50, 75, 90};
const uint8_t NUM_OUTPUTS = sizeof(outputPins) / sizeof(outputPins[0]);
const uint8_t NUM_DIVISIONS = sizeof(timeDivisions) / sizeof(timeDivisions[0]);
const uint8_t NUM_SLICER = sizeof(slicerPresets) / sizeof(slicerPresets[0]);
const uint8_t NUM_PROB = sizeof(probabilityPresets) / sizeof(probabilityPresets[0]);
const uint8_t NUM_SWING = sizeof(swingPresets) / sizeof(swingPresets[0]);
const uint8_t NUM_PRESETS = 8;
const uint8_t DIVISION_1X = 3;
const uint8_t DIVISION_24X = 11;

struct SavedSettings {
  uint32_t magic;
  uint8_t divisionIndex[4];
  uint8_t patternMode[4];
  uint8_t patternIndex[4];
  uint8_t swingIndex;
  uint16_t bpmTimes10;
};

SavedSettings captureSettings();
void applySettings(const SavedSettings &saved);
void syncClock();
void handleClock();

const uint32_t SETTINGS_MAGIC = 0x43444B37UL; // "CDK7"
const int EEPROM_ADDR = 0;
const uint8_t SAVED_DIVISION_MASK = 0x3F;
const uint8_t SAVED_SWING_MARKER = 0x40;
const uint8_t SAVED_SWING_ON = 0x80;

float bpm = 120.0;
bool running = false;
Screen screen = SCREEN_HOME;

ClockOutput clockOutputs[4] = {
  {DIVISION_1X, PATTERN_NORMAL, 0, 1},
  {DIVISION_1X, PATTERN_NORMAL, 0, 1},
  {DIVISION_1X, PATTERN_NORMAL, 0, 1},
  {DIVISION_1X, PATTERN_NORMAL, 0, 1}
};

uint32_t nextPulseUs[4] = {0, 0, 0, 0};
uint32_t outputOffUs[4] = {0, 0, 0, 0};
bool outputHigh[4] = {false, false, false, false};
uint32_t outputStep[4] = {0, 0, 0, 0};
uint32_t lastPulseMs[4] = {0, 0, 0, 0};
uint32_t lastBeatMs = 0;
uint32_t nextBeatCueUs = 0;
uint32_t visualBeatStep = 0;

int8_t encoderDelta = 0;
uint8_t lastEncoded = 0;

bool lastButtonReading = HIGH;
bool buttonState = HIGH;
uint32_t lastDebounceMs = 0;
uint32_t buttonDownMs = 0;
bool longPressHandled = false;
bool buttonTurnHandled = false;

uint32_t lastDisplayMs = 0;
bool displayDirty = true;
bool allowRunDisplayRefresh = false;
uint8_t selectedItem = 0; // 0-3 = A-D, 4 = Preset
uint8_t selectedOutput = 0; // 0-3 = A-D
uint8_t selectedEdit = 0;   // 0 = Time, 1 = Pattern, 2 = Swing
uint8_t pendingDivisionIndex = DIVISION_1X;
PatternMode pendingPatternMode = PATTERN_NORMAL;
uint8_t pendingPatternIndex = 0;
uint8_t pendingSwingEnabled = 1;
uint8_t swingIndex = 0;
uint8_t selectedPresetSlot = 0;
uint8_t selectedPresetAction = 0; // 0 = Load, 1 = Save
bool settingsSavePending = false;
uint32_t settingsSaveDueMs = 0;

uint8_t oledBuffer[OLED_WIDTH * OLED_HEIGHT / 8];

const uint8_t font5x7[96][5] = {
  {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
  {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
  {0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
  {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
  {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
  {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
  {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
  {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
  {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
  {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
  {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x7F,0x10,0x28,0x44,0x00},
  {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
  {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
  {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
  {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
  {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x10,0x08,0x08,0x10,0x08},{0x00,0x06,0x09,0x09,0x06}
};

void oledCommand(uint8_t command) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(command);
  Wire.endTransmission();
}

void oledDataBlock(const uint8_t *data, uint8_t count) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x40);
  for (uint8_t i = 0; i < count; i++) Wire.write(data[i]);
  Wire.endTransmission();
}

void oledSetCursor(uint8_t col, uint8_t row) {
  oledCommand(0xB0 + row);
  oledCommand(0x00 + (col & 0x0F));
  oledCommand(0x10 + ((col >> 4) & 0x0F));
}

void oledClear() {
  memset(oledBuffer, 0, sizeof(oledBuffer));
}

void oledFlush() {
  for (uint8_t row = 0; row < 8; row++) {
    oledSetCursor(0, row);
    for (uint8_t col = 0; col < OLED_WIDTH; col += 16) {
      oledDataBlock(&oledBuffer[row * OLED_WIDTH + col], 16);
      handleClock();
    }
  }
}

void oledSetPhysicalPixel(uint8_t x, uint8_t y, bool on) {
  if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
  uint16_t index = x + (y / 8) * OLED_WIDTH;
  uint8_t bit = 1 << (y & 7);
  if (on) oledBuffer[index] |= bit;
  else oledBuffer[index] &= ~bit;
}

void oledSetLogicalPixel(uint8_t x, uint8_t y, bool on) {
  if (x >= OLED_HEIGHT || y >= OLED_WIDTH) return;
  uint8_t physicalX = y;
  uint8_t physicalY = OLED_HEIGHT - 1 - x;
  oledSetPhysicalPixel(physicalX, physicalY, on);
}

void oledDrawCharPx(uint8_t x, uint8_t y, char c, uint8_t scale) {
  if (c < 32 || c > 127) c = '?';
  const uint8_t *glyph = font5x7[c - 32];
  for (uint8_t gx = 0; gx < 5; gx++) {
    uint8_t column = glyph[gx];
    for (uint8_t gy = 0; gy < 7; gy++) {
      if (column & (1 << gy)) {
        for (uint8_t sx = 0; sx < scale; sx++) {
          for (uint8_t sy = 0; sy < scale; sy++) {
            oledSetLogicalPixel(x + gx * scale + sx, y + gy * scale + sy, true);
          }
        }
      }
    }
  }
}

void oledDrawStringPx(uint8_t x, uint8_t y, const char *text, uint8_t scale) {
  while (*text) {
    oledDrawCharPx(x, y, *text++, scale);
    x += 6 * scale;
  }
}

void oledDrawCenteredStringPx(uint8_t y, const char *text, uint8_t scale) {
  uint8_t width = strlen(text) * 6 * scale;
  uint8_t x = width >= OLED_HEIGHT ? 0 : (OLED_HEIGHT - width) / 2;
  oledDrawStringPx(x, y, text, scale);
}

const char duckSpriteA[18][19] = {
  "......######......",
  "....##########....",
  "...###......###...",
  "..##..#....#..##..",
  "..##..........##..",
  ".###...####...###.",
  ".####........####.",
  "#####..####..#####",
  "#####........#####",
  ".###############..",
  "..###########.....",
  "...#########......",
  "....#######.......",
  "...##.###.##......",
  "..##..###..##.....",
  ".##...###...##....",
  "......###.........",
  "................."
};

const char duckSpriteB[18][19] = {
  "......######......",
  "....##########....",
  "...###......###...",
  "..##..#....#..##..",
  "..##..........##..",
  ".###...####...###.",
  ".####........####.",
  "#####..####..#####",
  "#####........#####",
  "..###############.",
  ".....###########..",
  "......#########...",
  ".......#######....",
  "......##.###.##...",
  ".....##..###..##..",
  "....##...###...##.",
  ".........###......",
  "................."
};

void oledDrawDuckFrame(uint8_t x, uint8_t y, bool secondFrame) {
  const char (*sprite)[19] = secondFrame ? duckSpriteB : duckSpriteA;
  for (uint8_t row = 0; row < 18; row++) {
    for (uint8_t col = 0; col < 18; col++) {
      if (sprite[row][col] == '#') {
        oledSetLogicalPixel(x + col, y + row, true);
      }
    }
  }
}

void oledInit() {
  delay(100);
  oledCommand(0xAE);
  oledCommand(0xD5); oledCommand(0x80);
  oledCommand(0xA8); oledCommand(0x3F);
  oledCommand(0xD3); oledCommand(0x00);
  oledCommand(0x40);
  oledCommand(0x8D); oledCommand(0x14);
  oledCommand(0x20); oledCommand(0x02);
  oledCommand(0xA1);
  oledCommand(0xC8);
  oledCommand(0xDA); oledCommand(0x12);
  oledCommand(0x81); oledCommand(0xCF);
  oledCommand(0xD9); oledCommand(0xF1);
  oledCommand(0xDB); oledCommand(0x40);
  oledCommand(0xA4);
  oledCommand(0xA6);
  oledCommand(0xAF);
  oledClear();
  oledFlush();
}

uint8_t packDivisionSetting(uint8_t divisionIndex, uint8_t swingEnabled) {
  return SAVED_SWING_MARKER | (swingEnabled ? SAVED_SWING_ON : 0) | divisionIndex;
}

uint8_t unpackDivisionIndex(uint8_t savedDivision) {
  return savedDivision & SAVED_DIVISION_MASK;
}

uint8_t unpackSwingEnabled(uint8_t savedDivision) {
  if ((savedDivision & SAVED_SWING_MARKER) == 0) return 1;
  return (savedDivision & SAVED_SWING_ON) ? 1 : 0;
}

bool validSavedOutput(uint8_t savedDivision) {
  return unpackDivisionIndex(savedDivision) < NUM_DIVISIONS;
}

bool validSavedPattern(uint8_t mode, uint8_t index) {
  if (mode == PATTERN_NORMAL) return index == 0;
  if (mode == PATTERN_SLICER) return index < NUM_SLICER;
  if (mode == PATTERN_PROB) return index < NUM_PROB;
  return false;
}

SavedSettings captureSettings() {
  SavedSettings saved;
  saved.magic = SETTINGS_MAGIC;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    saved.divisionIndex[i] = packDivisionSetting(clockOutputs[i].divisionIndex, clockOutputs[i].swingEnabled);
    saved.patternMode[i] = (uint8_t)clockOutputs[i].patternMode;
    saved.patternIndex[i] = clockOutputs[i].patternIndex;
  }
  saved.swingIndex = swingIndex;
  saved.bpmTimes10 = (uint16_t)(bpm * 10.0 + 0.5);
  return saved;
}

void applySettings(const SavedSettings &saved) {
  if (saved.magic != SETTINGS_MAGIC) return;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    if (!validSavedOutput(saved.divisionIndex[i])) return;
    if (!validSavedPattern(saved.patternMode[i], saved.patternIndex[i])) return;
  }
  if (saved.swingIndex >= NUM_SWING) return;

  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    clockOutputs[i].divisionIndex = unpackDivisionIndex(saved.divisionIndex[i]);
    clockOutputs[i].patternMode = (PatternMode)saved.patternMode[i];
    clockOutputs[i].patternIndex = saved.patternIndex[i];
    clockOutputs[i].swingEnabled = unpackSwingEnabled(saved.divisionIndex[i]);
  }
  swingIndex = saved.swingIndex;
  if (saved.bpmTimes10 >= (uint16_t)(MIN_BPM * 10) && saved.bpmTimes10 <= (uint16_t)(MAX_BPM * 10)) {
    bpm = saved.bpmTimes10 / 10.0;
  }
  syncClock();
  displayDirty = true;
}

void loadSettings() {
  SavedSettings saved;
  EEPROM.get(EEPROM_ADDR, saved);
  applySettings(saved);
}

void saveSettings() {
  SavedSettings saved = captureSettings();
  EEPROM.put(EEPROM_ADDR, saved);
}

void scheduleSettingsSave() {
  settingsSavePending = true;
  settingsSaveDueMs = millis() + SETTINGS_SAVE_DELAY_MS;
}

void handleDeferredSettingsSave() {
  if (settingsSavePending && (int32_t)(millis() - settingsSaveDueMs) >= 0) {
    settingsSavePending = false;
    saveSettings();
  }
}

int presetAddress(uint8_t slot) {
  return EEPROM_ADDR + sizeof(SavedSettings) * (slot + 1);
}

void savePreset(uint8_t slot) {
  SavedSettings saved = captureSettings();
  EEPROM.put(presetAddress(slot), saved);
}

bool loadPreset(uint8_t slot) {
  SavedSettings saved;
  EEPROM.get(presetAddress(slot), saved);
  if (saved.magic != SETTINGS_MAGIC) return false;
  applySettings(saved);
  saveSettings();
  return true;
}

uint32_t beatIntervalUs() {
  return (uint32_t)(60000000.0 / bpm);
}

uint32_t intervalForOutput(uint8_t outputIndex) {
  uint32_t beat = beatIntervalUs();
  TimeDivisionPreset division = timeDivisions[clockOutputs[outputIndex].divisionIndex];
  return maxU32(1000, (beat * division.numerator) / division.denominator);
}

uint32_t pulseWidthUs(uint32_t intervalUs) {
  uint32_t width = intervalUs / 3;
  if (width > MAX_PULSE_WIDTH_US) width = MAX_PULSE_WIDTH_US;
  return maxU32(250, width);
}

uint32_t pulseWidthForOutput(uint8_t outputIndex, uint32_t intervalUs, uint32_t stepIndex) {
  return pulseWidthUs(intervalUs);
}

uint32_t swungIntervalForOutput(uint8_t outputIndex, uint32_t stepIndex) {
  uint32_t interval = intervalForOutput(outputIndex);
  uint8_t swingAmount = swingPresets[swingIndex];
  if (!clockOutputs[outputIndex].swingEnabled || swingAmount == 0) return interval;

  uint8_t swing = 50 + ((uint16_t)swingAmount * 16) / 100;
  uint16_t percent = (stepIndex % 2 == 0) ? (swing * 2) : ((100 - swing) * 2);
  return maxU32(1000, (interval * percent) / 100);
}

bool slicerHit(uint32_t stepIndex, uint16_t mask, uint8_t steps) {
  uint8_t step = stepIndex % steps;
  return (mask & (1U << step)) != 0;
}

bool shouldFirePattern(uint8_t outputIndex) {
  ClockOutput &out = clockOutputs[outputIndex];
  if (out.patternMode == PATTERN_NORMAL) return true;

  if (out.patternMode == PATTERN_SLICER) {
    SlicerPreset preset = slicerPresets[out.patternIndex];
    return slicerHit(outputStep[outputIndex], preset.mask, preset.steps);
  }

  if (out.patternMode == PATTERN_PROB) {
    return random(100) < probabilityPresets[out.patternIndex];
  }

  return true;
}

void allOutputsLow() {
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(outputPins[i], LOW);
    outputHigh[i] = false;
  }
}

void syncClock() {
  uint32_t nowUs = micros();
  uint32_t nowMs = millis();
  for (uint8_t i = 0; i < 4; i++) {
    nextPulseUs[i] = nowUs;
    outputOffUs[i] = nowUs;
    outputStep[i] = 0;
  }
  lastBeatMs = nowMs;
  nextBeatCueUs = nowUs;
  visualBeatStep = 0;
  allOutputsLow();
}

void toggleRun() {
  running = !running;
  syncClock();
  displayDirty = true;
  allowRunDisplayRefresh = running;
}

void handleClock() {
  uint32_t nowUs = micros();

  if (!running || screen != SCREEN_HOME) {
    allOutputsLow();
    return;
  }

  uint32_t beatUs = beatIntervalUs();
  while ((int32_t)(nowUs - nextBeatCueUs) >= 0) {
    lastBeatMs = millis();
    visualBeatStep++;
    nextBeatCueUs += beatUs;
  }

  for (uint8_t i = 0; i < 4; i++) {
    if (outputHigh[i] && (int32_t)(nowUs - outputOffUs[i]) >= 0) {
      digitalWrite(outputPins[i], LOW);
      outputHigh[i] = false;
    }

    uint32_t intervalUs = swungIntervalForOutput(i, outputStep[i]);
    while ((int32_t)(nowUs - nextPulseUs[i]) >= (int32_t)intervalUs) {
      nextPulseUs[i] += intervalUs;
      outputStep[i]++;
      intervalUs = swungIntervalForOutput(i, outputStep[i]);
    }

    if ((int32_t)(nowUs - nextPulseUs[i]) >= 0) {
      if (shouldFirePattern(i)) {
        digitalWrite(outputPins[i], HIGH);
        outputHigh[i] = true;
        lastPulseMs[i] = millis();
      }
      outputOffUs[i] = nextPulseUs[i] + pulseWidthForOutput(i, intervalUs, outputStep[i]);
      nextPulseUs[i] += intervalUs;
      outputStep[i]++;
    }
  }
}

void readEncoder() {
  uint8_t encoded = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
  uint8_t sum = (lastEncoded << 2) | encoded;

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
    encoderDelta++;
  } else if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
    encoderDelta--;
  }

  lastEncoded = encoded;
}

uint8_t currentValueCount() {
  return NUM_DIVISIONS;
}

uint8_t currentPatternValueCount() {
  if (pendingPatternMode == PATTERN_SLICER) return NUM_SLICER;
  if (pendingPatternMode == PATTERN_PROB) return NUM_PROB;
  return 1;
}

void handleEncoder() {
  readEncoder();

  int8_t step = 0;
  if (encoderDelta >= 4) {
    encoderDelta = 0;
    step = 1;
  } else if (encoderDelta <= -4) {
    encoderDelta = 0;
    step = -1;
  }

  if (step == 0) return;

  if (screen == SCREEN_HOME) {
    if (buttonState == LOW) {
      if (running) {
        int16_t next = (int16_t)swingIndex + step;
        while (next < 0) next += NUM_SWING;
        while (next >= NUM_SWING) next -= NUM_SWING;
        swingIndex = (uint8_t)next;
        syncClock();
        scheduleSettingsSave();
      } else {
        bpm += step * 10;
        if (bpm < MIN_BPM) bpm = MIN_BPM;
        if (bpm > MAX_BPM) bpm = MAX_BPM;
        scheduleSettingsSave();
      }
      buttonTurnHandled = true;
    } else {
      bpm += step;
      if (bpm < MIN_BPM) bpm = MIN_BPM;
      if (bpm > MAX_BPM) bpm = MAX_BPM;
      scheduleSettingsSave();
    }
  } else if (screen == SCREEN_PICK_ITEM) {
    int16_t next = (int16_t)selectedItem + step;
    while (next < 0) next += 5;
    while (next >= 5) next -= 5;
    selectedItem = (uint8_t)next;
  } else if (screen == SCREEN_PICK_EDIT) {
    int16_t next = (int16_t)selectedEdit + step;
    while (next < 0) next += 3;
    while (next >= 3) next -= 3;
    selectedEdit = (uint8_t)next;
  } else if (screen == SCREEN_PICK_VALUE) {
    uint8_t count = currentValueCount();
    int16_t next = (int16_t)pendingDivisionIndex + step;
    while (next < 0) next += count;
    while (next >= count) next -= count;
    pendingDivisionIndex = (uint8_t)next;
  } else if (screen == SCREEN_PICK_PATTERN_MODE) {
    int16_t next = (int16_t)pendingPatternMode + step;
    while (next < 0) next += 3;
    while (next >= 3) next -= 3;
    pendingPatternMode = (PatternMode)next;
    pendingPatternIndex = 0;
  } else if (screen == SCREEN_PICK_PATTERN_VALUE) {
    uint8_t count = currentPatternValueCount();
    int16_t next = (int16_t)pendingPatternIndex + step;
    while (next < 0) next += count;
    while (next >= count) next -= count;
    pendingPatternIndex = (uint8_t)next;
  } else if (screen == SCREEN_PICK_SWING) {
    pendingSwingEnabled = pendingSwingEnabled ? 0 : 1;
  } else if (screen == SCREEN_PICK_PRESET_SLOT) {
    int16_t next = (int16_t)selectedPresetSlot + step;
    while (next < 0) next += NUM_PRESETS;
    while (next >= NUM_PRESETS) next -= NUM_PRESETS;
    selectedPresetSlot = (uint8_t)next;
  } else if (screen == SCREEN_PICK_PRESET_ACTION) {
    selectedPresetAction = (selectedPresetAction + 1) % 2;
  }

  displayDirty = true;
}

void enterSettings() {
  running = false;
  allOutputsLow();
  selectedItem = 0;
  selectedOutput = 0;
  selectedEdit = 0;
  screen = SCREEN_PICK_ITEM;
  displayDirty = true;
  allowRunDisplayRefresh = false;
}

void returnHome() {
  screen = SCREEN_HOME;
  displayDirty = true;
  allowRunDisplayRefresh = false;
}

void returnMainMenu() {
  screen = SCREEN_PICK_ITEM;
  displayDirty = true;
}

void finishFinalMenuSelection() {
  returnMainMenu();
}

void returnPreviousScreen() {
  if (screen == SCREEN_PICK_ITEM) {
    returnHome();
  } else if (screen == SCREEN_PICK_EDIT) {
    screen = SCREEN_PICK_ITEM;
    displayDirty = true;
  } else if (screen == SCREEN_PICK_VALUE || screen == SCREEN_PICK_PATTERN_MODE) {
    screen = SCREEN_PICK_EDIT;
    displayDirty = true;
  } else if (screen == SCREEN_PICK_PATTERN_VALUE) {
    screen = SCREEN_PICK_PATTERN_MODE;
    displayDirty = true;
  } else if (screen == SCREEN_PICK_SWING) {
    screen = SCREEN_PICK_EDIT;
    displayDirty = true;
  } else if (screen == SCREEN_PICK_PRESET_ACTION) {
    screen = SCREEN_PICK_ITEM;
    displayDirty = true;
  } else if (screen == SCREEN_PICK_PRESET_SLOT) {
    screen = SCREEN_PICK_PRESET_ACTION;
    displayDirty = true;
  } else {
    returnHome();
  }
}

void handleShortPress() {
  if (screen == SCREEN_HOME) {
    toggleRun();
    return;
  }

  if (screen == SCREEN_PICK_ITEM) {
    if (selectedItem < NUM_OUTPUTS) {
      selectedOutput = selectedItem;
      selectedEdit = 0;
      screen = SCREEN_PICK_EDIT;
    } else {
      selectedPresetSlot = 0;
      selectedPresetAction = 0;
      screen = SCREEN_PICK_PRESET_ACTION;
    }
    displayDirty = true;
  } else if (screen == SCREEN_PICK_EDIT) {
    if (selectedEdit == 0) {
      pendingDivisionIndex = clockOutputs[selectedOutput].divisionIndex;
      screen = SCREEN_PICK_VALUE;
    } else if (selectedEdit == 1) {
      pendingPatternMode = clockOutputs[selectedOutput].patternMode;
      pendingPatternIndex = clockOutputs[selectedOutput].patternIndex;
      screen = SCREEN_PICK_PATTERN_MODE;
    } else {
      pendingSwingEnabled = clockOutputs[selectedOutput].swingEnabled;
      screen = SCREEN_PICK_SWING;
    }
    displayDirty = true;
  } else if (screen == SCREEN_PICK_VALUE) {
    clockOutputs[selectedOutput].divisionIndex = pendingDivisionIndex;
    saveSettings();
    syncClock();
    finishFinalMenuSelection();
  } else if (screen == SCREEN_PICK_PATTERN_MODE) {
    if (pendingPatternMode == PATTERN_NORMAL) {
      clockOutputs[selectedOutput].patternMode = pendingPatternMode;
      clockOutputs[selectedOutput].patternIndex = 0;
      saveSettings();
      syncClock();
      finishFinalMenuSelection();
    } else {
      pendingPatternIndex = 0;
      screen = SCREEN_PICK_PATTERN_VALUE;
      displayDirty = true;
    }
  } else if (screen == SCREEN_PICK_PATTERN_VALUE) {
    clockOutputs[selectedOutput].patternMode = pendingPatternMode;
    clockOutputs[selectedOutput].patternIndex = pendingPatternIndex;
    saveSettings();
    syncClock();
    finishFinalMenuSelection();
  } else if (screen == SCREEN_PICK_SWING) {
    clockOutputs[selectedOutput].swingEnabled = pendingSwingEnabled;
    saveSettings();
    syncClock();
    finishFinalMenuSelection();
  } else if (screen == SCREEN_PICK_PRESET_ACTION) {
    selectedPresetSlot = 0;
    screen = SCREEN_PICK_PRESET_SLOT;
    displayDirty = true;
  } else if (screen == SCREEN_PICK_PRESET_SLOT) {
    if (selectedPresetAction == 0) {
      loadPreset(selectedPresetSlot);
    } else {
      savePreset(selectedPresetSlot);
      saveSettings();
    }
    finishFinalMenuSelection();
  }
}

void handleLongPress() {
  if (screen == SCREEN_HOME) {
    enterSettings();
  } else {
    returnPreviousScreen();
  }
}

void handleButton() {
  bool reading = digitalRead(ENC_SW);
  uint32_t nowMs = millis();

  if (reading != lastButtonReading) {
    lastDebounceMs = nowMs;
  }

  if (nowMs - lastDebounceMs > 25 && reading != buttonState) {
    buttonState = reading;
    if (buttonState == LOW) {
      buttonDownMs = nowMs;
      longPressHandled = false;
      buttonTurnHandled = false;
    } else {
      if (buttonTurnHandled) {
        settingsSavePending = false;
        saveSettings();
      } else if (!longPressHandled) {
        handleShortPress();
      }
    }
  }

  if (buttonState == LOW && !longPressHandled && !buttonTurnHandled && (nowMs - buttonDownMs) >= LONG_PRESS_MS) {
    longPressHandled = true;
    handleLongPress();
  }

  lastButtonReading = reading;
}

char outputName(uint8_t outputIndex) {
  return (char)('A' + outputIndex);
}

void formatDivisionLabel(uint8_t outputIndex, char *buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "%s", timeDivisions[clockOutputs[outputIndex].divisionIndex].label);
}

void formatPatternLabel(uint8_t outputIndex, char *buffer, size_t bufferSize) {
  ClockOutput &out = clockOutputs[outputIndex];

  if (out.patternMode == PATTERN_NORMAL) {
    snprintf(buffer, bufferSize, "N");
  } else if (out.patternMode == PATTERN_SLICER) {
    snprintf(buffer, bufferSize, "S%u", out.patternIndex + 1);
  } else {
    snprintf(buffer, bufferSize, "P%u", probabilityPresets[out.patternIndex]);
  }
}

void formatPendingPattern(char *buffer, size_t bufferSize) {
  if (pendingPatternMode == PATTERN_NORMAL) {
    snprintf(buffer, bufferSize, "NORMAL");
  } else if (pendingPatternMode == PATTERN_SLICER) {
    snprintf(buffer, bufferSize, "SLICER");
  } else {
    snprintf(buffer, bufferSize, "PROB");
  }
}

void formatPendingPatternValue(char *buffer, size_t bufferSize) {
  if (pendingPatternMode == PATTERN_SLICER) {
    snprintf(buffer, bufferSize, "%s", slicerPresets[pendingPatternIndex].label);
  } else if (pendingPatternMode == PATTERN_PROB) {
    snprintf(buffer, bufferSize, "%u%%", probabilityPresets[pendingPatternIndex]);
  } else {
    snprintf(buffer, bufferSize, "NORMAL");
  }
}

void formatPendingValue(char *buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "%s", timeDivisions[pendingDivisionIndex].label);
}

void oledFillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  for (uint8_t yy = 0; yy < h; yy++) {
    for (uint8_t xx = 0; xx < w; xx++) {
      oledSetLogicalPixel(x + xx, y + yy, true);
    }
  }
}

void oledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  for (uint8_t xx = 0; xx < w; xx++) {
    oledSetLogicalPixel(x + xx, y, true);
    oledSetLogicalPixel(x + xx, y + h - 1, true);
  }
  for (uint8_t yy = 0; yy < h; yy++) {
    oledSetLogicalPixel(x, y + yy, true);
    oledSetLogicalPixel(x + w - 1, y + yy, true);
  }
}

void updateDisplay() {
  uint32_t nowMs = millis();
  if (running) {
    if (!displayDirty && nowMs - lastDisplayMs < 220) return;
  } else if (!displayDirty && nowMs - lastDisplayMs < 250) {
    return;
  }
  lastDisplayMs = nowMs;
  displayDirty = false;
  allowRunDisplayRefresh = false;

  char line[18];
  char divLabel[8];
  char patLabel[8];
  oledClear();

  if (screen == SCREEN_HOME) {
    snprintf(line, sizeof(line), "%3d", (int)(bpm + 0.5));
    oledDrawCenteredStringPx(18, line, 2);

    bool beatPulse = running && (nowMs - lastBeatMs < 120);
    bool secondFrame = running && ((visualBeatStep & 1) == 1);

    oledDrawDuckFrame(23, beatPulse ? 36 : 39, secondFrame);
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
      formatDivisionLabel(i, divLabel, sizeof(divLabel));
      formatPatternLabel(i, patLabel, sizeof(patLabel));
      snprintf(line, sizeof(line), "%c%s %s%c", outputName(i), divLabel, patLabel, clockOutputs[i].swingEnabled ? 'S' : '-');
      oledDrawCenteredStringPx(68 + i * 11, line, 1);
    }
    snprintf(line, sizeof(line), "SW%u", swingPresets[swingIndex]);
    oledDrawCenteredStringPx(112, line, 1);
  } else if (screen == SCREEN_PICK_ITEM) {
    oledDrawCenteredStringPx(20, "OUTPUT", 1);
    if (selectedItem < NUM_OUTPUTS) snprintf(line, sizeof(line), "%c", outputName(selectedItem));
    else snprintf(line, sizeof(line), "PRE");
    oledDrawCenteredStringPx(52, line, 2);
    oledDrawCenteredStringPx(92, "PUSH OK", 1);
  } else if (screen == SCREEN_PICK_EDIT) {
    snprintf(line, sizeof(line), "EDIT %c", outputName(selectedOutput));
    oledDrawCenteredStringPx(20, line, 1);
    if (selectedEdit == 0) snprintf(line, sizeof(line), "TIME");
    else if (selectedEdit == 1) snprintf(line, sizeof(line), "PATT");
    else snprintf(line, sizeof(line), "SWING");
    oledDrawCenteredStringPx(52, line, 2);
    oledDrawCenteredStringPx(92, "PUSH OK", 1);
  } else if (screen == SCREEN_PICK_VALUE) {
    snprintf(line, sizeof(line), "EDIT %c", outputName(selectedOutput));
    oledDrawCenteredStringPx(20, line, 1);
    formatPendingValue(line, sizeof(line));
    oledDrawCenteredStringPx(52, line, 2);
    oledDrawCenteredStringPx(92, "PUSH SAVE", 1);
  } else if (screen == SCREEN_PICK_PATTERN_MODE) {
    snprintf(line, sizeof(line), "EDIT %c", outputName(selectedOutput));
    oledDrawCenteredStringPx(20, line, 1);
    formatPendingPattern(line, sizeof(line));
    oledDrawCenteredStringPx(52, line, strlen(line) > 5 ? 1 : 2);
    oledDrawCenteredStringPx(92, "PUSH OK", 1);
  } else if (screen == SCREEN_PICK_PATTERN_VALUE) {
    snprintf(line, sizeof(line), "EDIT %c", outputName(selectedOutput));
    oledDrawCenteredStringPx(20, line, 1);
    formatPendingPatternValue(line, sizeof(line));
    oledDrawCenteredStringPx(52, line, strlen(line) > 4 ? 1 : 2);
    oledDrawCenteredStringPx(92, "PUSH SAVE", 1);
  } else if (screen == SCREEN_PICK_SWING) {
    snprintf(line, sizeof(line), "EDIT %c", outputName(selectedOutput));
    oledDrawCenteredStringPx(20, line, 1);
    oledDrawCenteredStringPx(52, pendingSwingEnabled ? "ON" : "OFF", 2);
    oledDrawCenteredStringPx(92, "PUSH SAVE", 1);
  } else if (screen == SCREEN_PICK_PRESET_ACTION) {
    oledDrawCenteredStringPx(20, "PRESET", 1);
    oledDrawCenteredStringPx(52, selectedPresetAction == 0 ? "LOAD" : "SAVE", 2);
    oledDrawCenteredStringPx(92, "PUSH OK", 1);
  } else if (screen == SCREEN_PICK_PRESET_SLOT) {
    oledDrawCenteredStringPx(20, selectedPresetAction == 0 ? "LOAD" : "SAVE", 1);
    snprintf(line, sizeof(line), "SLOT %u", selectedPresetSlot + 1);
    oledDrawCenteredStringPx(52, line, 1);
    oledDrawCenteredStringPx(92, "PUSH SAVE", 1);
  }

  oledFlush();
}

void setup() {
  pinMode(BUF_EN, OUTPUT);
  digitalWrite(BUF_EN, HIGH);

  pinMode(OUT_A, OUTPUT);
  pinMode(OUT_B, OUTPUT);
  pinMode(OUT_C, OUTPUT);
  pinMode(OUT_D, OUTPUT);
  allOutputsLow();

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  lastEncoded = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);

  loadSettings();
  randomSeed(micros());

  Wire.begin();
  Wire.setClock(400000);
  oledInit();

  digitalWrite(BUF_EN, LOW);
  displayDirty = true;
}

void loop() {
  handleEncoder();
  handleButton();
  handleClock();
  updateDisplay();
  handleDeferredSettingsSave();
}
