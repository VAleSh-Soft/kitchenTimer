#pragma once
#include <Arduino.h>
#include "shSimpleRTC.h"

// #define USE_LIGHT_SENSOR // использовать или нет датчик света на пине А6 для регулировки яркости экрана

#define USE_TEMP_DATA // использовать или нет вывод на экран температуры по клику кнопкой Up

#define USE_MODE_BUTTON // использование кнопки переключения режима отображения экрана

// ==== пины =========================================

// ==== кнопки =======================================
#define BTN_SET_PIN 5   // пин для подключения кнопки Set
#define BTN_DOWN_PIN 4  // пин для подключения кнопки Down
#define BTN_UP_PIN 3    // пин для подключения кнопки Up
#define BTN_TIMER_PIN 2 // пин для подключения кнопки Timer
#ifdef USE_MODE_BUTTON
#define BTN_MODE_PIN 12 // пин для подключения кнопки Mode
#endif

// ==== DS3231 =======================================
#define DS3231_SDA_PIN A4 // пин для подключения вывода SDA модуля DS3231 (не менять!!!)
#define DS3231_SCL_PIN A5 // пин для подключения вывода SCL модуля DS3231 (не менять!!!)

// ==== экран ========================================
#define DISPLAY_CLK_PIN 11      // пин для подключения экрана - CLK
#define DISPLAY_DAT_PIN 10      // пин для подключения экрана - DAT
#define BUZZER_PIN 6            // пин для подключения пищалки

// ==== светодиоды ===================================
#define LED_CLOCK_PIN 7         // пин для подключения светодиода-индикатора - часы
#define LED_TIMER1_PIN 8        // пин для подключения светодиода-индикатора - таймер 1
#define LED_TIMER2_PIN 9        // пин для подключения светодиода-индикатора - таймер 2
#define LED_TIMER1_GREEN_PIN A0 // пин для подключения зеленого светодиода таймера 1
#define LED_TIMER1_RED_PIN A1   // пин для подключения красного светодиода таймера 1
#define LED_TIMER2_GREEN_PIN A2 // пин для подключения зеленого светодиода таймера 2
#define LED_TIMER2_RED_PIN A3   // пин для подключения красного светодиода таймера 2

// ==== датчики ======================================
#ifdef USE_LIGHT_SENSOR
#define LIGHT_SENSOR_PIN A6 // пин для подключения датчика света
#endif

// ==== индексы EEPROM для хранения данных ===========

#define TIMER_1_EEPROM_INDEX 80 // индекс в EEPROM для сохранения данных первого таймера
#define TIMER_2_EEPROM_INDEX 90 // индекс в EEPROM для сохранения данных второго таймера
#define DATA_LIST_INDEX 100     // первый индекс в EEPROM для хранения списка сохраненных значений таймера

// ===================================================

enum DisplayMode : uint8_t
{
  DISPLAY_MODE_SHOW_TIME,    // основной режим - вывод времени на экран
  DISPLAY_MODE_SET_HOUR,     // режим настройки часов
  DISPLAY_MODE_SET_MINUTE,   // режим настройки минут
#ifdef USE_TEMP_DATA
  DISPLAY_MODE_SHOW_TEMP,    // режим вывода температуры
#endif
  DISPLAY_MODE_SHOW_TIMER_1, // режим первого таймера
  DISPLAY_MODE_SHOW_TIMER_2  // режим второго таймера
};

// ==== опрос кнопок =================================
void checkButton();
void checkSetButton();
void checkUpDownButton();
void checkTimerButton();
#ifdef USE_MODE_BUTTON
void checkModeButton();
#endif

// ==== задачи =======================================
void blink();
void restartBlink();
void returnToDefMode();
void showTimeSetting();
void rtcNow();
#ifdef USE_TEMP_DATA
void showTemp();
#endif
void setLeds();
void showTimerMode();
void runBuzzer();
void restartBuzzer();
void setDisp();
void checkTimers();
void backupEndPoints();
#ifdef USE_LIGHT_SENSOR
void setBrightness();
#endif

// ==== вывод данных на экран ========================
// вывод на экран данных в режиме настройки времени и в таймерных режимах
void showTimeData(uint8_t hour, uint8_t minute);

// показ обозначения типа таймера при входе в таймерный режим
void showTimerChar(uint8_t _type);

// ==== часы =========================================
// сохранение времени после настройки
void saveTime(uint8_t hour, uint8_t minute);

// ==== разное =======================================
// изменение данных по клику кнопки с контролем выхода за предельное значение
void checkData(uint8_t &dt, uint8_t max, bool toUp);

// вывод данных на экран
void setDisplay();

// сброс сработавшего таймера
bool clearStopFlag(Timer &tmr);
