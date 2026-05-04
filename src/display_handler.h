#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// 1.8" 128x160 RGB TFT ST7735S wiring. Many modules label SPI MOSI as SDA
// and SPI SCLK as SCL. SDA intentionally avoids GPIO23 because DHT is on GPIO23.
#define TFT_CS_PIN 5
#define TFT_DC_PIN 21
#define TFT_RST_PIN 22
#define TFT_SDA_PIN 19
#define TFT_SCL_PIN 18
#define TFT_BL_PIN 4
#define TFT_BACKLIGHT_ACTIVE_LOW 0
#define TFT_ROTATION 1
#define TFT_TEXT_SIZE 2
#define TFT_INIT_TAB INITR_BLACKTAB
#define TFT_SPI_FREQUENCY 8000000

class AgroDisplay : public Print
{
public:
    AgroDisplay();

    void init();
    void backlight(bool enabled = true);
    void clear();
    void setCursor(uint8_t col, uint8_t row);
    void setTextColor(uint16_t color);
    void setTextSize(uint8_t size);
    size_t write(uint8_t value) override;

    using Print::print;
    using Print::println;
    using Print::write;

private:
    static constexpr uint8_t CHAR_WIDTH = 6;
    static constexpr uint8_t LINE_HEIGHT = 10;

    Adafruit_ST7735 tft;
    SemaphoreHandle_t mutex = nullptr;
    uint8_t cursor_col = 0;
    uint8_t cursor_row = 0;
    uint8_t text_size = TFT_TEXT_SIZE;
    bool initialized = false;

    void lock();
    void unlock();
    void backlight_unlocked(bool enabled);
    void apply_cursor();
};

extern AgroDisplay lcd;
