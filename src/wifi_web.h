#pragma once
#include <ESPAsyncWebServer.h>

#define WIFI_OFF_DELAY  120000UL

extern bool wsConnected;
extern bool WifiClientConnected;
extern bool WifiActive;
extern uint32_t wifiOffTime;
extern 
char textBuffer[];
extern AsyncWebSocket ws;
void wifi_begin(void);
void wifi_start(void);
void wifi_end(void);
void ws_send_string(char* msg);