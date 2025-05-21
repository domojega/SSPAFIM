#ifndef AUX_MANAGER_H
#define AUX_MANAGER_H

#include <Arduino.h>

/* ────────────────────────────────────────────
   AUX-tab row indices                         */
enum AuxIdx : uint8_t {
    AUX_LCD_BRIGHTNESS = 0,
    AUX_INTERNAL_TEST,
    AUX_EEPROM_FORMAT,
    AUX_AUTO_RESET,
    AUX_POWER_LUT,
    AUX_COUNT
};

/* inline-edit sub-modes (only BYTE for now)  */
enum AuxEditMode : uint8_t { AUX_EDIT_NONE = 0, AUX_EDIT_BYTE };

/* ────────────────────────────────────────────
   Settings that must survive while the UI
   is running.  (Persist to EEPROM later.)     */
struct AuxState {
    uint8_t     lcdBrightness;     // 0-255 RT4527A DAC code
    bool        autoResetEnable;   // ON/OFF
    uint16_t    autoResetDelay;    // 0-1000 ms
    AuxEditMode editMode;          // current in-row mode
};

/* global instance (defined in AuxManager.cpp) */
extern AuxState auxState;

/* public API used by main sketch / managers  */
void auxInit();               // call from setup()
void auxTick();               // call every loop()
void auxEncoder(int8_t d);    // rotary delta
void auxHandleShort();        // short OK
void auxHandleLong();         // long  OK

/* helper used by DisplayManager.cpp          */
void redrawAuxRow(uint8_t idx);

#endif   /* AUX_MANAGER_H */
