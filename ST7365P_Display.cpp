// ST7365P_Display.cpp

#include "ST7365P_Display.h"

// Direct‐port writes for MKR Zero (SAMD21)
#define WR_CS_LOW()   PORT->Group[PORTA].OUTCLR.reg = (1 << 22)
#define WR_CS_HIGH()  PORT->Group[PORTA].OUTSET.reg = (1 << 22)
#define WR_SCK_LOW()  PORT->Group[PORTA].OUTCLR.reg = (1 << 17)
#define WR_SCK_HIGH() PORT->Group[PORTA].OUTSET.reg = (1 << 17)
#define WR_SDA_LOW()  PORT->Group[PORTA].OUTCLR.reg = (1 << 16)
#define WR_SDA_HIGH() PORT->Group[PORTA].OUTSET.reg = (1 << 16)

static inline void pulseClock() {
    WR_SCK_LOW();
    WR_SCK_HIGH();
}

ST7365P_Display::ST7365P_Display()
  : Adafruit_GFX(PANEL_W, PANEL_H)  // initialize base GFX with width & height
{}

void ST7365P_Display::begin() {
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_CS,  OUTPUT);
    pinMode(PIN_SCK, OUTPUT);
    pinMode(PIN_SDA, OUTPUT);

    WR_CS_HIGH();
    WR_SCK_HIGH();
    WR_SDA_HIGH();

    hwReset();
    initDisplay();
}

void ST7365P_Display::hwReset() {
    digitalWrite(PIN_RST, LOW);  delay(10);
    digitalWrite(PIN_RST, HIGH); delay(120);
}

void ST7365P_Display::initDisplay() {
    // Initialization sequence from ST7365P datasheet
    sendCmd(0x01);  delay(120);            // SWRESET
    sendCmd(0x11);  delay(120);            // SLPOUT
    sendCmd(0xB0);  sendData(0x00);        // IFMODE (3-wire)
    sendCmd(0x38);                        // IDMOFF
    sendCmd(0x3A);  sendData(0x55);        // COLMOD: 16-bit
    sendCmd(0x13);                        // NORON
    sendCmd(0x36);  sendData(0x28); sendData(0x20);  // MADCTL: landscape
    sendCmd(0x29);  delay(120);            // DISPON
    sendCmd(0x20);                        // INVOFF
}

void ST7365P_Display::setColumn(uint16_t x0, uint16_t x1) {
    sendCmd(0x2A);
    sendData(x0 >> 8); sendData(x0 & 0xFF);
    sendData(x1 >> 8); sendData(x1 & 0xFF);
}

void ST7365P_Display::setRow(uint16_t y0, uint16_t y1) {
    sendCmd(0x2B);
    sendData(y0 >> 8); sendData(y0 & 0xFF);
    sendData(y1 >> 8); sendData(y1 & 0xFF);
}

void ST7365P_Display::startRAM() {
    sendCmd(0x2C);
}

void ST7365P_Display::sendSPI9(uint8_t dc, uint8_t val) {
    WR_CS_LOW();
    // 9th bit = D/C
    if (dc) WR_SDA_HIGH();
    else    WR_SDA_LOW();
    pulseClock();
    // 8 data bits
    for (int8_t i = 7; i >= 0; i--) {
        if (val & (1 << i)) WR_SDA_HIGH();
        else                WR_SDA_LOW();
        pulseClock();
    }
    WR_CS_HIGH();
}

void ST7365P_Display::sendCmd(uint8_t cmd) {
    sendSPI9(0, cmd);
}

void ST7365P_Display::sendData(uint8_t data) {
    sendSPI9(1, data);
}

void ST7365P_Display::fillRectFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // Fills w×h area with color (inverted internally if hardware needs it)
    uint16_t inv = ~color;
    setColumn(x, x + w - 1);
    setRow(y + YOFF, y + YOFF + h - 1);
    startRAM();
    uint32_t total = (uint32_t)w * h;
    while (total--) {
        sendData(inv >> 8);
        sendData(inv & 0xFF);
    }
}

void ST7365P_Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // GFX override: just call your fast routine
    fillRectFast(x, y, w, h, color);
}

void ST7365P_Display::fillScreen(uint16_t color) {
    fillRectFast(0, 0, PANEL_W, PANEL_H, color);
}

// **THIS** is the one pure‐virtual Adafruit_GFX requires
void ST7365P_Display::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= PANEL_W || y < 0 || y >= PANEL_H) return;
    uint16_t inv = ~color;
    setColumn(x, x);
    setRow(y + YOFF, y + YOFF);
    startRAM();
    sendData(inv >> 8);
    sendData(inv & 0xFF);
}

// Text helpers that wrap Adafruit_GFX’s cursor/print API
void ST7365P_Display::drawChar(int16_t x, int16_t y, char c, uint16_t color, uint8_t size) {
    setTextColor(color);
    setTextSize(size);
    setCursor(x, y);
    write(c);
}

void ST7365P_Display::drawText(int16_t x, int16_t y, const char* str, uint16_t color, uint8_t size) {
    setTextColor(color);
    setTextSize(size);
    setCursor(x, y);
    print(str);
}
