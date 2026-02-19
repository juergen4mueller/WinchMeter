#include "hw_config.h"
#include <Arduino.h>

void hw_init(void){
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