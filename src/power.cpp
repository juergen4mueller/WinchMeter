#include "power.h"
#include <Arduino.h>
#include "display.h"


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
