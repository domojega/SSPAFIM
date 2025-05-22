#include <Arduino.h>
#include <Wire.h>
#include "MenuState.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "EncoderManager.h"
#include "InterlockManager.h"
#include "EepromManager.h"
#include "AuxManager.h"

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  initDisplay();                 // tft.begin(), clears the screen
  initButtons();
  initEncoder();
  initInterlocks();
  initEeprom();
  loadOverviewSettings();        // may change simulated bits
  auxInit();                     // sets back-light etc.
  redrawAll();                   // ←  move DOWN here
  bumpIdleTimer();               // start idle timer
}

void loop() {
  pollButtons();
  pollEncoder();
  auxTick();

delay(1);
  if (menuState.screen == SCREEN_MENU &&
      millis() - menuState.lastAction > IDLE_MS) {
    showIdleScreen();
  }
}
