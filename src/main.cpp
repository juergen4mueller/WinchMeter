#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "HX711.h"
#include "LittleFS.h"
#include <Preferences.h>
#include <NimBLEDevice.h>

#define DEBUG 0

bool WifiClientConnected = false;

NimBLEAdvertising* pAdvertising;
static NimBLECharacteristic* notifyChar;
static NimBLECharacteristic* writeChar;

class CmdCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) {
        std::string val = c->getValue();
        if (val.empty()) return;

        uint8_t cmd = val[0];
        Serial.printf("Received command: 0x%02X\n", cmd);

    //     switch (cmd) {
    //         case 0x02:
    //             Serial.println("TARE command received");
    //             // TODO: tare logic
    //             break;

    //         case 0x03:
    //             Serial.println("UNIT CHANGE command received");
    //             // TODO: unit change logic
    //             break;

    //         case 0x04:
    //             Serial.println("HOLD command received");
    //             // TODO: hold logic
    //             break;

    //         default:
    //             Serial.printf("Unknown command: 0x%02X\n", cmd);
    //     }
     }
};

// A3B8C4F4-1298-D5A4-5191-0A0D7DEA7C0A: IF_B7
// <0100> 0203 112A C019 1124 4301 0253 01F4 A144 30
void init_bt_ad(void) {
    NimBLEDevice::init("IF_B7");

    // Server wird benötigt, auch wenn du keine Services nutzt
    NimBLEServer* server = NimBLEDevice::createServer();

    // Advertising-Objekt nur EINMAL holen
    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setMinInterval(50);
    pAdvertising->setMaxInterval(100);

    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName("IF_B7");
    advData.addServiceUUID("A3B8C4F4-1298-D5A4-5191-0A0D7DEA7C0A");

    // Start-Payload
    std::string payload;
    payload.push_back(0x01);
    payload.push_back(0x00);
    payload.push_back(0x02);
    payload.push_back(0x03);
    payload.push_back(0x11);
    payload.push_back(0x2A);
    payload.push_back(0xC0);
    payload.push_back(0x19);
    payload.push_back(0x11);
    payload.push_back(0x24);
    payload.push_back(0x43);
    payload.push_back(0x01);
    payload.push_back(0x00);
    payload.push_back(0x00);
    payload.push_back(0x01);
    payload.push_back(0xF4);
    payload.push_back(0xA1);
    payload.push_back(0x44);
    payload.push_back(0x30);

    advData.setManufacturerData(payload);

    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();

    Serial.println("BLE Advertising started");
}

uint32_t nextBtAdd = 0;

void bt_set_ad(float scValue) {
    uint32_t now = millis();
    if (now < nextBtAdd || WifiClientConnected) return;

    nextBtAdd = now + 100;
    if(DEBUG >= 1){
        Serial.printf("BT send %.2f\r\n", scValue);
    }    
    if(scValue < 0) scValue = 0;
    uint16_t raw = scValue * 100;

    NimBLEAdvertisementData advData;

    // Nur Manufacturer-Data aktualisieren!
    std::string payload;
    payload.push_back(0x01);
    payload.push_back(0x00);
    payload.push_back(0x02);
    payload.push_back(0x03);
    payload.push_back(0x11);
    payload.push_back(0x2A);
    payload.push_back(0xC0);
    payload.push_back(0x19);
    payload.push_back(0x11);
    payload.push_back(0x24);
    payload.push_back(0x43);
    payload.push_back(0x01);
    payload.push_back((raw >> 8) & 0xFF);
    payload.push_back(raw & 0xFF);
    payload.push_back(0x01);
    payload.push_back(0xF4);
    payload.push_back(0xA1);
    payload.push_back(0x44);
    payload.push_back(0x30);

    advData.setManufacturerData(payload);

    // Advertising läuft bereits → nur Daten ersetzen
    pAdvertising->setAdvertisementData(advData);
}



float calibValue = 1;
float scaleValue = 1;
bool scaleCalibrate=0;
bool scaleTare = 0;

#define WEIGHT_FILTER_SIZE  12
int weightMeassures = 0;
float weightSumm = 0;

Preferences prefs;

void saveCalibration(float value) {
    prefs.begin("waage", false);   // Namespace "waage"
    prefs.putFloat("calib", value);
    prefs.end();
}

float loadCalibration() {
    prefs.begin("waage", true);    // read-only
    float value = prefs.getFloat("calib", 2000.0f);  // 0.0 = Default
    prefs.end();
    return value;
}

// HX711 Konfiguration
const int LOADCELL_SCK_PIN = 3;
const int LOADCELL_DOUT_PIN = 2;
HX711 scale;
float weightFromQueue;
uint8_t counter;

// Netzwerk-Objekte
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char* ssid = "Waage";
const char* password = "123456789";

char textBuffer[25];
int i;
bool scaleRunning = false;
bool wsConnected;

void calibrateScale(float calWeight){
    if(scaleValue > 0){
        float scaleFactor = scaleValue/calWeight;
        calibValue *= scaleFactor;
        saveCalibration(calibValue);
        scaleCalibrate = 1;
    }
}
    

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
                counter = 0; 
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
            currentWeight = scale.get_units(16);
            // Gewicht in die Queue schreiben (nicht blockierend)
            xQueueOverwrite(weightQueue, &currentWeight);
        } else {
            Serial.println("Waage nicht gefunden - Task wartet...");
        }
        // WICHTIG: Dem Watchdog Zeit geben!
        vTaskDelay(pdMS_TO_TICKS(50)); 
        continue;
    }
}

// Initialisierung von LittleFS
void initFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Fehler beim Mounten von LittleFS");
    return;
  }
  Serial.println("LittleFS erfolgreich geladen");
}

#ifdef HELTEC_WIFI_S3
    #define ADC_CTL 37
#elif SUPERMINI_C3
    #define ADC_CTL 5
#endif

#define ADC_IN  1
#define U_IN_Teiler 390 / 100

/*
12 Bit entsprechen 1,1V
LSB = 1,1V / 4096 = 0,26855 mV
Spannungsteiler mit Faktor 4,9 -> LSB = 1,3159 mV
-> ADC Value * 1,3159 = Spannung in mV
-> ADC Value / 759 = Spannung in V
*/
int get_lipo_percent(float voltage) {
  if (voltage >= 4.2) return 100;
  if (voltage >= 4.05) return 90;
  if (voltage >= 3.97) return 80;
  if (voltage >= 3.91) return 70;
  if (voltage >= 3.86) return 60;
  if (voltage >= 3.81) return 50;
  if (voltage >= 3.78) return 40;
  if (voltage >= 3.76) return 30;
  if (voltage >= 3.73) return 20;
  if (voltage >= 3.67) return 10;
  if (voltage <= 3.30) return 0;
  return 0;
}
void batt_meassure_init(void){

  analogReadResolution(12); // Werte von 0 bis 4095
  analogSetAttenuation(ADC_0db); // max. 1,1V

  pinMode(ADC_IN, INPUT);
  pinMode(ADC_CTL, OUTPUT);
  digitalWrite(ADC_CTL, 0);
}

uint8_t batt_soc; 
float battVolt;
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

#ifdef HELTEC_WIFI_S3
    // Heltec Display, Pins, etc.
    // Heltec WiFi Kit V3: SSD1306 128x64 I2C
    // SDA = GPIO 4, SCL = GPIO 5
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
        U8G2_R0,     // Rotation
        /* reset=*/ 21,
        /* clock=*/ 18,
        /* data=*/ 17
    );
#elif SUPERMINI_C3
    // C3 SuperMini Pins
    // Heltec Display, Pins, etc.
    // Heltec WiFi Kit V3: SSD1306 128x64 I2C
    // SDA = GPIO 4, SCL = GPIO 5
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
        U8G2_R0,     // Rotation
        /* reset=*/ -1,
        /* clock=*/ 9,
        /* data=*/ 10
    );
    #define     OLED_POWER_GND  7
    #define     OLED_POWER_3V3  8
#endif




uint32_t lastDisplayValueTime = 0;
void display_init(void){
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "-.-kg");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso28_tr);

  // Textbreite berechnen, damit es zentriert ist
  int textWidth = u8g2.getStrWidth(buffer);
  int x = (128 - textWidth) / 2;
  int y = 52;  // gute vertikale Position für 32px Font
  u8g2.drawStr(x, y, buffer);

  snprintf(buffer, sizeof(buffer), "SOC %d %% ", batt_soc);
  u8g2.setFont(u8g2_font_logisoso16_tr);
  textWidth = u8g2.getStrWidth(buffer);
  x = 128 - textWidth;
  y = 18; 
  u8g2.drawStr(x, y, buffer);
  u8g2.sendBuffer();
}

void display_write_weigth(float weight){
  lastDisplayValueTime = millis();
  char buffer[16];

  u8g2.clearBuffer();
  snprintf(buffer, sizeof(buffer), "%.2f", weight);
  u8g2.setFont(u8g2_font_logisoso28_tr);

  // Textbreite berechnen, damit es zentriert ist
  int textWidth = u8g2.getStrWidth(buffer);
  int x = (128 - textWidth) / 2;
  int y = 52;  // gute vertikale Position für 32px Font
  u8g2.drawStr(x, y, buffer);

  snprintf(buffer, sizeof(buffer), "SOC %d %% ", batt_soc);
  u8g2.setFont(u8g2_font_logisoso16_tr);
  textWidth = u8g2.getStrWidth(buffer);
  x = 128 - textWidth;
  y = 18; 
  u8g2.drawStr(x, y, buffer);

  u8g2.sendBuffer();
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

#define BTN_CTRL 6 // Button mit PullUp, unterscheidung zwischen Click, Dobbelclick, Longclick oder Menü
                    // Einschalten, Ausschalten, Tara, Kalibrierung

void setup() {
  Serial.begin(115200);
  initFS(); // Wichtig: Zuerst das Dateisystem starten

#ifdef OLED_POWER_GND
    pinMode(OLED_POWER_GND, OUTPUT);
    digitalWrite(OLED_POWER_GND, 0);
#endif
#ifdef OLED_POWER_3V3
    pinMode(OLED_POWER_3V3, OUTPUT);
    digitalWrite(OLED_POWER_3V3, 1);
#endif

    pinMode(BTN_CTRL, INPUT);

  // WiFi Access Point
  batt_meassure_init();
  u8g2.begin();
  display_init();

  //esp_wifi_set_max_tx_power(40);
  //WiFi.setSleep(true);
  
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password, 10);
  delay(100);
  
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // STATISCHE DATEIEN SERVIEREN
  // Das ersetzt die manuellen Routen. Der ESP sucht die Datei im /data Ordner
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  server.begin();

  // Waage initialisieren// Queue erstellen (Platz für 1 Float-Wert)
  weightQueue = xQueueCreate(1, sizeof(float));
  // Task starten
  // Name, Stack-Größe, Priorität, Handle, Kern (beim C3 immer 0)

  calibValue = loadCalibration();
  Serial.print("Kalibrierwert geladen: ");
  Serial.println(calibValue);

  scale.set_scale(calibValue);

  xTaskCreate(scaleTask, "ScaleTask", 4096, NULL, 1, NULL);
  
  init_bt_ad();
  delay(100);

  esp_sleep_enable_ext1_wakeup(
        (1ULL << GPIO_NUM_6) ,
        ESP_EXT1_WAKEUP_ANY_LOW
    );


}



uint8_t lastShown = 0;
uint32_t now = 0, nextEvent = 0, buttonLoop = 0;
#define BUTTON_PIN 0

unsigned long lastChange = 0;
unsigned long pressStart = 0;
int clickCount = 0;
bool buttonState = false;
bool lastButtonState = false;

const unsigned long debounceTime = 30;
const unsigned long clickTimeout = 300;
const unsigned long longPressTime = 600;
const unsigned long veryLongPressTime = 2000;


void loop() {
  now = millis();
  if(now > nextEvent){
    batt_voltage_read();
    if(wsConnected){
        sprintf(textBuffer, "B:SOC:%d%", battVolt, batt_soc);
        Serial.print(textBuffer);
        Serial.println();
        ws.textAll(String(textBuffer));
    }
    nextEvent = now + 1000;
    if((now - lastDisplayValueTime) > 1000){
      display_init();
    }
  }

  if(now >= buttonLoop){
    buttonLoop = now + debounceTime;
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
                Serial.println("Prepare for Sleep");
                u8g2.clear();
                delay(200);
                esp_deep_sleep_start();
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
            Serial.println("Single Click"); // Tara Waage
            scaleTare = true;
        } else if (clickCount == 2) {
            Serial.println("Double Click"); // Kalibrieren mit 10kg
           // calibrateScale(10.0);
        } else if (clickCount == 3) {
            Serial.println("Triple Click");
        } else {
            Serial.printf("%d Clicks\n", clickCount);
        }

        // WICHTIG: Immer zurücksetzen!
        clickCount = 0;
    }
  }

  // Prüfen, ob ein neuer Wert in der Queue liegt
  if (xQueueReceive(weightQueue, &weightFromQueue, 0) == pdTRUE) {
      // Wert per WebSocket senden
      scaleValue = weightFromQueue;
      if(fabs(scaleValue)< 0.08) scaleValue = 0;
      display_write_weigth(scaleValue);
      bt_set_ad(scaleValue);
      if((scaleRunning) && (wsConnected)){
          Serial.printf("SV: %.2f %08d\r\n", scaleValue, millis());
          ws.textAll("W:"+String(scaleValue, 2));
      }
  }

  ws.cleanupClients();
}
