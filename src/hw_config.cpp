#include "hw_config.h"
#include <Arduino.h>
#include <LittleFS.h>


// Initialisierung von LittleFS
void initFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Fehler beim Mounten von LittleFS");
    return;
  }
  Serial.println("LittleFS erfolgreich geladen");
}


void hw_init(void){
    initFS();
    #ifdef OLED_POWER_GND
        pinMode(OLED_POWER_GND, OUTPUT);
        digitalWrite(OLED_POWER_GND, 0);
    #endif
    #ifdef OLED_POWER_3V3
        pinMode(OLED_POWER_3V3, OUTPUT);
        digitalWrite(OLED_POWER_3V3, 1);
    #endif
    #ifdef LOADCELL_GND
        pinMode(LOADCELL_GND, OUTPUT);
        digitalWrite(LOADCELL_GND, 0);
    #endif
    #ifdef LOADCELL_3V3
        pinMode(LOADCELL_3V3, OUTPUT);
        digitalWrite(LOADCELL_3V3, 1);
    #endif
        pinMode(BTN_CTRL, INPUT);
}