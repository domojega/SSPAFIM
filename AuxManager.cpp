/* ---------- AuxManager.cpp (full file) ---------- */
#include "AuxManager.h"
#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>                   // snprintf
#include "ST7365P_Display.h"         // TFT driver
#include "MenuState.h"
#include "DisplayManager.h"          // tft & colour constants
#include "ButtonManager.h"
#include "EncoderManager.h"
#include "InterlockManager.h"
#include "EepromManager.h"

extern ST7365P_Display tft;          // single global instance (defined in
                                     // DisplayManager.cpp)

/* ───── RT4527A back-light driver ───── */
static constexpr uint8_t RT_A0_LOW  = 0x36;
static constexpr uint8_t RT_A0_HIGH = 0x37;
static uint8_t rtAddr = 0;           // filled by detectRt()

static bool rtWrite(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(rtAddr);
    Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}
static bool detectRt()
{
    const uint8_t addr[2] = { RT_A0_LOW, RT_A0_HIGH };

    for (uint8_t i = 0; i < 2; ++i) {
        Wire.beginTransmission(addr[i]);
        if (Wire.endTransmission() == 0) {
            rtAddr = addr[i];
            return true;             // device found
        }
    }
    return false;                     // neither answered
}
static void rtSetDC()                     { rtWrite(0x00, 0x01); } // DC mode
static void rtSetBrightness(uint8_t code) { rtWrite(0x01, code); }

/* ───── persistent AUX state ───── */
AuxState auxState = {
    .lcdBrightness   = 128,
    .autoResetEnable = false,
    .autoResetDelay  = 500,
    .editMode        = AUX_EDIT_NONE
};

/* ───── autoreset background tick ───── */
static bool     arPending = false;
static uint32_t arStartMs = 0;

void auxTick()
{
    if (!auxState.autoResetEnable) return;

    bool giFault = !readInterlock(1, 2);   // P10 LOW = fault
    if (giFault && !arPending) { arPending = true; arStartMs = millis(); }

    if (arPending && millis() - arStartMs >= auxState.autoResetDelay) {
        sendResetPulse();                  // 500 ms pulse on P7 & P14
        arPending = false;
    }
}

/* ───── line renderer for AUX tab (used by DisplayManager) ───── */
void redrawAuxRow(uint8_t idx)      // keep extern linkage!
{
    const uint16_t y   = 30 + idx * 24;
    const bool     sel = (idx == menuState.selectedItem);

    tft.fillRect(0, y, 480, 24, sel ? COLOR_SELECTED_BG : COLOR_BLACK);
    tft.setCursor(2, y + 6);
    tft.setTextSize(2);
    tft.setTextColor(sel ? COLOR_YELLOW : COLOR_WHITE);

    switch (idx) {
        case AUX_LCD_BRIGHTNESS:
            tft.print(F("LCD brightness: "));
            tft.print(auxState.lcdBrightness);
            break;

        case AUX_INTERNAL_TEST:
            tft.print(F("Internal test  (long OK)"));
            break;

        case AUX_EEPROM_FORMAT:
            tft.print(F("EEPROM format  (long OK)"));
            break;

        case AUX_AUTO_RESET:
            tft.print(F("Autoreset: "));
            tft.print(auxState.autoResetEnable ? F("ON ") : F("OFF"));
            tft.print(F("  t="));
            tft.print(auxState.autoResetDelay);
            tft.print(F(" ms"));
            break;

        case AUX_POWER_LUT:
            tft.print(F("Power LUT  (locked)"));
            break;
    }
}
static void redrawAuxList() { for (uint8_t i = 0; i < AUX_COUNT; ++i) redrawAuxRow(i); }

/* ───── public init ───── */
void auxInit()
{
    Wire.begin();
    if (detectRt()) rtSetDC();
    rtSetBrightness(auxState.lcdBrightness);
}

/* ───── encoder handler ───── */
void auxEncoder(int8_t delta)
{
    switch (menuState.selectedItem)
    {
        case AUX_LCD_BRIGHTNESS:
            auxState.lcdBrightness = constrain(auxState.lcdBrightness + delta,
                                               0, 255);
            rtSetBrightness(auxState.lcdBrightness);
            redrawAuxRow(AUX_LCD_BRIGHTNESS);
            break;

        case AUX_AUTO_RESET:
            if (auxState.editMode == AUX_EDIT_BYTE) {
                auxState.autoResetDelay =
                    constrain(auxState.autoResetDelay + delta * 10, 0, 1000);
                redrawAuxRow(AUX_AUTO_RESET);
            }
            break;

        default: break;
    }
}

/* ───── short OK ───── */
void auxHandleShort()
{
    if (menuState.selectedItem == AUX_AUTO_RESET &&
        auxState.editMode     == AUX_EDIT_BYTE)
    {
        auxState.editMode = AUX_EDIT_NONE;
        updateEditIndicator(false);
        redrawAuxRow(AUX_AUTO_RESET);
    }
}

/* ───── long OK ───── */
void auxHandleLong()
{
    switch (menuState.selectedItem)
    {
        /* 1) device probe ------------------------------------- */
        case AUX_INTERNAL_TEST: {
            tft.fillRect(0, 30, 480, 242, COLOR_BLACK);
            tft.setTextSize(2); tft.setTextColor(COLOR_WHITE);
            tft.setCursor(4, 34);

            struct Dev { const char* n; uint8_t a; };
            const Dev devs[] = {
                { "TCA9555 MCU", 0x20 }, { "ADC PMOP",   0x21 },
                { "ADC RFOPD",   0x22 }, { "VR PMOP",    0x28 },
                { "VR RFOPD",    0x2B }, { "RT4527A MB", 0x36 },
                { "EEPROM MCU",  0x50 }
            };

            char line[40];
            for (auto& d : devs) {
                Wire.beginTransmission(d.a);
                bool ok = Wire.endTransmission() == 0;
                tft.setTextColor(ok ? COLOR_GREEN : COLOR_RED);
                snprintf(line, sizeof(line), "%-12s @0x%02X %s",
                         d.n, d.a, ok ? "OK" : "MISSING");
                tft.print(line); tft.print('\n');
            }

            /* crude SPI-flash probe */
            pinMode(7, OUTPUT); digitalWrite(7, LOW); delayMicroseconds(2);
            bool spiOk = (digitalRead(7) == LOW);
            digitalWrite(7, HIGH); pinMode(7, INPUT_PULLUP);

            tft.setTextColor(spiOk ? COLOR_GREEN : COLOR_RED);
            tft.print(F("SPI-Flash       "));
            tft.print(spiOk ? F("OK") : F("MISSING"));

            /* 10-s wait with UI alive */
            uint32_t t0 = millis();
            while (millis() - t0 < 10000) {
                pollButtons(); pollEncoder(); auxTick();
            }
            redrawAuxList();
            break;
        }

        /* 2) EEPROM format ------------------------------------ */
        case AUX_EEPROM_FORMAT: {
            tft.fillRect(100, 120, 280, 40, COLOR_BLACK);
            tft.drawRect(100, 120, 280, 40, COLOR_WHITE);
            tft.setCursor(110, 130);
            tft.setTextSize(2); tft.setTextColor(COLOR_WHITE);
            tft.print(F("Formatting …"));
            eepromChipErase();
            delay(600);
            redrawAuxList();
            break;
        }

        /* 3) autoreset toggle / enter delay edit -------------- */
        case AUX_AUTO_RESET:
            if (auxState.editMode == AUX_EDIT_NONE) {
                auxState.autoResetEnable = !auxState.autoResetEnable;
                if (auxState.autoResetEnable) {
                    auxState.editMode = AUX_EDIT_BYTE;
                    updateEditIndicator(true);
                }
            } else {
                auxState.editMode = AUX_EDIT_NONE;
                updateEditIndicator(false);
            }
            redrawAuxRow(AUX_AUTO_RESET);
            break;

        default: break;
    }
}
/* ---------------- end of file ---------------- */
