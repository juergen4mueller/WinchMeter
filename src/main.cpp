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
  hw_init();
  display_init();
  // WiFi Access Point
  batt_meassure_init();

  wifi_begin();
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
  if(now > scaleOffTime){
    switch_off();
  }
  if(wsConnected){
    wifiOffTime = now + wifiOffTimeout;
  }
  if(now > wifiOffTime){
    wifi_end();
  }
  
  if(now > nextEvent){
    nextEvent = now + 1000;
    batt_voltage_read();
    if(wsConnected){
        sprintf(textBuffer, "B:%.2fV SOC:%d", battVolt, batt_soc);
        Serial.print(textBuffer);
        Serial.println();
        ws.textAll(String(textBuffer));
    }
  }


  // Prüfen, ob ein neuer Wert in der Queue liegt
  if (xQueueReceive(weightQueue, &weightFromQueue, 0) == pdTRUE) {
      // Wert per WebSocket senden
      scaleValue = weightFromQueue;
      if(scaleValue > 2.0){
        scaleOffTime = now + autoscaleOffTimeout;
      }
      if(fabs(scaleValue)< 0.08) scaleValue = 0;
      display_write_weigth(scaleValue);
      bluetooth_update_scale_value(scaleValue);
      if((scaleRunning) && (wsConnected)){
          Serial.printf("SV: %.2f %08d\r\n", scaleValue, millis());
          ws.textAll("W:"+String(scaleValue, 2));
      }
  }

  ws.cleanupClients();
}
