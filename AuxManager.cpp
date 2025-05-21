/*********************************************************************
 *  AuxManager.cpp  –  AUX-tab logic (LCD brightness, internal test,
 *                     EEPROM format, autoreset, locked LUT)
 *********************************************************************/

#include "AuxManager.h"
#include <Arduino.h>
#include <Wire.h>

#include "MenuState.h"
#include "DisplayManager.h"      // tft, colours, updateEditIndicator()
#include "ButtonManager.h"       // pollButtons() for wait-loops
#include "InterlockManager.h"    // readInterlock(), sendResetPulse()
#include "EepromManager.h"       // eepromChipErase()
#include "ST7365P_Display.h"

extern ST7365P_Display tft;      // single global instance from DisplayManager


/* ─────────── RT4527A back-light driver ─────────── */

static constexpr uint8_t RT_ADDR0 = 0x36;     // A0 strap = 0
static constexpr uint8_t RT_ADDR1 = 0x37;     // A0 strap = 1
static uint8_t rtAddr = 0;                    // filled by detectRt()

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

static inline void rtSetDcMode()             { rtWrite(0x00, 0x01); } // DC dim
static inline void setBacklight(uint8_t val) { rtWrite(0x01, val);  }

/* ─────────── persistent AUX state ─────────── */

AuxState auxState =
{
    /* lcdBrightness   */ 128,
    /* autoResetEnable */ false,
    /* autoResetDelay  */ 500,
    /* editMode        */ AUX_EDIT_NONE
};

/* ─────────── autoreset background tick ─────────── */

static bool     arPending = false;
static uint32_t arT0      = 0;

void auxTick()
{
    if (!auxState.autoResetEnable) return;

    bool giLow = readInterlock(1, 2);               // P10 LOW → interlock
    if (giLow && !arPending) { arPending = true; arT0 = millis(); }

    if (arPending && (millis() - arT0) >= auxState.autoResetDelay)
    {
        sendResetPulse();
        arPending = false;
    }
}

/* ─────────── screen helpers ─────────── */

void redrawAuxRow(uint8_t idx)
{
    const uint16_t y   = 30 + idx * 24;
    const bool     sel = (idx == menuState.selectedItem);

    tft.fillRect(0, y, 480, 24, sel ? COLOR_SELECTED_BG : COLOR_BLACK);
    tft.setCursor(2, y + 6);
    tft.setTextSize(2);
    tft.setTextColor(sel ? COLOR_YELLOW : COLOR_WHITE);

    switch (idx)
    {
        case AUX_LCD_BRIGHTNESS:
            tft.print("LCD brightness: ");
            tft.print(auxState.lcdBrightness);
            break;

        case AUX_INTERNAL_TEST:
            tft.print("Internal test  (long OK)");
            break;

        case AUX_EEPROM_FORMAT:
            tft.print("EEPROM format  (long OK)");
            break;

        case AUX_AUTO_RESET:
            tft.print("Autoreset: ");
            tft.print(auxState.autoResetEnable ? "ON " : "OFF");
            tft.print("  t=");
            tft.print(auxState.autoResetDelay);
            tft.print(" ms");
            break;

        case AUX_POWER_LUT:
            tft.print("Power LUT  (locked)");
            break;
    }
}

static void redrawAuxList()
{
    for (uint8_t i = 0; i < AUX_COUNT; ++i) redrawAuxRow(i);
}

/* ─────────── public init ─────────── */

void auxInit()
{
    Wire.begin();                       // ensure I²C up
    if (detectRt()) rtSetDcMode();
    setBacklight(auxState.lcdBrightness);
}

/* ─────────── encoder handler ─────────── */

void auxEncoder(int8_t d)
{
    switch (menuState.selectedItem)
    {
        /* live 0–255 brightness ------------------------------------------------ */
        case AUX_LCD_BRIGHTNESS:
            auxState.lcdBrightness = constrain(
                auxState.lcdBrightness + d, 0, 255);
            setBacklight(auxState.lcdBrightness);
            redrawAuxRow(AUX_LCD_BRIGHTNESS);
            break;

        /* autoreset delay 0–1000 ms in 10 ms steps ---------------------------- */
        case AUX_AUTO_RESET:
            if (auxState.editMode == AUX_EDIT_BYTE)
            {
                auxState.autoResetDelay = constrain(
                    auxState.autoResetDelay + d * 10, 0, 1000);
                redrawAuxRow(AUX_AUTO_RESET);
            }
            break;

        default: break;
    }
}

/* ─────────── OK short ─────────── */

void auxHandleShort()
{
    if (menuState.selectedItem == AUX_AUTO_RESET &&
        auxState.editMode == AUX_EDIT_BYTE)
    {
        auxState.editMode     = AUX_EDIT_NONE;
        menuState.editMode    = false;              // ═══ keep flag in sync ═══
        updateEditIndicator(false);
        redrawAuxRow(AUX_AUTO_RESET);
    }
}

/* ─────────── OK long ─────────── */

void auxHandleLong()
{
    switch (menuState.selectedItem)
    {
        /* 1) internal I²C / SPI probe ---------------------------------------- */
        case AUX_INTERNAL_TEST:
        {
            tft.fillRect(0, 30, 480, 242, COLOR_BLACK);
            tft.setTextSize(2); tft.setTextColor(COLOR_WHITE);
            tft.setCursor(4, 34);

            struct { const char* n; uint8_t a; } dev[] =
            {
                { "TCA9555 MCU", 0x20 }, { "ADC PMOP",  0x21 },
                { "ADC RFOPD",   0x22 }, { "VR PMOP",   0x28 },
                { "VR RFOPD",    0x2B }, { "RT4527A MB",0x36 },
                { "EEPROM MCU",  0x50 }
            };

            for (auto& p : dev)
            {
                Wire.beginTransmission(p.a);
                bool ok = (Wire.endTransmission() == 0);
                tft.setTextColor(ok ? COLOR_GREEN : COLOR_RED);
                tft.print(p.n); tft.print(" @0x"); tft.print(p.a, HEX);
                tft.print(ok ? "  OK" : "  MISSING"); tft.print('\n');
            }

            /* very simple SPI-flash presence: CS = PA21 (D7) must pull low */
            pinMode(7, OUTPUT); digitalWrite(7, LOW); delayMicroseconds(2);
            bool spiOk = (digitalRead(7) == LOW);
            digitalWrite(7, HIGH); pinMode(7, INPUT_PULLUP);

            tft.setTextColor(spiOk ? COLOR_GREEN : COLOR_RED);
            tft.print("SPI-Flash       ");
            tft.print(spiOk ? "OK" : "MISSING");

            delay(10000);                       // 10 s pause
            redrawAuxList();
            break;
        }

        /* 2) EEPROM chip-erase ----------------------------------------------- */
        case AUX_EEPROM_FORMAT:
        {
            tft.fillRect(100, 120, 280, 40, COLOR_BLACK);
            tft.drawRect(100, 120, 280, 40, COLOR_WHITE);
            tft.setCursor(110, 130);
            tft.setTextSize(2); tft.setTextColor(COLOR_WHITE);
            tft.print("Formatting…");
            eepromChipErase();
            delay(600);
            redrawAll();
            break;
        }

        /* 3) autoreset toggle / enter-leave delay edit ------------------------ */
        case AUX_AUTO_RESET:
        {
            if (auxState.editMode == AUX_EDIT_NONE)          // entering
            {
                auxState.autoResetEnable = !auxState.autoResetEnable;
                if (auxState.autoResetEnable)
                {
                    auxState.editMode  = AUX_EDIT_BYTE;
                    menuState.editMode = true;               // ═ sync ═
                    updateEditIndicator(true);
                }
            }
            else                                             // leaving
            {
                auxState.editMode  = AUX_EDIT_NONE;
                menuState.editMode = false;                  // ═ sync ═
                updateEditIndicator(false);
            }
            redrawAuxList();
            //redrawAuxRow(AUX_AUTO_RESET);
            break;
        }

        default: break;
    }
}
