#include "EepromManager.h"
#include <Wire.h>
#include "InterlockManager.h"
#include "MenuState.h"
#include "ButtonManager.h"   // pollButtons()
#include "EncoderManager.h"  // pollEncoder()
#include "AuxManager.h"      // auxTick()

// ───── Internal Helpers ─────
uint8_t eepromRead(uint32_t addr) {
  uint8_t device = EEPROM_BASE_ADDR + ((addr >> 16) & 0x03);  // select bank
  uint16_t wordAddr = addr & 0xFFFF;

  Wire.beginTransmission(device);
  Wire.write((wordAddr >> 8) & 0xFF);       // MSB
  Wire.write(wordAddr & 0xFF);              // LSB
  if (Wire.endTransmission(false) != 0) {
    Serial.println("[EEPROM] Read: address NACK");
    return 0xFF;
  }

  Wire.requestFrom(device, (uint8_t)1);
  if (!Wire.available()) {
    Serial.println("[EEPROM] Read: no data received");
    return 0xFF;
  }

  return Wire.read();
}

void eepromWrite(uint32_t addr, uint8_t data) {
  uint8_t device = EEPROM_BASE_ADDR + ((addr >> 16) & 0x03);
  uint16_t wordAddr = addr & 0xFFFF;

  Wire.beginTransmission(device);
  Wire.write((wordAddr >> 8) & 0xFF);
  Wire.write(wordAddr & 0xFF);
  Wire.write(data);
  if (Wire.endTransmission() != 0) {
    Serial.println("[EEPROM] Write: address NACK");
  }

  delay(6);  // mandatory EEPROM write delay
}

void eepromChipErase()
{
  for (uint32_t a = 0; a < EEPROM_TOTAL_SIZE; a += EEPROM_PAGE_SIZE)
  {
      for (uint16_t i = 0; i < EEPROM_PAGE_SIZE; ++i)
          eepromWrite(a + i, 0xFF);
      delay(30);                // let I²C settle / yield to USB
      pollButtons();           // keep UI responsive
      pollEncoder();           //  –– » ––
      auxTick();               //
      delay(30);                // let I²C settle / yield to USB
      loadOverviewSettings();
  }
}

// ───── Public Interface ─────
void initEeprom() {
  Wire.begin();
}

void saveOverviewSettings() {
  Serial.println("[EEPROM] Saving overview settings...");

  // Write validity flag
  eepromWrite(OVERVIEW_CONFIG_ADDR, OVERVIEW_VALID_FLAG);

  // Write state for each of the 8 interlocks (change if you need 9)
  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t state = 0;  // default = input

    if (isSimulated(interlocks[i].port, interlocks[i].bit)) {
      bool out = (readOutputRegister(interlocks[i].port) >> interlocks[i].bit) & 1;
      state = out ? 2 : 1;
    }

    eepromWrite(OVERVIEW_CONFIG_ADDR + 1 + i, state);
    Serial.print("[EEPROM] Stored item "); Serial.print(i);
    Serial.print(" with state "); Serial.println(state);
  }
}

void loadOverviewSettings() {
  Serial.println("[EEPROM] Loading overview settings...");

  uint8_t flag = eepromRead(OVERVIEW_CONFIG_ADDR);
  Serial.print("[EEPROM] Read flag: 0x");
  Serial.println(flag, HEX);

  if (flag != OVERVIEW_VALID_FLAG) {
    Serial.println("[EEPROM] No valid config found. Skipping load.");
    return;
  }

  Serial.println("[EEPROM] Valid config found. Applying states...");

  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t state = eepromRead(OVERVIEW_CONFIG_ADDR + 1 + i);
    Serial.print("[EEPROM] Item "); Serial.print(i);
    Serial.print(" → EEPROM state: "); Serial.println(state);

    applyEditStateToItem(i, state);
  }
}
/* ───── Aux-tab autoreset persistence ───── */
void saveAuxSettings()
{
    eepromWrite(AUX_CONFIG_ADDR, AUX_VALID_FLAG);
    eepromWrite(AUX_CONFIG_ADDR + 1, auxState.autoResetEnable ? 1 : 0);

    uint16_t ms = auxState.autoResetDelay;        // 0-1000 ms
    eepromWrite(AUX_CONFIG_ADDR + 2, ms & 0xFF);  // LSB
    eepromWrite(AUX_CONFIG_ADDR + 3, ms >> 8);    // MSB
}

void loadAuxSettings()
{
    if (eepromRead(AUX_CONFIG_ADDR) != AUX_VALID_FLAG) return;   // nothing saved

    auxState.autoResetEnable = eepromRead(AUX_CONFIG_ADDR + 1) != 0;

    uint16_t lsb = eepromRead(AUX_CONFIG_ADDR + 2);
    uint16_t msb = eepromRead(AUX_CONFIG_ADDR + 3);
    auxState.autoResetDelay  = (msb << 8) | lsb;
}

