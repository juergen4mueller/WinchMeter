#include "display.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "hw_config.h"
#include "battery.h"


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


  

void display_init(void){
  u8g2.begin();
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

void display_clear(void){
    u8g2.clear();
}

bool menuActive = false;
const char* menuItems[] = {
  "TARA",
  "CAL 10kg",
  "WiFi on",
  "END"
};
int menuIndex = 0;
bool menuSelected = false;

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
    }
    u8g2.sendBuffer();
}


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
