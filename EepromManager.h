#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include <Arduino.h>

#define EEPROM_BASE_ADDR      0x50
#define EEPROM_TOTAL_SIZE     0x40000
#define EEPROM_PAGE_SIZE      256

#define OVERVIEW_CONFIG_ADDR  0x0000
#define OVERVIEW_VALID_FLAG   0xA5

void initEeprom();
void loadOverviewSettings();
void saveOverviewSettings();

uint8_t eepromRead(uint32_t addr);
void    eepromWrite(uint32_t addr,uint8_t data);
void    eepromChipErase();

#endif
