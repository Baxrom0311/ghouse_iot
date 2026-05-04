#include <display_handler.h>

AgroDisplay lcd;

AgroDisplay::AgroDisplay()
    : tft(&SPI, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN)
{
}

void AgroDisplay::init()
{
    if (mutex == nullptr)
    {
        mutex = xSemaphoreCreateMutex();
    }

    lock();
    pinMode(TFT_BL_PIN, OUTPUT);
    backlight_unlocked(false);
    pinMode(TFT_CS_PIN, OUTPUT);
    digitalWrite(TFT_CS_PIN, HIGH);

    SPI.begin(TFT_SCL_PIN, -1, TFT_SDA_PIN, TFT_CS_PIN);
    tft.initR(TFT_INIT_TAB);
    tft.setSPISpeed(TFT_SPI_FREQUENCY);
    tft.setRotation(TFT_ROTATION);
    tft.setTextWrap(false);
    text_size = TFT_TEXT_SIZE;
    tft.setTextSize(text_size);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.fillScreen(ST77XX_BLACK);

    cursor_col = 0;
    cursor_row = 0;
    initialized = true;
    backlight_unlocked(true);
    unlock();
}

void AgroDisplay::backlight(bool enabled)
{
    lock();
    backlight_unlocked(enabled);
    unlock();
}

void AgroDisplay::backlight_unlocked(bool enabled)
{
    digitalWrite(
        TFT_BL_PIN,
        TFT_BACKLIGHT_ACTIVE_LOW
            ? (enabled ? LOW : HIGH)
            : (enabled ? HIGH : LOW));
}

void AgroDisplay::clear()
{
    lock();
    if (!initialized)
    {
        unlock();
        return;
    }

    tft.fillScreen(ST77XX_BLACK);
    cursor_col = 0;
    cursor_row = 0;
    apply_cursor();
    unlock();
}

void AgroDisplay::setCursor(uint8_t col, uint8_t row)
{
    lock();
    cursor_col = col;
    cursor_row = row;
    apply_cursor();
    unlock();
}

void AgroDisplay::setTextColor(uint16_t color)
{
    lock();
    if (!initialized)
    {
        unlock();
        return;
    }

    tft.setTextColor(color, ST77XX_BLACK);
    unlock();
}

void AgroDisplay::setTextSize(uint8_t size)
{
    lock();
    if (size == 0)
    {
        unlock();
        return;
    }

    text_size = size;
    if (!initialized)
    {
        unlock();
        return;
    }

    tft.setTextSize(text_size);
    apply_cursor();
    unlock();
}

size_t AgroDisplay::write(uint8_t value)
{
    lock();
    if (!initialized)
    {
        unlock();
        return 0;
    }

    if (value == '\n')
    {
        cursor_col = 0;
        cursor_row++;
        apply_cursor();
        unlock();
        return 1;
    }

    size_t written = tft.write(value);
    cursor_col++;
    unlock();
    return written;
}

void AgroDisplay::lock()
{
    if (mutex != nullptr)
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

void AgroDisplay::unlock()
{
    if (mutex != nullptr)
    {
        xSemaphoreGive(mutex);
    }
}

void AgroDisplay::apply_cursor()
{
    if (!initialized)
    {
        return;
    }

    tft.setCursor(cursor_col * CHAR_WIDTH * text_size, cursor_row * LINE_HEIGHT * text_size);
}
