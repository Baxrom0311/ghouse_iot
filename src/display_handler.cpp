#include <display_handler.h>

AgroDisplay lcd;

AgroDisplay::AgroDisplay()
    : tft(TFT_CS_PIN, TFT_DC_PIN, TFT_MOSI_PIN, TFT_SCLK_PIN, TFT_RST_PIN)
{
}

void AgroDisplay::init()
{
    pinMode(TFT_BL_PIN, OUTPUT);
    backlight(false);

    tft.initR(TFT_INIT_TAB);
    tft.setRotation(TFT_ROTATION);
    tft.setTextWrap(false);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.fillScreen(ST77XX_BLACK);

    cursor_col = 0;
    cursor_row = 0;
    initialized = true;
    backlight(true);
}

void AgroDisplay::backlight(bool enabled)
{
    digitalWrite(
        TFT_BL_PIN,
        TFT_BACKLIGHT_ACTIVE_LOW
            ? (enabled ? LOW : HIGH)
            : (enabled ? HIGH : LOW));
}

void AgroDisplay::clear()
{
    if (!initialized)
    {
        return;
    }

    tft.fillScreen(ST77XX_BLACK);
    cursor_col = 0;
    cursor_row = 0;
    apply_cursor();
}

void AgroDisplay::setCursor(uint8_t col, uint8_t row)
{
    cursor_col = col;
    cursor_row = row;
    apply_cursor();
}

void AgroDisplay::setTextColor(uint16_t color)
{
    if (!initialized)
    {
        return;
    }

    tft.setTextColor(color, ST77XX_BLACK);
}

size_t AgroDisplay::write(uint8_t value)
{
    if (!initialized)
    {
        return 0;
    }

    if (value == '\n')
    {
        cursor_col = 0;
        cursor_row++;
        apply_cursor();
        return 1;
    }

    size_t written = tft.write(value);
    cursor_col++;
    return written;
}

void AgroDisplay::apply_cursor()
{
    if (!initialized)
    {
        return;
    }

    tft.setCursor(cursor_col * CHAR_WIDTH, cursor_row * LINE_HEIGHT);
}
