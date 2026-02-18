#pragma once
#include <Arduino.h>


extern uint8_t batt_soc; 
extern float battVolt;


void batt_meassure_init(void);
void batt_voltage_read(void);