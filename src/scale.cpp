
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "scale.h"
#include "HX711.h"
#include "hw_config.h"
#include "pref_control.h"


float calibValue = 1;
float scaleValue = 1;
bool scaleCalibrate=0;
bool scaleTare = 0;

// HX711 Konfiguration
HX711 scale;
float weightFromQueue;
uint8_t counter;


void calibrateScale(float calWeight){
    Serial.printf("Old scale Factor: %.2f", calibValue);
    if(scaleValue > 0){
        float scaleFactor = scaleValue/calWeight;
        calibValue *= scaleFactor;
        saveCalibration(calibValue);
        scaleCalibrate = 1;
    }
    else{
        Serial.println(" Scale Value <= 0");
    }
}
   
// Queue Handle
QueueHandle_t weightQueue;

// Task für die Waage
void scaleTask(void *pvParameters) {
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(calibValue); 
    scale.tare();

    float currentWeight;
    while (true) {
        // Prüfen, ob die Waage bereit ist (ohne das System zu blockieren)
        if (scale.is_ready()) {
            if(scaleCalibrate){
                scaleCalibrate = 0;
                scale.set_scale(calibValue);
            }
            if(scaleTare){
                scaleTare = 0;
                scale.tare();
            }
            currentWeight = scale.get_units(SCALE_MEASSURES_NUM);
            // Gewicht in die Queue schreiben (nicht blockierend)
            xQueueOverwrite(weightQueue, &currentWeight);
        } else {
            Serial.println("Waage nicht gefunden - Task wartet...");
        }
        // WICHTIG: Dem Watchdog Zeit geben!
        vTaskDelay(pdMS_TO_TICKS(100)); 
        continue;
    }
}

void scale_begin(void){
  calibValue = loadCalibration();
  if(calibValue == 0) calibValue = 2000;
  if(calibValue >10000) calibValue = 2000;
  Serial.print("Kalibrierwert geladen: ");
  Serial.println(calibValue);
  scale.set_scale(calibValue);
  weightQueue = xQueueCreate(1, sizeof(float));
  //xTaskCreateStaticPinnedToCore(scaleTask, "ScaleTask", 4096, NULL, 1, NULL, NULL, 1);
  xTaskCreate(scaleTask, "ScaleTask", 4096, NULL, 1, NULL);
}

bool scale_read(float *val){
    if (xQueueReceive(weightQueue, &weightFromQueue, 0) == pdTRUE) {
        *val = weightFromQueue;
        return 1;
    }
    return 0;
}
