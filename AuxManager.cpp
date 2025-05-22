/* ───── AuxManager.cpp (full) ─────────────────────────────────────────── */
#include "AuxManager.h"
#include <Arduino.h>
#include <Wire.h>

#include "MenuState.h"
#include "DisplayManager.h"      // tft, colours, updateEditIndicator()
#include "InterlockManager.h"
#include "ButtonManager.h"       // pollButtons() for wait loops
#include "EepromManager.h"

#include "ST7365P_Display.h"
extern ST7365P_Display tft;

/* ===================================================================== */
/*  RT4527A back-light driver                                            */
/* ===================================================================== */
static constexpr uint8_t RT_ADDR0 = 0x36;   // A0-strap low
static constexpr uint8_t RT_ADDR1 = 0x37;   // A0-strap high
static uint8_t rtAddr = RT_ADDR0;           // filled by detectRt()

static bool rtWrite(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(rtAddr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}
static bool detectRt()
{
    const uint8_t list[2] = { RT_ADDR0, RT_ADDR1 };
    for (uint8_t a : list)
    {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) { rtAddr = a; return true; }
    }
    return false;
}
static void rtSetDcMode()           { rtWrite(0x00, 0x01); } // DC-dimming
static void rtSetBrightness(uint8_t c) { rtWrite(0x01, c); }

/* ===================================================================== */
/*  Persistent AUX state                                                 */
/* ===================================================================== */
AuxState auxState = { 128, false, 500, AUX_EDIT_NONE };

/* ===================================================================== */
/*  Auto-reset background tick                                           */
/* ===================================================================== */
static bool     arPending = false;
static uint32_t arStartMs = 0;

void auxTick()
{
    if (!auxState.autoResetEnable) return;

    bool giLow = readInterlock(1, 2);               // P10 LOW = tripped
    if (giLow && !arPending) { arPending = true; arStartMs = millis(); }

    if (arPending && millis() - arStartMs >= auxState.autoResetDelay) {
        sendResetPulse();
        arPending = false;
    }
}

/* ===================================================================== */
/*  Line redraw helpers (called from DisplayManager)                     */
/* ===================================================================== */
void redrawAuxRow(uint8_t idx)   /* declaration lives in AuxManager.h   */
{
    const uint16_t y   = 30 + idx * 24;
    const bool     sel = (idx == menuState.selectedItem);

    tft.fillRect(0, y, 480, 24, sel ? COLOR_SELECTED_BG : COLOR_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(sel ? COLOR_YELLOW : COLOR_WHITE);
    tft.setCursor(2, y + 6);

    switch (idx)
    {
        case AUX_LCD_BRIGHTNESS:
            tft.print(F("LCD brightness: "));
            tft.print(auxState.lcdBrightness);
            break;

        case AUX_INTERNAL_TEST:
            tft.print(F("Internal test  (long OK)"));
            break;

        case AUX_EEPROM_FORMAT:
            tft.print(F("EEPROM format   (long OK)"));
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

static void redrawAuxList()
{
    for (uint8_t i = 0; i < AUX_COUNT; ++i) redrawAuxRow(i);
}

/* ===================================================================== */
/*  Public init                                                          */
/* ===================================================================== */
void auxInit()
{
    Wire.begin();                         // ensure I²C is up
    if (detectRt())  rtSetDcMode();       // ignore PWM pin
    rtSetBrightness(auxState.lcdBrightness);
}

/* ===================================================================== */
/*  Encoder handler                                                      */
/* ===================================================================== */
void auxEncoder(int8_t delta)
{
    switch (menuState.selectedItem)
    {
        /* live 0-255 back-light code */
        case AUX_LCD_BRIGHTNESS:
            auxState.lcdBrightness = constrain(auxState.lcdBrightness + delta,
                                               0, 255);
            rtSetBrightness(auxState.lcdBrightness);
            redrawAuxRow(AUX_LCD_BRIGHTNESS);
            break;

        /* delay 0-1000 ms in 10 ms steps */
        case AUX_AUTO_RESET:
            if (auxState.editMode == AUX_EDIT_BYTE) {
                auxState.autoResetDelay = constrain(
                    auxState.autoResetDelay + delta * 10, 0, 1000);
                redrawAuxRow(AUX_AUTO_RESET);
            }
            break;

        default: break;
    }
}

/* ===================================================================== */
/*  OK – short                                                           */
/* ===================================================================== */
void auxHandleShort()
{
    if (menuState.selectedItem == AUX_AUTO_RESET &&
        auxState.editMode == AUX_EDIT_BYTE)
    {
        auxState.editMode = AUX_EDIT_NONE;
        updateEditIndicator(false);
        redrawAuxRow(AUX_AUTO_RESET);
    }
}

/* ===================================================================== */
/*  OK – long                                                            */
/* ===================================================================== */
void auxHandleLong()
{
    switch (menuState.selectedItem)
    {
        /* ── 1) I²C / SPI probe ───────────────────────────────────── */
        case AUX_INTERNAL_TEST:
        {
            tft.fillRect(0, 30, 480, 242, COLOR_BLACK);
            tft.setTextSize(2);
            tft.setCursor(4, 34);

            struct Dev { const char* name; uint8_t addr; };
            constexpr Dev devs[] = {
                { "TCA9555 MCU", 0x20 }, { "ADC PMOP",  0x21 },
                { "ADC RFOPD",  0x22 }, { "VR PMOP",   0x28 },
                { "VR RFOPD",   0x2B }, { "RT4527A MB",0x36 },
                { "EEPROM MCU", 0x50 }
            };

            char line[40];

            for (const auto& d : devs)
            {
                Wire.beginTransmission(d.addr);
                bool ok = (Wire.endTransmission() == 0);

                tft.setTextColor(ok ? COLOR_GREEN : COLOR_RED);
                // compose "name @0xXX  OK/MISSING"
                snprintf(line, sizeof(line),
                         "%-12s @0x%02X  %s",
                         d.name, d.addr, ok ? "OK" : "MISSING");
                tft.println(line);
            }

            /* crude SPI flash test (CS = PA21 = Arduino pin 7) */
            pinMode(7, OUTPUT);  digitalWrite(7, LOW);  delayMicroseconds(2);
            bool spiOk = digitalRead(7) == LOW;
            digitalWrite(7, HIGH); pinMode(7, INPUT_PULLUP);

            tft.setTextColor(spiOk ? COLOR_GREEN : COLOR_RED);
            snprintf(line, sizeof(line),
                     "%-12s        %s", "SPI-Flash", spiOk ? "OK" : "MISSING");
            tft.println(line);

            /* wait max 10 s or until any key press */
            uint32_t t0 = millis();
            while (millis() - t0 < 10000) { pollButtons(); delay(10); }
            initDisplay();
            redrawAuxList();
            break;
        }

        /* ── 2) full EEPROM erase ────────────────────────────────── */
        case AUX_EEPROM_FORMAT:
        {
            tft.fillRect(100, 120, 280, 40, COLOR_BLACK);
            tft.drawRect(100, 120, 280, 40, COLOR_WHITE);
            tft.setCursor(110, 130);
            tft.setTextColor(COLOR_WHITE);
            tft.print(F("Formatting …"));
            eepromChipErase();
            delay(600);
            redrawAuxList();
            break;
        }

        /* ── 3) autoreset toggle / delay edit ───────────────────── */
        case AUX_AUTO_RESET:
        {
            if (auxState.editMode == AUX_EDIT_NONE)
            {
                auxState.autoResetEnable = !auxState.autoResetEnable;
                if (auxState.autoResetEnable) {
                    auxState.editMode = AUX_EDIT_BYTE;
                    updateEditIndicator(true);
                }
            }
            else
            {
                auxState.editMode = AUX_EDIT_NONE;
                updateEditIndicator(false);
            }
            redrawAuxRow(AUX_AUTO_RESET);
            break;
        }

        default: break;
    }
}
/* ─────────────────────────────────────────────────────────────────────── */
