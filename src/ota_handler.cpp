#include "ota_handler.h"
#include <ArduinoOTA.h>
#include <secrets_config.h>

void ota_setup()
{
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
            type = "sketch";
        }
        else
        {
            type = "filesystem";
        }
        log_i("Start updating %s", type.c_str());
    });

    ArduinoOTA.onEnd([]() {
        log_i("\nEnd");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        log_i("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        log_e("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            log_e("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            log_e("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            log_e("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            log_e("Receive Failed");
        else if (error == OTA_END_ERROR)
            log_e("End Failed");
    });

    ArduinoOTA.begin();
    log_i("OTA setup complete");
}

void ota_loop()
{
    ArduinoOTA.handle();
}
