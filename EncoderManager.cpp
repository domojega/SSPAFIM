#include "EncoderManager.h"
#include <Arduino.h>
#include "MenuState.h"
#include "DisplayManager.h"
#include "InterlockManager.h"
#include "AuxManager.h"

#define ENC_A_PIN 2
#define ENC_B_PIN 3

static uint8_t lastAB = 0;
static uint32_t lastEditStepMs = 0;

static const int8_t dirTable[16] = {
   0,-1,+1, 0,
  +1, 0, 0,-1,
  -1, 0, 0,+1,
   0,+1,-1, 0
};

void initEncoder() {
  pinMode(ENC_A_PIN, INPUT_PULLUP);
  pinMode(ENC_B_PIN, INPUT_PULLUP);
  lastAB = (digitalRead(ENC_A_PIN) << 1) | digitalRead(ENC_B_PIN);
}

static int8_t readDelta() {
  uint8_t ab = (digitalRead(ENC_A_PIN) << 1) | digitalRead(ENC_B_PIN);
  uint8_t idx = (lastAB << 2) | ab;
  lastAB = ab;
  return dirTable[idx];
}

void pollEncoder() {
  int8_t d = readDelta();
  if (d == 0) return;

  // editing overview
  if (menuState.editMode && menuState.currentTab == TAB_OVERVIEW) {
    uint32_t now = millis();
    if (now - lastEditStepMs < 300) return;
    lastEditStepMs = now;
    if (d > 0) menuState.editStateIndex = (menuState.editStateIndex + 1) % 3;
    else       menuState.editStateIndex = (menuState.editStateIndex + 2) % 3;
    paintItem(menuState.selectedItem, true);
    return;
  }

  // aux tab
  if (menuState.currentTab == TAB_AUXILIARY) {
    auxEncoder(d);
    return;
  }

  // scrolling
  uint8_t count = itemCountForTab(menuState.currentTab);
  if (d > 0 && menuState.selectedItem + 1 < count) {
    menuState.selectedItem++;
    updateItem();
  } else if (d < 0 && menuState.selectedItem > 0) {
    menuState.selectedItem--;
    updateItem();
  }
}
