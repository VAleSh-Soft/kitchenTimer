#pragma once
#include <Arduino.h>
#include <DS3231.h>

// #define USE_LIGHT_SENSOR // использовать или нет датчик света для регулировки яркости дисплея

#define DISPLAY_MODE_SHOW_TIME 0  // основной режим - вывод времени на индикатор
#define DISPLAY_MODE_SET_HOUR 1   // режим настройки часов
#define DISPLAY_MODE_SET_MINUTE 2 // режим настройки минут
#define DISPLAY_MODE_SHOW_TEMP 3  // режим вывода температуры

#define BTN_FLAG_NONE 0 // флаг кнопки - ничего не делать
#define BTN_FLAG_NEXT 1 // флаг кнопки - изменить значение
#define BTN_FLAG_EXIT 2 // флаг кнопки - возврат в режим показа текущего времени

#define BTN_SET_PIN 4
#define BTN_UP_PIN 9
#define BTN_DOWN_PIN 6
#ifdef USE_LIGHT_SENSOR
#define LIGHT_SENSOR_PIN A0
#endif

// ==== опрос кнопок =================================
void checkButton();
void checkSetButton();
void checkUpDownButton();

// ==== задачи =======================================
void blink();
void restartBlink();
void returnToDefMode();
void showTimeSetting();
#ifdef USE_LIGHT_SENSOR
void setBrightness();
#endif

// ==== часы =========================================
// вывод времени на индикатор
void showTime(DateTime dt, bool force = false);
void showTime(byte hour, byte minute, bool force = false);
// вывод на экран данных в режиме настройки времени
void showTimeSettingData(byte hour, byte minute);
// сохранение времени после настройки
void saveTime(byte hour, byte minute);

// ==== разное =======================================
// изменение данных по клику кнопки с контролем выхода за предельное значение
void checkData(byte &dt, byte max, bool toUp);

// вывести на экран температуру
void showTemp();