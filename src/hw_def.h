#pragma once


#ifdef HELTEC_WIFI_S3
    #define BTN_CTRL 6
    #define ADC_CTL 37
    #define LOADCELL_3V3 34
    #define LOADCELL_SCK_PIN 3
    #define LOADCELL_DOUT_PIN 2
    #define OLED_RST 21
    #define OLED_CLK 18
    #define OLED_DAT 17
#endif

#ifdef SUPERMINI_C3
    #define BTN_CTRL 6
    #define ADC_CTL 5
    #define LOADCELL_SCK_PIN 3
    #define LOADCELL_DOUT_PIN 2
    #define OLED_RST -1
    #define OLED_CLK 9
    #define OLED_DAT 10
    #define OLED_POWER_GND  7
    #define OLED_POWER_3V3  8
#endif
#ifdef ESP32S3_SUPERMINI
    #define BTN_CTRL 6 
    #define LOADCELL_3V3 5
    #define LOADCELL_SCK_PIN 4
    #define LOADCELL_DOUT_PIN 3
    #define LOADCELL_GND 2
    #define ADC_CTL 7
    #define OLED_RST -1
    #define OLED_CLK 10
    #define OLED_DAT 11
    #define OLED_POWER_GND  8
    #define OLED_POWER_3V3  9
#endif
