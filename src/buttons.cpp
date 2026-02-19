#include "buttons.h"
#include <Arduino.h>
#include <hw_config.h>
#include "power.h"
#include "display.h"
#include "wifi_web.h"
#include "display.h"
#include "scale.h"



unsigned long lastChange = 0;
unsigned long pressStart = 0;
int clickCount = 0;
bool buttonState = false;
bool lastButtonState = false;

const unsigned long debounceTime = 30;
const unsigned long clickTimeout = 300;
const unsigned long longPressTime = 600;
const unsigned long veryLongPressTime = 2000;

void buttons_proc(void){
    uint32_t now = millis();
    bool reading = !digitalRead(BTN_CTRL);
    
    if (reading != buttonState) {
        buttonState = reading;
        lastChange = now;
        if (buttonState) {
            // Button pressed
            pressStart = now;
        } 
        else {
            // Button released
            unsigned long pressDuration = now - pressStart;
            if (pressDuration > veryLongPressTime) {
                Serial.println("Very Long Press");
                switch_off();
            } else if (pressDuration > longPressTime) {
                Serial.println("Long Press");
            } else {
                clickCount++;
            }
        }
    }

    // Click-Auswertung nach Timeout
    if (!buttonState && clickCount > 0 && (now - lastChange) > clickTimeout) {

        if (clickCount == 1) {
            Serial.println("Single Click"); // Mneü weiterblättern
            if(menuActive){
                menuIndex ++;
                if(menuIndex >= MENU_COUNT){
                    menuIndex = 0;
                }
            }
        } else if (clickCount == 2) {
            Serial.println("Double Click"); // Menü bestätigen
            if(menuActive){
                if(menuIndex == 0){
                    scaleTare = true;
                }
                else if(menuIndex == 1){
                    calibrateScale(10.0);
                }
                else if(menuIndex == 2){
                    wifiOffTime = now + wifiOffTimeout;
                    wifi_begin();
                }
                menuActive = false;
            }

        } else if (clickCount == 3) {
            Serial.println("Triple Click"); // Menü starten
            if(menuActive == false){
                menuActive = true;
                menuIndex = 0;
            }
        } else {
            Serial.printf("%d Clicks\n", clickCount);
        }

        // WICHTIG: Immer zurücksetzen!
        clickCount = 0;
    }
    if(menuActive){
        drawMenu();
    }
  }