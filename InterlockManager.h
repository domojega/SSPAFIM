#ifndef INTERLOCK_MANAGER_H
#define INTERLOCK_MANAGER_H

#include <Arduino.h>
#include "MenuState.h"

struct InterlockItem {
  const char* label;
  uint8_t port, bit;
  bool allowSim;
};

extern InterlockItem interlocks[9];

void initInterlocks();
bool readInterlock(uint8_t port,uint8_t bit);
bool isSimulated(uint8_t port,uint8_t bit);
void setSimulated(uint8_t port,uint8_t bit,bool state);
void toggleSimulated(uint8_t port,uint8_t bit);
void sendResetPulse();
void applyEditStateToItem(uint8_t idx,uint8_t state);
uint8_t readOutputRegister(uint8_t port);
uint16_t getStatusColor(uint8_t idx);

#endif
