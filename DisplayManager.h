#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "MenuState.h"           // ← TabID & colour constants

/* ---------- public API used by other modules ---------- */
void initDisplay();
void redrawAll();
void updateTab();
void updateItem();
void updateEditIndicator(bool on);
void showIdleScreen();
void flashResetIndicator();

/* this one is called from AuxManager.cpp for the AUX rows */
void paintItem(uint8_t idx, bool selected);

/* ButtonManager needs to repaint a single tab header */
void paintTab(TabID id, bool selected);     // ← now PUBLIC

#endif
