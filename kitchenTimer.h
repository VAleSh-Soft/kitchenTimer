#pragma once
#include <Arduino.h>
#include <DS3231.h> // https://github.com/NorthernWidget/DS3231

// #define USE_LIGHT_SENSOR // использовать или нет датчик света на пине А6 для регулировки яркости экрана

#define DISPLAY_MODE_SHOW_TIME 0     // основной режим - вывод времени на экран
#define DISPLAY_MODE_SET_HOUR 1      // режим настройки часов
#define DISPLAY_MODE_SET_MINUTE 2    // режим настройки минут
#define DISPLAY_MODE_SHOW_TEMP 3     // режим вывода температуры
#define DISPLAY_MODE_SHOW_TIMER_1 10 // режим первого таймера
#define DISPLAY_MODE_SHOW_TIMER_2 11 // режим второго таймера

#define BTN_SET_PIN 5           // пин для подключения кнопки Set
#define BTN_DOWN_PIN 4          // пин для подключения кнопки Down
#define BTN_UP_PIN 3            // пин для подключения кнопки Up
#define BTN_TIMER_PIN 2         // пин для подключения кнопки Timer
#define DISPLAY_CLK_PIN 11      // пин для подключения экрана - CLK
#define DISPLAY_DAT_PIN 10      // пин для подключения экрана - DAT
#define BUZZER_PIN 6            // пин для подключения пищалки
#define LED_CLOCK_PIN 7         // пин для подключения светодиода-индикатора - часы
#define LED_TIMER1_PIN 8        // пин для подключения светодиода-индикатора - таймер 1
#define LED_TIMER2_PIN 9        // пин для подключения светодиода-индикатора - таймер
#define LED_TIMER1_GREEN_PIN A0 // пин для подключения зеленого светодиода таймера 1
#define LED_TIMER1_RED_PIN A1   // пин для подключения красного светодиода таймера 1
#define LED_TIMER2_GREEN_PIN A2 // пин для подключения зеленого светодиода таймера 2
#define LED_TIMER2_RED_PIN A3   // пин для подключения красного светодиода таймера 2
#ifdef USE_LIGHT_SENSOR
#define LIGHT_SENSOR_PIN A6 // пин для подключения датчика света
#endif

#define TIMER_1_TYPE_INDEX 98 // индекс в EEPROM для сохранения типа первого таймера
#define TIMER_2_TYPE_INDEX 99 // индекс в EEPROM для сохранения типа второго таймера
#define DATA_LIST_INDEX 100   // первый индекс в EEPROM для хранения списка сохраненных значений таймера

// ==== опрос кнопок =================================
void checkButton();
void checkSetButton();
void checkUpDownButton();
void checkTimerButton();

// ==== задачи =======================================
void blink();
void restartBlink();
void returnToDefMode();
void showTimeSetting();
void showTemp();
void setLeds();
void showTimerMode();
void runBuzzer();
void restartBuzzer();
void setDisp();
#ifdef USE_LIGHT_SENSOR
void setBrightness();
#endif

// ==== вывод данных на экран ========================
// вывод на экран времени
void showTime(DateTime dt);
// вывод на экран данных в режиме настройки времени и в таймерных режимах
void showTimeData(byte hour, byte minute);

// ==== часы =========================================
// сохранение времени после настройки
void saveTime(byte hour, byte minute);

// ==== таймеры ======================================
// проверка состояния таймеров
void checkTimers();
// показ обозначения типа таймера при входе в таймерный режим
void showTimerChar(byte _type);

// ==== разное =======================================
// изменение данных по клику кнопки с контролем выхода за предельное значение
void checkData(byte &dt, byte max, bool toUp);
// то же самое, но для таймеров и без выхода за нуль
void checkTimerData(uint16_t &dt, uint16_t max, bool toUp);
// вывод данных на экран
void setDisplay();
// получение текущего количества минут с полуночи
uint16_t getCurMinuteCount();
