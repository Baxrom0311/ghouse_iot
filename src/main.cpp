#include <WiFi.h>
#include <mqtt_handler.h>
#include <io_handler.h>
#include <states.h>
#include <display_handler.h>
#include <secrets_config.h>
#include <ota_handler.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>
#include <time.h>

namespace
{
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;
constexpr uint32_t TLS_TIME_SYNC_TIMEOUT_MS = 15000;
constexpr uint32_t WATCHDOG_TIMEOUT_SEC = 45;
constexpr UBaseType_t MQTT_TO_IO_QUEUE_LENGTH = 16;
constexpr UBaseType_t IO_TO_MQTT_QUEUE_LENGTH = 32;
uint32_t last_wifi_reconnect_attempt = 0;
bool mqtt_task_started = false;

void mqtt_task(void *parameter)
{
  esp_task_wdt_add(NULL);
  mqtt_setup();
  for (;;)
  {
    esp_task_wdt_reset();
    mqtt_loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void io_task(void *parameter)
{
  esp_task_wdt_add(NULL);
  for (;;)
  {
    esp_task_wdt_reset();
    io_loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void connect_wifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t wifi_connect_started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - wifi_connect_started < WIFI_CONNECT_TIMEOUT_MS)
  {
    esp_task_wdt_reset();
    delay(500);
    log_i(".");
  }
  wifi_connected = (WiFi.status() == WL_CONNECTED);
  if (!wifi_connected)
  {
    log_w("Wi-Fi connection timed out, continuing offline");
  }
}

void sync_time_for_tls()
{
#if MQTT_TLS_ENABLED
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  uint32_t sync_started = millis();
  while (time(nullptr) < 1700000000 &&
         millis() - sync_started < TLS_TIME_SYNC_TIMEOUT_MS)
  {
    esp_task_wdt_reset();
    delay(500);
    log_i("Waiting for NTP time sync...");
  }

  if (time(nullptr) < 1700000000)
  {
    log_w("NTP time sync timed out; MQTT TLS may fail");
    return;
  }
  log_i("NTP time synced for MQTT TLS");
#endif
}

void keep_wifi_connected()
{
  bool was_connected = wifi_connected;
  wifi_connected = (WiFi.status() == WL_CONNECTED);

  if (wifi_connected)
  {
    if (!was_connected)
    {
      log_i("Wi-Fi reconnected");
    }
    return;
  }

  if (millis() - last_wifi_reconnect_attempt < WIFI_RECONNECT_INTERVAL_MS)
  {
    return;
  }

  last_wifi_reconnect_attempt = millis();
  log_w("Wi-Fi disconnected, reconnecting");
  WiFi.disconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}
} // namespace

void setup()
{
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t wdt_config = {};
  wdt_config.timeout_ms = WATCHDOG_TIMEOUT_SEC * 1000;
  wdt_config.idle_core_mask = (1 << portNUM_PROCESSORS) - 1;
  wdt_config.trigger_panic = true;
  esp_task_wdt_init(&wdt_config);
#else
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);
#endif
  esp_task_wdt_add(NULL);
  delay(1000);
  io_setup();
  lcd.init();
  lcd.backlight();
  load_settings();

  // Queues yaratish (io_task uchun kerak)
  mqttToIo = xQueueCreate(MQTT_TO_IO_QUEUE_LENGTH, sizeof(Command));
  ioToMqtt = xQueueCreate(IO_TO_MQTT_QUEUE_LENGTH, sizeof(Callback));
  if (mqttToIo == nullptr || ioToMqtt == nullptr)
  {
    log_e("Failed to create FreeRTOS queues");
    ESP.restart();
  }

  // IO task ni darhol ishga tushirish (WiFi siz ham ishlaydi)
  xTaskCreatePinnedToCore(io_task, "io_task", 8192, nullptr, 2, nullptr, 1);

  // WiFi ulanishga harakat
  lcd.setCursor(0, 0);
  lcd.print("Wi-Fi...");
  log_i("Connecting...");
  connect_wifi();

  if (wifi_connected)
  {
    log_i("Connected");
    sync_time_for_tls();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wi-Fi OK");
    lcd.setCursor(0, 1);
    lcd.print("MQTT...");
    delay(200);
    xTaskCreatePinnedToCore(mqtt_task, "mqtt_task", 8192, nullptr, 2, nullptr, 0);
    ota_setup();
    mqtt_task_started = true;
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("OFFLINE");
    lcd.setCursor(0, 1);
    lcd.print("Sensors OK");
    delay(1000);
  }
}

void loop()
{
  esp_task_wdt_reset();
  keep_wifi_connected();

  // WiFi qayta ulanganda MQTT task ni ishga tushirish
  if (wifi_connected && !mqtt_task_started)
  {
    sync_time_for_tls();
    xTaskCreatePinnedToCore(mqtt_task, "mqtt_task", 8192, nullptr, 2, nullptr, 0);
    ota_setup();
    mqtt_task_started = true;
    log_i("MQTT task started after WiFi connected");
  }

  if (wifi_connected)
  {
    ota_loop();
  }
  vTaskDelay(pdMS_TO_TICKS(50));
}
