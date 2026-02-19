#pragma once
#include <ESPAsyncWebServer.h>


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