#include <display_handler.h>

// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
LiquidCrystal_I2C lcd(0x27, 16, 2);