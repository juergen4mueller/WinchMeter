#include <Arduino.h>
#include "LittleFS.h"
#include "bluetooth.h"
#include "pref_control.h"
#include "hw_config.h"
#include "battery.h"
#include "display.h"
#include "power.h"
#include "scale.h"
#include "wifi_web.h"
#include "buttons.h"


// Initialisierung von LittleFS
void initFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Fehler beim Mounten von LittleFS");
    return;
  }
  Serial.println("LittleFS erfolgreich geladen");
}



void setup() {
  Serial.begin(115200);
  initFS(); // Wichtig: Zuerst das Dateisystem starten
  hw_init(); // IOs entsprechend der Hardware setzen und initialisieren
  display_init(); // 
  // WiFi Access Point
  batt_meassure_init();
  wifi_begin();
  scale_begin();
  //esp_wifi_set_max_tx_power(40);
  //WiFi.setSleep(true);
  
  // Waage initialisieren// Queue erstellen (Platz für 1 Float-Wert)
  // Task starten
  // Name, Stack-Größe, Priorität, Handle, Kern (beim C3 immer 0)


  
  init_bluetooth();
  delay(100);

  scaleOffTime = millis()+autoscaleOffTimeout;

    delay(100);
    scaleTare = true;

}


uint32_t now, nextEvent;

void loop() {
  now = millis();
  if(!(now % 30)){ // alle 30ms
    buttons_proc();
  }

  if(now > scaleOffTime){
    switch_off();
  }
  if(wsConnected){
    wifiOffTime = now + wifiOffTimeout;
  }
  if(now > wifiOffTime){
    wifi_end();
  }
  
  if(!(now % 1000)){
    batt_voltage_read();
    sprintf(textBuffer, "B:%.2fV SOC:%d", battVolt, batt_soc);
    Serial.print(textBuffer);
    Serial.println();
    ws_send_string(textBuffer);
  }


  // Prüfen, ob ein neuer Wert in der Queue liegt
  float scaleResult;
  if(scale_read(&scaleResult)){
    if(fabs(scaleResult) < 0.1)scaleResult = 0; // zappeln um 0 ausblenden
    Serial.printf("New scale value: %.2f\r\n", scaleResult);
    display_write_weigth(scaleResult);
    sprintf(textBuffer, "W:%.2f", scaleResult);
    ws_send_string(textBuffer);
    if(scaleResult > 2.0){
      power_off_time_reset();
    }
  }
  ws.cleanupClients();
}
