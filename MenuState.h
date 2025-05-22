#ifndef MENU_STATE_H
#define MENU_STATE_H
#include <Arduino.h>
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_GRAY    0x8410
#define COLOR_SELECTED_BG 0x0000
#define SCREEN_MENU   0
#define SCREEN_IDLE   1


enum TabID {
  TAB_OVERVIEW = 0,
  TAB_SETTINGS,
  TAB_AUXILIARY,
  TAB_COUNT
};

enum BtnIdx {
  IDX_DOWN,
  IDX_LEFT,
  IDX_UP,
  IDX_RIGHT,
  IDX_OK,
  BTN_COUNT
};

constexpr uint8_t NO_SELECTION = 0xFF;   // 255 = “nothing selected”
const uint32_t IDLE_MS = 120000;
constexpr uint16_t TAB_REDRAW_DELAY_MS = 350;

struct MenuState {
  uint8_t screen = SCREEN_MENU;
  TabID currentTab = TAB_OVERVIEW;
  uint8_t selectedItem = NO_SELECTION;      // ← was 0
  bool editMode = false;
  uint8_t editStateIndex = 0;
  uint32_t lastAction = 0;
  bool     bodyRedrawPending = false;   // waiting for delay timeout
  uint32_t bodyRedrawT0      = 0;       // millis() when waiting started
};

extern MenuState menuState;

inline void bumpIdleTimer() {
  menuState.lastAction = millis();
}

inline uint8_t itemCountForTab(TabID tab) {
  switch (tab) {
    case TAB_OVERVIEW:    return 9;
    case TAB_SETTINGS:    return 8;
    case TAB_AUXILIARY:   return 5; //  ←  use the symbol
    default:              return 0;
  }
}

#endif
