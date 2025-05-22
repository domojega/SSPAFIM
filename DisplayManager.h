#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

void initDisplay();
void redrawAll();
void updateTab();
void updateItem();
void updateEditIndicator(bool active);
void showIdleScreen();
void flashResetIndicator();
void paintItem(uint8_t index, bool selected);


#endif
