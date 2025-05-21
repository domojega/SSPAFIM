#include "InterlockManager.h"
#include <Wire.h>
#include "MenuState.h" // for color constants

// ───── TCA9555 Register Definitions ─────
#define TCA_ADDR        0x20

#define REG_INPUT0      0x00
#define REG_INPUT1      0x01
#define REG_OUTPUT0     0x02
#define REG_OUTPUT1     0x03
#define REG_POLARITY0   0x04
#define REG_POLARITY1   0x05
#define REG_CONFIG0     0x06
#define REG_CONFIG1     0x07

// ───── Internal I2C Helpers ─────
void tcaWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t tcaRead(uint8_t reg) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(TCA_ADDR, (uint8_t)1);
  return Wire.read();
}

// ───── Port I/O Controls ─────
void tcaDir(uint8_t port, uint8_t bit, bool input) {
  uint8_t cfg = tcaRead(REG_CONFIG0 + port);
  if (input) cfg |= (1 << bit);
  else       cfg &= ~(1 << bit);
  tcaWrite(REG_CONFIG0 + port, cfg);
}

void tcaOut(uint8_t port, uint8_t bit, bool high) {
  uint8_t out = tcaRead(REG_OUTPUT0 + port);
  if (high) out |= (1 << bit);
  else      out &= ~(1 << bit);
  tcaWrite(REG_OUTPUT0 + port, out);
}

// ───── Public API ─────
InterlockItem interlocks[9] = {
  {"ΔPhase",    0, 0, true},
  {"Overduty",  0, 1, true},
  {"ΔMag",      0, 2, true},
  {"Overpower", 0, 3, true},
  {"User",      0, 4, true},
  {"PSS",       0, 5, true},
  {"External",  0, 6, true},
  {"Global",    1, 2, true},
  {"Reset",     0, 7, false}
};

void initInterlocks() {
  tcaWrite(REG_CONFIG0, 0xFF);     // all inputs
  tcaWrite(REG_CONFIG1, 0xFF);
  tcaWrite(REG_POLARITY0, 0x00);   // normal polarity
  tcaWrite(REG_POLARITY1, 0x00);
}

bool readInterlock(uint8_t port, uint8_t bit) {
  uint8_t reg = (port == 0) ? REG_INPUT0 : REG_INPUT1;
  return ((tcaRead(reg) >> bit) & 1) == 0;  // Active LOW = ON
}

bool isSimulated(uint8_t port, uint8_t bit) {
  uint8_t cfg = tcaRead(REG_CONFIG0 + port);
  return ((cfg >> bit) & 1) == 0;  // Output = simulated
}

void setSimulated(uint8_t port, uint8_t bit, bool state) {
  tcaDir(port, bit, false);
  tcaOut(port, bit, !state);  // false = LOW = ON
}

void toggleSimulated(uint8_t port, uint8_t bit) {
  uint8_t out = tcaRead(REG_OUTPUT0 + port);
  setSimulated(port, bit, ((out >> bit) & 1));
}

void sendResetPulse() {
  // P7 = port 0, bit 7
  // P14 = port 1, bit 6

  tcaDir(0, 7, false);   // Set P7 as OUTPUT
  tcaOut(0, 7, true);    // Set P7 HIGH
  tcaDir(1, 6, false);   // Set P14 as OUTPUT
  tcaOut(1, 6, true);    // Set P14 HIGH
  delay(500);            // Hold for 500ms
  tcaDir(0, 7, true);    // Set P7 back to INPUT (Hi-Z)
  tcaDir(1, 6, true);    // Set P14 back to INPUT
}

void applyEditStateToItem(uint8_t itemIndex, uint8_t state) {
  if (itemIndex >= 9) return;
  auto& it = interlocks[itemIndex];
  if (!it.allowSim) return;

  switch (state) {
    case 0:  // Input
      tcaDir(it.port, it.bit, true);
      break;
    case 1:  // Output LOW (Sim ON)
      tcaDir(it.port, it.bit, false);
      tcaOut(it.port, it.bit, false);
      break;
    case 2:  // Output HIGH (Sim OFF)
      tcaDir(it.port, it.bit, false);
      tcaOut(it.port, it.bit, true);
      break;
  }
  //if (state == 0)       tcaDir(it.port, it.bit, true);   // back to input
//else                  /* keep as output */;
}

uint8_t readOutputRegister(uint8_t port) {
  return tcaRead(REG_OUTPUT0 + port);
}

uint16_t getStatusColor(uint8_t idx)
{
  if (idx >= 9) return COLOR_GRAY;

  const auto& it = interlocks[idx];
  bool sim       = isSimulated(it.port,it.bit);

  if (sim)                       // simulated → yellow ring
  {
      bool outHigh = (readOutputRegister(it.port)>>it.bit)&1;
      return outHigh ? (COLOR_GREEN|COLOR_YELLOW)
                     : (COLOR_RED  |COLOR_YELLOW);
  }
  else                           // real input (active LOW)
  {
      bool active = readInterlock(it.port,it.bit);  // LOW→true
      return active ? COLOR_RED : COLOR_GREEN;      // ← fixed
  }
}
