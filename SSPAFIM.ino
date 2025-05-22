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
  if (menuState.bodyRedrawPending &&
    (millis() - menuState.bodyRedrawT0) >= TAB_REDRAW_DELAY_MS)
{
    updateTab();                        // your existing full repaint
    menuState.bodyRedrawPending = false;
}
  auxTick();

delay(1);
  if (menuState.screen == SCREEN_MENU &&
      millis() - menuState.lastAction > IDLE_MS) {
    showIdleScreen();
  }
/* — delayed body repaint — */
if (menuState.bodyRedrawPending &&
    millis() - menuState.bodyRedrawT0 >= 200)   // 200 ms grace
{
    updateTab();                       // heavy redraw
    menuState.bodyRedrawPending = false;
}
}
