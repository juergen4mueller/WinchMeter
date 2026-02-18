#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "HX711.h"
#include "LittleFS.h"


#include "bt_control.h"
#include "pref_control.h"
#include "hw_def.h"
#include "battery.h"


#define DEBUG 0

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R0,     // Rotation
    /* reset=*/ OLED_RST,
    /* clock=*/ OLED_CLK,
    /* data=*/ OLED_DAT
);

// 16x16 WLAN Icon
static const unsigned char wifi_icon[] U8X8_PROGMEM = {
0b11100000, 0b00000111,
0b11111100, 0b00111111,
0b11111111, 0b11111111,
0b00011111, 0b11111000,
0b00000000, 0b00000000,
0b00000000, 0b00000000,
0b00000000, 0b00000000,
0b11100000, 0b00000111,
0b11111000, 0b00011111,
0b11110000, 0b00001111,
0b00110000, 0b00001100,
0b00000000, 0b00000000,
0b00000000, 0b00000000,
0b00000000, 0b00000000,
0b11000000, 0b00000011,
0b11000000, 0b00000011
};

static const unsigned char wifi_icon_apple[] U8X8_PROGMEM = {
  0x00,0x00,
  0xE0,0x07,
  0x38,0x1C,
  0x0C,0x30,
  0xC6,0x63,
  0xE2,0x47,
  0x70,0x0E,
  0x30,0x0C,
  0x18,0x18,
  0x08,0x10,
  0x00,0x00,
  0x00,0x00,
  0x00,0x00,
  0x00,0x00
};




bool WifiClientConnected = false;
bool WifiActive = false;

float calibValue = 1;
float scaleValue = 1;
bool scaleCalibrate=0;
bool scaleTare = 0;
bool scalePowerDown=0;

#define WEIGHT_FILTER_SIZE  12


int weightMeassures = 0;
float weightSumm = 0;

// HX711 Konfiguration
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

bool menuActive = false;
void display_write_weigth(float weight){
    if(menuActive == true){
        return;
    }
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

    if(WifiActive){
        u8g2.drawXBMP(0, 0, 16, 16, wifi_icon);
        //u8g2.drawXBMP(0, 30, 14, 14, wifi_icon_apple);
        
    }
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


uint8_t lastShown = 0;
uint32_t now = 0, nextEvent = 0, buttonLoop = 0;

unsigned long lastChange = 0;
unsigned long pressStart = 0;
unsigned long scaleOffTime = 0;
int clickCount = 0;
bool buttonState = false;
bool lastButtonState = false;

const unsigned long debounceTime = 30;
const unsigned long clickTimeout = 300;
const unsigned long longPressTime = 600;
const unsigned long veryLongPressTime = 2000;

const unsigned long autoscaleOffTimeout = 300000; // 5 Minuten, wird bei Scale > 5kg zurückgesetzt
const unsigned long wifiOffTimeout = 120000; // 2 Minuten, wird bei aktiver WLAN Verbindung + Websocket zurückgesetzt
unsigned long wifiOffTime = 120000;

void switch_off(void){
    Serial.println("Prepare for Sleep");
#ifdef LOADCELL_3V3
    digitalWrite(LOADCELL_3V3, 0);
#endif
    u8g2.clear();
    delay(200);
    esp_deep_sleep_start();
}



#define BTN_ACT_SINGL_CLICK     1
#define BTN_ACT_DBL_CLICK       2
#define BTN_ACT_TRBL_CLICK      3
#define BTN_ACT_LONGPRESS       4

// -------------------------------------------------------------
// MENU
// -------------------------------------------------------------
const char* menuItems[] = {
  "TARA",
  "CAL 10kg",
  "WiFi on",
  "END"
};
const int MENU_COUNT = 4;

int menuIndex = 0;
bool menuSelected = false;

// -------------------------------------------------------------
// DISPLAY FUNKTION
// -------------------------------------------------------------
void drawMenu(void) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tf);

    for (int i = 0; i < MENU_COUNT; i++) {
        if (i == menuIndex) {
        u8g2.drawBox(0, i * 14, 128, 14);
        u8g2.setDrawColor(0);
        u8g2.drawStr(4, i * 14 + 11, menuItems[i]);
        u8g2.setDrawColor(1);
        } else {
        u8g2.drawStr(4, i * 14 + 11, menuItems[i]);
        }
    }

    if (menuSelected) {
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(0, 63, "Ausgewaehlt!");
    }

    u8g2.sendBuffer();
}

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
#ifdef LOADCELL_GND
    pinMode(LOADCELL_GND, OUTPUT);
    digitalWrite(LOADCELL_GND, 0);
#endif
#ifdef LOADCELL_3V3
    pinMode(LOADCELL_3V3, OUTPUT);
    digitalWrite(LOADCELL_3V3, 1);
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
  WifiActive = true;
  
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
  if(calibValue == 0) calibValue = 2000;
  if(calibValue >10000) calibValue = 2000;
  Serial.print("Kalibrierwert geladen: ");
  Serial.println(calibValue);

  scale.set_scale(calibValue);

  xTaskCreate(scaleTask, "ScaleTask", 4096, NULL, 1, NULL);
  
  init_bluetooth();
  delay(100);

  scaleOffTime = millis()+autoscaleOffTimeout;

  esp_sleep_enable_ext1_wakeup(
        (1ULL << GPIO_NUM_6) ,
        ESP_EXT1_WAKEUP_ANY_LOW
    );
    delay(100);
    scaleTare = true;

}




void loop() {
  now = millis();
  if(now > scaleOffTime){
    switch_off();
  }
  if(wsConnected){
    wifiOffTime = now + wifiOffTimeout;
  }
  if(now > wifiOffTime){
    WiFi.softAPdisconnect(true); // AP aus 
    WiFi.mode(WIFI_OFF); // optional: Funk komplett aus
    WifiActive = false;
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
                    WiFi.onEvent(WiFiEvent);
                    WiFi.mode(WIFI_AP);
                    WiFi.softAP(ssid, password, 10);
                    delay(10);
                    WifiActive = true;
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
