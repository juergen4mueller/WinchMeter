#include "power.h"
#include <Arduino.h>
#include "display.h"
#include "wifi_web.h"


unsigned long scaleOffTime = 300000;

void switch_off(void){
// set wake condition
  esp_sleep_enable_ext1_wakeup(
        (1ULL << GPIO_NUM_6) ,
        ESP_EXT1_WAKEUP_ANY_LOW
    );
// goto sleep
    Serial.println("Prepare for Sleep");
#ifdef LOADCELL_3V3
    digitalWrite(LOADCELL_3V3, 0);
#endif
    display_clear();
    delay(200);
    esp_deep_sleep_start();
}

void power_manager(void){
    uint32_t now = millis();
    if(now > wifiOffTime){
        wifi_end();
    }
    if(now > scaleOffTime){
        switch_off();
    }
    if(WifiClientConnected){
        wifiOffTime = now + WIFI_OFF_DELAY;
    }
    // alte Websockets eliminieren
    ws.cleanupClients();
}


void power_off_time_reset(void){
    scaleOffTime = millis()+ autoscaleOffTimeout;
}
