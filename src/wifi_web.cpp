#include "wifi_web.h"
#include "hw_config.h"
#include "scale.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "hw_config.h"
#include "scale.h"


bool wsConnected = false;
bool scaleRunning = false;

bool WifiClientConnected = false;
bool WifiActive = false;

float maxScaleValue = 0;

uint32_t wifiOffTime = 120000;

// Netzwerk-Objekte
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char* ssid = "Waage";
const char* password = "123456789";
char textBuffer[25];
int i;
// WebSocket Event Handler
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if(type == WS_EVT_CONNECT){
        wsConnected = true;
        Serial.println("WS connected");
    }
    else if(type == WS_EVT_DISCONNECT){
        wsConnected = false;
        Serial.println("WS disconnected!");
    }
    else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        // Hier war der Fehler: Der Vergleich muss auf den Opcode im Frame schauen
        if (info->final && info->index == 0 && info->len == len && info->opcode == 0x01) { // 0x01 ist Text
            data[len] = 0;
            if(DEBUG){
                Serial.printf("WS len: %d Data: %s", len, data);
            }
            if (strncmp((char*)data, "tare", len) == 0) {
               // scale.tare();
                Serial.println("Tara ausgeführt");
                scaleTare = 1;
            }
            if (strncmp((char*)data, "start", len) == 0) {
               // starte messung
                Serial.println("start");
                scaleRunning = true;
            }
            if (strncmp((char*)data, "stopp", len) == 0) {
               // stoppe messung
                Serial.println("stopp");
                scaleRunning = false;
            }
            else if (strncmp((char*)data, "calib", 5) == 0) {
                int numbers = len -6;
                int i;
                for(i=0;i<numbers;i++){
                    textBuffer[i]= data[6+i];
                }
                textBuffer[numbers]= 0;
                String calString = String(textBuffer);
                float calWeight = calString.toFloat();
               // scale.tare();
               if(DEBUG){
                    Serial.print("Calibration Value: ");
                    Serial.print(calWeight);
                    Serial.print("Scale Value:");
                    Serial.print(scaleValue);
               }
               calibrateScale(calWeight);
            }
        }
    }
}



void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            WifiClientConnected = true;
            Serial.printf("Client connected: MAC %02X:%02X:%02X:%02X:%02X:%02X, AID=%d\n",
                info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5],
                info.wifi_ap_staconnected.aid
            );
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            WifiClientConnected = false;
            Serial.printf("Client disconnected: MAC %02X:%02X:%02X:%02X:%02X:%02X, AID=%d\n",
                info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
                info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5],
                info.wifi_ap_stadisconnected.aid
            );
            break;
    }
}

void wifi_begin(void){
  //esp_wifi_set_max_tx_power(40);
  //WiFi.setSleep(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password, 10, 0, 1, false);
  delay(100);
  WifiActive = true;
  
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  // STATISCHE DATEIEN SERVIEREN
  // Das ersetzt die manuellen Routen. Der ESP sucht die Datei im /data Ordner
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.begin();

}

void wifi_start(void);
void wifi_end(void){
    WiFi.softAPdisconnect(true); // AP aus 
    WiFi.mode(WIFI_OFF); // optional: Funk komplett aus
    WifiActive = false;
}
void ws_send_string(char* msg){
    if(wsConnected && scaleRunning && WifiClientConnected){
        ws.textAll(String(msg));
    }
}