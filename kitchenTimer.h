#pragma once
#include <Arduino.h>
#include <DS3231.h> 

// #define USE_LIGHT_SENSOR // использовать или нет датчик света для регулировки яркости дисплея

#define DISPLAY_MODE_SHOW_TIME 0  // основной режим - вывод времени на индикатор
#define DISPLAY_MODE_SET_HOUR 1   // режим настройки часов
#define DISPLAY_MODE_SET_MINUTE 2 // режим настройки минут

#define BTN_SET_PIN 4
#define BTN_UP_PIN 9
#define BTN_DOWN_PIN 6

// ==== опрос кнопок =================================
void checkButton();
void checkSetButton();
void checkUpButton();
void checkDownButton();

// ==== задачи =======================================
void blink();
void restartBlink();
void returnToDefMode();
#ifdef USE_LIGHT_SENSOR
void setBrightness();
#endif

// ==== часы =========================================
// вывод времени на индикатор
void showTime(DateTime dt, bool force = false);
void showTime(byte hour, byte minute, bool force = false);
