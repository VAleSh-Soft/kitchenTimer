#pragma once
#include <Arduino.h>
#include <DS3231.h>

// #define USE_LIGHT_SENSOR // использовать или нет датчик света на пине А6 для регулировки яркости дисплея

#define DISPLAY_MODE_SHOW_TIME 0     // основной режим - вывод времени на индикатор
#define DISPLAY_MODE_SET_HOUR 1      // режим настройки часов
#define DISPLAY_MODE_SET_MINUTE 2    // режим настройки минут
#define DISPLAY_MODE_SHOW_TEMP 3     // режим вывода температуры
#define DISPLAY_MODE_SHOW_TIMER_1 10 // режим первого таймера
#define DISPLAY_MODE_SHOW_TIMER_2 11 // режим второго таймера

#define BTN_SET_PIN 5
#define BTN_DOWN_PIN 4
#define BTN_UP_PIN 3
#define BTN_TIMER_PIN 2
#define BUZZER_PIN 6
#define LED_CLOCK_PIN 7
#define LED_TIMER1_PIN 8
#define LED_TIMER2_PIN 9
#define LED_TIMER1_GREEN_PIN A0
#define LED_TIMER1_RED_PIN A1
#define LED_TIMER2_GREEN_PIN A2
#define LED_TIMER2_RED_PIN A3
#ifdef USE_LIGHT_SENSOR
#define LIGHT_SENSOR_PIN A6
#endif

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
#ifdef USE_LIGHT_SENSOR
void setBrightness();
#endif

// ==== вывод данных на дисплей ======================
// вывод времени на индикатор
void showTime(DateTime dt);
// вывод на экран данных в режиме настройки времени и в таймерных режимах
void showTimeData(byte hour, byte minute);
// отрисовка данных на дисплее
void setDisplayData(int8_t num_left, int8_t num_right, bool show_colon);

// ==== часы =========================================
// сохранение времени после настройки
void saveTime(byte hour, byte minute);

// ==== разное =======================================
// изменение данных по клику кнопки с контролем выхода за предельное значение
void checkData(byte &dt, byte max, bool toUp);
void checkData(uint16_t &dt, uint16_t max, bool toUp);

// вывод данных на экран
void setDisplay();