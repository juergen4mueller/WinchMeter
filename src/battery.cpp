#include <Arduino.h>
#include "hw_config.h"


#define ADC_IN  1
#define U_IN_Teiler 390 / 100


uint8_t batt_soc; 
float battVolt;

/*
12 Bit entsprechen 1,1V
LSB = 1,1V / 4096 = 0,26855 mV
Spannungsteiler mit Faktor 4,9 -> LSB = 1,3159 mV
-> ADC Value * 1,3159 = Spannung in mV
-> ADC Value / 759 = Spannung in V
*/
int get_lipo_percent(float voltage) {
  if (voltage >= 4.10) return 100;
  if (voltage >= 4.03) return 90;
  if (voltage >= 3.95) return 80;
  if (voltage >= 3.89) return 70;
  if (voltage >= 3.84) return 60;
  if (voltage >= 3.80) return 50;
  if (voltage >= 3.77) return 40;
  if (voltage >= 3.75) return 30;
  if (voltage >= 3.72) return 20;
  if (voltage >= 3.65) return 10;
  if (voltage <= 3.40) return 0;
  return 0;
}

void batt_meassure_init(void){

  analogReadResolution(12); // Werte von 0 bis 4095
  analogSetAttenuation(ADC_0db); // max. 1,1V

  pinMode(ADC_IN, INPUT);
  pinMode(ADC_CTL, OUTPUT);
  digitalWrite(ADC_CTL, 0);
}

void batt_voltage_read(void){
  digitalWrite(ADC_CTL, 1);
  delay(1);
  uint32_t pin_mv = 0;
  for(int i=0;i<10;i++){
    pin_mv += analogReadMilliVolts(ADC_IN);
  }
  uint32_t batt_mv = pin_mv * 0.49;
  battVolt = batt_mv / 1000.0;
  batt_soc = get_lipo_percent(battVolt);
 // printf("V= %d mV SOC:%d\n", batt_mv, batt_soc);
  digitalWrite(ADC_CTL, 0);
}
