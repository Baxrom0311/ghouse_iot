#include <WiFi.h>
#include <mqtt_handler.h>
#include <io_handler.h>
#include <states.h>
#include <display_handler.h>
#include <secrets_config.h>

void setup()
{
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
  delay(1000);
  io_setup();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("Wi-Fi");
  delay(2000);
  load_settings();
  log_i("Connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t wifi_connect_started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - wifi_connect_started < 20000)
  {
    delay(500);
    log_i(".");
    lcd.print(".");
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wi-Fi failed");
    lcd.setCursor(0, 1);
    lcd.print("Restarting...");
    log_e("Wi-Fi connection timed out");
    delay(3000);
    ESP.restart();
  }
  log_i("Connected");
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("Wi-Fi");
  lcd.setCursor(0, 1);
  lcd.print("Connected");
  delay(200);
  mqttToIo = xQueueCreate(10, sizeof(Command));
  ioToMqtt = xQueueCreate(10, sizeof(Callback));
  mqtt_setup();
}

void loop()
{
  mqtt_loop();
  io_loop();
  // vTaskDelay(1);
}
