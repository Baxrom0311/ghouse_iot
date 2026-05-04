#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// 1.8" 128x160 RGB TFT ST7735S wiring. Change these if your module is wired
// differently. MOSI intentionally avoids GPIO23 because DHT is on GPIO23.
#define TFT_CS_PIN 5
#define TFT_DC_PIN 21
#define TFT_RST_PIN 22
#define TFT_MOSI_PIN 19
#define TFT_SCLK_PIN 18
#define TFT_BL_PIN 4
#define TFT_BACKLIGHT_ACTIVE_LOW 0
#define TFT_ROTATION 0
#define TFT_INIT_TAB INITR_BLACKTAB

class AgroDisplay : public Print
{
public:
    AgroDisplay();

    void init();
    void backlight(bool enabled = true);
    void clear();
    void setCursor(uint8_t col, uint8_t row);
    void setTextColor(uint16_t color);
    size_t write(uint8_t value) override;

    using Print::print;
    using Print::println;
    using Print::write;

private:
    static constexpr uint8_t CHAR_WIDTH = 6;
    static constexpr uint8_t LINE_HEIGHT = 10;

    Adafruit_ST7735 tft;
    uint8_t cursor_col = 0;
    uint8_t cursor_row = 0;
    bool initialized = false;

    void apply_cursor();
};

extern AgroDisplay lcd;
