#include <Arduino.h>
#include "bluetooth.h"
#include "pref_control.h"
#include "hw_config.h"
#include "battery.h"
#include "display.h"
#include "power.h"
#include "scale.h"
#include "wifi_web.h"
#include "buttons.h"


void setup() {
  Serial.begin(115200);
  hw_init(); // IOs entsprechend der Hardware setzen und initialisieren
  display_init(); // 
  batt_meassure_init();
  wifi_begin();
  scale_begin();  
  init_bluetooth();
  delay(100);
  scaleTare = true; // nochmal Tara nach einschwingen
}


uint32_t now, nextEvent;

void loop() {
  now = millis();
  if(!(now % 30)){ // alle 30ms
    buttons_proc();
    power_manager();
  }
  
  if(!(now % 1000)){
    batt_voltage_read();
    sprintf(textBuffer, "B:%.2fV SOC:%d", battVolt, batt_soc);
    Serial.print(textBuffer);
    Serial.println();
    ws_send_string(textBuffer);
  }


  // Prüfen, ob ein neuer Wert in der Queue liegt
  if(scale_read(&scaleValue)){
    if(fabs(scaleValue) < 0.1)scaleValue = 0; // zappeln um 0 ausblenden
    Serial.printf("New scale value: %.2f\r\n", scaleValue);
    display_write_weigth(scaleValue);
    sprintf(textBuffer, "W:%.2f", scaleValue);
    ws_send_string(textBuffer);
    bluetooth_update_scale_value(scaleValue);
    if(scaleValue > 2.0){
      power_off_time_reset();
    }
  }
}
