// ST7365P_Display.h

#ifndef ST7365P_DISPLAY_H
#define ST7365P_DISPLAY_H

#include <Arduino.h>
#include <Adafruit_GFX.h>

class ST7365P_Display : public Adafruit_GFX {
public:
    ST7365P_Display();

    // Must call before any drawing
    void begin();

    // Fills entire screen
    void fillScreen(uint16_t color);

    // Fast rectangle (inverted color) and normal rect
    void fillRectFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;

    // Required Adafruit_GFX override
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;

    // Convenience text routines (wrap A‑GFX)
    void drawChar(int16_t x, int16_t y, char c, uint16_t color, uint8_t size);
    void drawText(int16_t x, int16_t y, const char* str, uint16_t color, uint8_t size);

private:
    void hwReset();
    void initDisplay();
    void sendSPI9(uint8_t dc, uint8_t val);
    void sendCmd(uint8_t cmd);
    void sendData(uint8_t data);
    void setColumn(uint16_t x0, uint16_t x1);
    void setRow(uint16_t y0, uint16_t y1);
    void startRAM();

    // Pins
    static const uint8_t PIN_RST = 5;     // PB11
    static const uint8_t PIN_CS  = 0;     // PA22
    static const uint8_t PIN_SCK = SCK;   // PA17
    static const uint8_t PIN_SDA = MOSI;  // PA16

    // Panel geometry
    static const uint16_t PANEL_W = 480;
    static const uint16_t PANEL_H = 272;
    static const uint16_t RAM_H   = 320;
    static const uint16_t YOFF    = (RAM_H - PANEL_H) / 2;
};

#endif
