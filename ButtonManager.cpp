#include "ButtonManager.h"
#include "MenuState.h"
#include "DisplayManager.h"
#include "InterlockManager.h"
#include "AuxManager.h"
#include "EepromManager.h"

/* ─────────── forward prototypes for tab helpers ─────────── */
static void handleTabLeft();
static void handleTabRight();

/* ─────────── hardware pins ─────────── */
#define BTN_DOWN_PIN   A3
#define BTN_LEFT_PIN   A4
#define BTN_UP_PIN     A5
#define BTN_RIGHT_PIN  A6
#define BTN_OK_PIN     1

struct BtnState {
  uint8_t  ctr = 0;
  bool     down = false;
  bool     evt = false;
  bool     dblArm = false;
  uint32_t t0 = 0;
} btn[BTN_COUNT];

static constexpr uint8_t  DB_TICKS = 4;
static constexpr uint32_t LONG_MS  = 800;
static constexpr uint32_t DBL_MS   = 450;

/* ─────────── local handlers ─────────── */
static void onShort (uint8_t idx);
static void onLong  (uint8_t idx);
static void onDouble(uint8_t idx);

/* ─────────── init & poll ─────────── */
void initButtons()
{
  pinMode(BTN_DOWN_PIN,  INPUT_PULLUP);
  pinMode(BTN_LEFT_PIN,  INPUT_PULLUP);
  pinMode(BTN_UP_PIN,    INPUT_PULLUP);
  pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
  pinMode(BTN_OK_PIN,    INPUT_PULLUP);
}

void pollButtons()
{
  static uint32_t next = 0;
  uint32_t now_us = micros();
  if ((int32_t)(now_us - next) < 0) return;
  next = now_us + 5000;                        // 5 kHz poll

  uint32_t port = PORT->Group[0].IN.reg;
  bool raw[BTN_COUNT] = {
    !(port & (1ul << 4)), !(port & (1ul << 5)),
    !(port & (1ul << 6)), !(port & (1ul << 7)),
    !(port & (1ul << 23))
  };
  uint32_t ms = millis();

  for (uint8_t i = 0; i < BTN_COUNT; ++i) {
    auto &b = btn[i];

    if (raw[i]) {                               // pressed (active-LOW)
      if (b.ctr < DB_TICKS) ++b.ctr;
      if (b.ctr >= DB_TICKS && !b.down) {
        b.down = true;  b.t0 = ms;  b.evt = false;
      }
      if (b.down && !b.evt && ms - b.t0 >= LONG_MS) {
        onLong(i);  b.evt = true;
      }
    } else {
      if (b.ctr) --b.ctr;
      if (b.down && b.ctr == 0) {
        b.down = false;
        if (!b.evt) {
          if (i == IDX_OK && b.dblArm && ms - b.t0 <= DBL_MS) {
            onDouble(i);  b.dblArm = false;
          } else {
            onShort(i);
            if (i == IDX_OK) { b.dblArm = true;  b.t0 = ms; }
          }
        }
      }
    }
    if (i == IDX_OK && b.dblArm && ms - b.t0 > DBL_MS) b.dblArm = false;
  }
}

/* ==================================================================== */
/*  EVENT-HANDLER IMPLEMENTATIONS                                       */
/* ==================================================================== */
static void onShort(uint8_t idx)
{
  bumpIdleTimer();

  /* wake from idle screen */
  if (menuState.screen == SCREEN_IDLE) {
    menuState.screen = SCREEN_MENU;
    redrawAll();
    return;
  }

  /* navigation ------------------------------------------------------- */
  if (idx == IDX_UP && menuState.selectedItem > 0) {
    menuState.selectedItem--;  updateItem();  return;
  }
  if (idx == IDX_DOWN) {
    uint8_t n = itemCountForTab(menuState.currentTab);
    if (menuState.selectedItem + 1 < n) {
      menuState.selectedItem++;  updateItem();  return;
    }
  }
  if (idx == IDX_LEFT)  handleTabLeft();
  if (idx == IDX_RIGHT) handleTabRight();

  /* confirm edit in Overview ---------------------------------------- */
  if (idx == IDX_OK &&
      menuState.currentTab == TAB_OVERVIEW &&
      menuState.editMode)
  {
    applyEditStateToItem(menuState.selectedItem,
                         menuState.editStateIndex);
    saveOverviewSettings();                // persistent!
    menuState.editMode = false;
    updateEditIndicator(false);
    paintItem(menuState.selectedItem, true);
  }
}

static void onLong(uint8_t idx)
{
  bumpIdleTimer();

  /* AUX-tab long-press is delegated */
  if (menuState.currentTab == TAB_AUXILIARY && idx == IDX_OK) {
    auxHandleLong();
    return;
  }

  /* toggle edit mode in Overview */
  if (idx == IDX_OK && menuState.currentTab == TAB_OVERVIEW) {
    menuState.editMode = !menuState.editMode;
    if (menuState.editMode) menuState.editStateIndex = 0;
    updateEditIndicator(menuState.editMode);
    paintItem(menuState.selectedItem, true);
  }

  /* fire manual reset pulse (Overview item 8) */
  if (idx == IDX_DOWN &&
      menuState.currentTab == TAB_OVERVIEW &&
      menuState.selectedItem == 8)
  {
    flashResetIndicator();
    sendResetPulse();
  }
}

static void onDouble(uint8_t idx)
{
  bumpIdleTimer();
  if (idx == IDX_OK) showIdleScreen();
}

/* ==================================================================== */
/*  TAB-SWITCH HELPERS (LEFT / RIGHT)                                   */
/* ==================================================================== */
static void handleTabLeft()
{
  if (menuState.currentTab == 0) return;
  menuState.currentTab = (TabID)(menuState.currentTab - 1);
  menuState.selectedItem = NO_SELECTION;      // nothing focused yet
  updateTab();                                // header repaint now
}

static void handleTabRight()
{
  if (menuState.currentTab + 1 >= TAB_COUNT) return;
  menuState.currentTab = (TabID)(menuState.currentTab + 1);
  menuState.selectedItem = NO_SELECTION;
  updateTab();
}
