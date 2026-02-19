#pragma once

extern bool WifiActive;


#define  MENU_COUNT 4
extern bool menuActive;
extern const char* menuItems[];
extern int menuIndex;
extern bool menuSelected;

void display_init(void);
void display_clear(void);
void display_write_weigth(float weight);
void drawMenu(void);