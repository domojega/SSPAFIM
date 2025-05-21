/* ------------- ButtonManager.cpp ------------- */
#include "ButtonManager.h"

#include <Arduino.h>
#include "MenuState.h"
#include "DisplayManager.h"
#include "InterlockManager.h"
#include "EepromManager.h"
#include "AuxManager.h"

/* ────── forward-declare the handlers (needed!) ────── */
static void onShort (uint8_t idx);
static void onLong  (uint8_t idx);
static void onDouble(uint8_t idx);

/* ────── GPIO mapping ────── */
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

struct BtnState {
    uint8_t  ctr     = 0;
    bool     down    = false;
    bool     evt     = false;
    bool     dblArm  = false;
    uint32_t t0      = 0;
} btns[BTN_COUNT];

static constexpr uint8_t  DB_TICKS  = 4;
static constexpr uint32_t LONG_MS   = 1000;
static constexpr uint32_t DBL_MS    = 500;

/* ------------------------------------------------------------------ */
void initButtons()
{
    pinMode(BTN_DOWN_PIN,  INPUT_PULLUP);
    pinMode(BTN_LEFT_PIN,  INPUT_PULLUP);
    pinMode(BTN_UP_PIN,    INPUT_PULLUP);
    pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BTN_OK_PIN,    INPUT_PULLUP);
}

/* ------------------------------------------------------------------ */
void pollButtons()
{
    static uint32_t next = 0;
    uint32_t us = micros();
    if ((int32_t)(us - next) < 0) return;
    next = us + 5000;

    uint32_t port = PORT->Group[0].IN.reg;
    bool raw[BTN_COUNT] = {
        !(port & (1ul << BTN_DOWN_BIT)),
        !(port & (1ul << BTN_LEFT_BIT)),
        !(port & (1ul << BTN_UP_BIT)),
        !(port & (1ul << BTN_RIGHT_BIT)),
        !(port & (1ul << BTN_OK_BIT))
    };
    uint32_t now = millis();

    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        auto &b = btns[i];

        /* ------------- debouncing FSM ------------- */
        if (raw[i]) {
            if (b.ctr < DB_TICKS) b.ctr++;
            if (b.ctr >= DB_TICKS && !b.down) {
                b.down = true; b.t0 = now; b.evt = false;
            }
            if (b.down && !b.evt && now - b.t0 >= LONG_MS) {
                onLong(i); b.evt = true;
            }
        } else {
            if (b.ctr) b.ctr--;
            if (b.down && b.ctr == 0) {
                b.down = false;
                if (!b.evt) {
                    if (i == IDX_OK && b.dblArm && now - b.t0 <= DBL_MS) {
                        onDouble(i); b.dblArm = false;
                    } else {
                        onShort(i);
                        if (i == IDX_OK) { b.dblArm = true; b.t0 = now; }
                    }
                }
            }
        }
        if (i == IDX_OK && b.dblArm && now - b.t0 > DBL_MS) b.dblArm = false;
    }
}

/* ====== helper ======================================================== */
static void selectFirstIfNone()
{
    if (menuState.selectedItem == NO_SELECTION)
        menuState.selectedItem = 0;
}

static void selectLastIfNone()
{
    if (menuState.selectedItem == NO_SELECTION)
        menuState.selectedItem =
            itemCountForTab(menuState.currentTab) - 1;
}

/* ====== EVENT HANDLERS ================================================= */

/* ---- short press ---- */
static void onShort(uint8_t idx)
{
    bumpIdleTimer();

    /* wake from idle */
    if (menuState.screen == SCREEN_IDLE) {
        menuState.screen = SCREEN_MENU;
        redrawAll();
        return;
    }

    /* navigation */
    if (idx == IDX_UP) {
        if (menuState.selectedItem == NO_SELECTION) selectLastIfNone();
        else if (menuState.selectedItem) menuState.selectedItem--;
        updateItem();
    }
    else if (idx == IDX_DOWN) {
        uint8_t cnt = itemCountForTab(menuState.currentTab);
        if (menuState.selectedItem == NO_SELECTION) selectFirstIfNone();
        else if (menuState.selectedItem + 1 < cnt) menuState.selectedItem++;
        updateItem();
    }
    else if (idx == IDX_LEFT && menuState.currentTab > 0) {
        menuState.currentTab = (TabID)(menuState.currentTab - 1);
        menuState.selectedItem = NO_SELECTION;
        updateTab();
    }
    else if (idx == IDX_RIGHT && menuState.currentTab + 1 < TAB_COUNT) {
        menuState.currentTab = (TabID)(menuState.currentTab + 1);
        menuState.selectedItem = NO_SELECTION;
        updateTab();
    }

    /* confirm edit (Overview) */
    else if (idx == IDX_OK &&
             menuState.editMode && menuState.currentTab == TAB_OVERVIEW)
    {
        applyEditStateToItem(menuState.selectedItem,
                             menuState.editStateIndex);
        saveOverviewSettings();
        menuState.editMode = false;
        updateEditIndicator(false);
        paintItem(menuState.selectedItem, true);
    }

    /* delegate to AUX */
    if (menuState.currentTab == TAB_AUXILIARY && idx == IDX_OK)
        auxHandleShort();
}

/* ---- long press ---- */
static void onLong(uint8_t idx)
{
    bumpIdleTimer();

    /* Aux tab takes priority */
    if (menuState.currentTab == TAB_AUXILIARY && idx == IDX_OK) {
        auxHandleLong(); return;
    }

    /* toggle edit mode in Overview tab */
    if (idx == IDX_OK && menuState.currentTab == TAB_OVERVIEW) {
        menuState.editMode = !menuState.editMode;
        if (menuState.editMode) menuState.editStateIndex = 0;
        updateEditIndicator(menuState.editMode);
        if (menuState.selectedItem != NO_SELECTION)
            paintItem(menuState.selectedItem, true);
    }

    /* send reset pulse (Overview item 8) */
    if (idx == IDX_DOWN &&
        menuState.currentTab == TAB_OVERVIEW &&
        menuState.selectedItem == 8)
    {
        flashResetIndicator();
        sendResetPulse();
    }
}

/* ---- double press ---- */
static void onDouble(uint8_t idx)
{
    bumpIdleTimer();
    if (idx == IDX_OK) showIdleScreen();
}
