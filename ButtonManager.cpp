/* --------------------  ButtonManager.cpp  -------------------- */
#include "ButtonManager.h"

#include <Arduino.h>
#include "MenuState.h"
#include "DisplayManager.h"     // drawTabHeader(), updateItem(), updateTab()
#include "InterlockManager.h"   // flashResetIndicator(), sendResetPulse()
#include "AuxManager.h"
#include "EepromManager.h"      // saveOverviewSettings()

/* ─────────── pins & bits ─────────── */
#define BTN_DOWN_PIN   A3
#define BTN_LEFT_PIN   A4
#define BTN_UP_PIN     A5
#define BTN_RIGHT_PIN  A6
#define BTN_OK_PIN     1

#define BTN_DOWN_BIT   4
#define BTN_LEFT_BIT   5
#define BTN_UP_BIT     6
#define BTN_RIGHT_BIT  7
#define BTN_OK_BIT     23

/* ─────────── timing ─────────── */
constexpr uint8_t  DB_TICKS = 4;
constexpr uint32_t LONG_MS  = 1000;
constexpr uint32_t DBL_MS   = 500;

/* ─────────── state ─────────── */
struct BtnState {
    uint8_t  ctr = 0;
    bool     down = false;
    bool     evt = false;
    bool     dblArm = false;
    uint32_t t0 = 0;
} btns[BTN_COUNT];

/* forward decls */
static void onShort (uint8_t idx);
static void onLong  (uint8_t idx);
static void onDouble(uint8_t idx);
static void handleTabLeft();
static void handleTabRight();

/* ═════════════════  INIT  ═════════════════ */
void initButtons()
{
    pinMode(BTN_DOWN_PIN , INPUT_PULLUP);
    pinMode(BTN_LEFT_PIN , INPUT_PULLUP);
    pinMode(BTN_UP_PIN   , INPUT_PULLUP);
    pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BTN_OK_PIN   , INPUT_PULLUP);
}

/* ═════════════════  POLL  ═════════════════ */
void pollButtons()
{
    static uint32_t next = 0;
    uint32_t now_us = micros();
    if ((int32_t)(now_us - next) < 0) return;
    next = now_us + 5000;                    // 5 ms

    uint32_t port = PORT->Group[0].IN.reg;
    bool raw[BTN_COUNT] = {
        !(port & (1ul << BTN_DOWN_BIT)),
        !(port & (1ul << BTN_LEFT_BIT)),
        !(port & (1ul << BTN_UP_BIT)),
        !(port & (1ul << BTN_RIGHT_BIT)),
        !(port & (1ul << BTN_OK_BIT))
    };
    uint32_t t = millis();

    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        auto &b = btns[i];

        /* --------------- pressed --------------- */
        if (raw[i]) {
            if (b.ctr < DB_TICKS) b.ctr++;
            if (b.ctr == DB_TICKS && !b.down) {
                b.down = true; b.t0 = t; b.evt = false;
            }
            if (b.down && !b.evt && t - b.t0 >= LONG_MS) {
                onLong(i); b.evt = true;
            }
        }
        /* --------------- released -------------- */
        else {
            if (b.ctr) b.ctr--;
            if (b.down && b.ctr == 0) {
                b.down = false;
                if (!b.evt) {
                    if (i == IDX_OK && b.dblArm && (t - b.t0) <= DBL_MS) {
                        onDouble(i); b.dblArm = false;
                    } else {
                        onShort(i);
                        if (i == IDX_OK) { b.dblArm = true; b.t0 = t; }
                    }
                }
                b.evt = false;
            }
        }
        if (i == IDX_OK && b.dblArm && (t - b.t0) > DBL_MS) b.dblArm = false;
    }
}

/* ═════════════════  EVENT HANDLERS  ═════════════════ */
static void onShort(uint8_t idx)
{
    bumpIdleTimer();

    if (menuState.screen == SCREEN_IDLE) {
        menuState.screen = SCREEN_MENU;
        redrawAll();
        return;
    }

    if (idx == IDX_UP && menuState.selectedItem) {
        menuState.selectedItem--; updateItem();
    }
    else if (idx == IDX_DOWN &&
             menuState.selectedItem + 1 <
                 itemCountForTab(menuState.currentTab)) {
        menuState.selectedItem++; updateItem();
    }
    else if (idx == IDX_LEFT)  handleTabLeft();
    else if (idx == IDX_RIGHT) handleTabRight();
    else if (idx == IDX_OK) {
        if (menuState.currentTab == TAB_AUXILIARY) {
            auxHandleShort(); return;
        }
        if (menuState.currentTab == TAB_OVERVIEW && menuState.editMode) {
            applyEditStateToItem(menuState.selectedItem,
                                 menuState.editStateIndex);
            saveOverviewSettings();          // ↢ now resolved
            menuState.editMode = false;
            updateEditIndicator(false);
            paintItem(menuState.selectedItem, true);
        }
    }
}

static void onLong(uint8_t idx)
{
    bumpIdleTimer();

    if (menuState.currentTab == TAB_AUXILIARY && idx == IDX_OK) {
        auxHandleLong(); return;
    }
    if (menuState.currentTab == TAB_OVERVIEW && idx == IDX_OK) {
        menuState.editMode = !menuState.editMode;
        if (menuState.editMode) menuState.editStateIndex = 0;
        updateEditIndicator(menuState.editMode);
        paintItem(menuState.selectedItem, true);
        return;
    }
    if (menuState.currentTab == TAB_OVERVIEW &&
        idx == IDX_DOWN && menuState.selectedItem == 8) {
        flashResetIndicator(); sendResetPulse();
    }
}

static void onDouble(uint8_t idx)
{
    bumpIdleTimer();
    if (idx == IDX_OK) showIdleScreen();
}

/* ═════════════════  TAB HELPERS (instant header, delayed body) ═════════════════ */
static void handleTabLeft()
{
    if (menuState.currentTab == 0) return;
    drawTabHeader(menuState.currentTab, false);
    menuState.currentTab = (TabID)(menuState.currentTab - 1);
    drawTabHeader(menuState.currentTab, true);

    menuState.bodyRedrawPending = true;
    menuState.bodyRedrawT0      = millis();
}

static void handleTabRight()
{
    if (menuState.currentTab + 1 >= TAB_COUNT) return;
    drawTabHeader(menuState.currentTab, false);
    menuState.currentTab = (TabID)(menuState.currentTab + 1);
    drawTabHeader(menuState.currentTab, true);

    menuState.bodyRedrawPending = true;
    menuState.bodyRedrawT0      = millis();
}
