#include <TM1637Display.h>
#include <Wire.h>   // Подключаем библиотеку для работы с I2C устройствами
#include <DS3231.h> // Подключаем библиотеку для работы с RTC DS3231
#include "kitchenTimer.h"
#include <shButton.h>
#include <shTaskManager.h>

TM1637Display tm(11, 10); // CLK, DAT
DS3231 clock;             // SDA - A4, SCL - A5
RTClib RTC;

#ifdef USE_LIGHT_SENSOR
shTaskManager tasks(3); // создаем список задач
#else
shTaskManager tasks(2); // создаем список задач
#endif

shHandle blink_timer;            // таймер блинка
shHandle return_to_default_mode; // таймер автовозврата в режим показа времени из любого режима настройки
#ifdef USE_LIGHT_SENSOR
shHandle light_sensor_guard; // отслеживание показаний датчика света
#endif

byte displayMode = DISPLAY_MODE_SHOW_TIME;
bool blink_flag = false;

// ==== класс кнопок с предварительной настройкой ====
class ktButton : public shButton
{
public:
  ktButton(byte button_pin) : shButton(button_pin)
  {
    shButton::setTimeout(1000);
    shButton::setLongClickMode(LCM_ONLYONCE);
    shButton::setVirtualClickOn(true);
    shButton::setDebounce(50);
  }

  byte getButtonState()
  {
    byte _state = shButton::getButtonState();
    switch (_state)
    {
    case BTN_DOWN:
    case BTN_DBLCLICK:
    case BTN_LONGCLICK:
      // в любом режиме, кроме стандартного, каждый клик любой кнопки перезапускает таймер автовыхода в стандартный режим
      if (tasks.getTaskState(return_to_default_mode))
      {
        tasks.restartTask(return_to_default_mode);
      }
      break;
    }
    return (_state);
  }
};
// ===================================================

ktButton btnSet(BTN_SET_PIN);   // кнопка Set - смена режима работы часов
ktButton btnUp(BTN_UP_PIN);     // кнопка Up - изменение часов/минут в режиме настройки
ktButton btnDown(BTN_DOWN_PIN); // кнопка Down - изменение часов/минут в режиме настройки

// ===================================================
void checkButton()
{
  checkSetButton();
  checkUpButton();
  checkDownButton();
}

void checkSetButton()
{
}

void checkUpButton()
{
}

void checkDownButton()
{
}

// ===================================================
void blink()
{
  if (!tasks.getTaskState(blink_timer))
  {
    tasks.startTask(blink_timer);
    blink_flag = false;
  }
  else
  {
    blink_flag = !blink_flag;
  }
}

void restartBlink()
{
  tasks.stopTask(blink_timer);
  blink();
}

void returnToDefMode()
{
  displayMode = DISPLAY_MODE_SHOW_TIME;
  tasks.stopTask(return_to_default_mode);
}

#ifdef USE_LIGHT_SENSOR
void setBrightness()
{
  static word b;
  b = (b * 2 + analogRead(LIGHT_SENSOR_PIN)) / 3;
  byte c = (b < 300) ? 1 : 7;
  tm.setBrightness(c);
}
#endif

// ===================================================
void showTime(DateTime dt, bool force)
{
  showTime(dt.hour(), dt.minute(), force);
}

void showTime(byte hour, byte minute, bool force = false)
{
  static bool p = blink_flag;
  // вывод делается только в момент смены состояния блинка, т.е. через каждые 500 милисекунд или по флагу принудительного обновления
  if (force || p != blink_flag)
  {
    word h = hour * 100 + minute;
    if (!force)
    {
      p = !p;
    }
    uint8_t s = (p) ? 0b01000000 : 0; // это двоеточие
    tm.showNumberDecEx(h, s, true);
  }
}

// ===================================================
void setup()
{
  // Serial.begin(9600);

  Wire.begin();
  clock.setClockMode(false); // 24-часовой режим

  btnUp.setLongClickMode(LCM_CLICKSERIES);
  btnUp.setLongClickTimeout(100);
  btnDown.setLongClickMode(LCM_CLICKSERIES);
  btnDown.setLongClickTimeout(100);

  blink_timer = tasks.addTask(500, blink);
  return_to_default_mode = tasks.addTask(10000, returnToDefMode, false);
#ifdef USE_LIGHT_SENSOR
  light_sensor_guard = tasks.addTask(100, setBrightness);
#else
  tm.setBrightness(7);
#endif
}

void loop()
{
  checkButton();
  tasks.tick();
  switch (displayMode)
  {
  case DISPLAY_MODE_SHOW_TIME:
    showTime(RTC.now());
    break;
  default:
    break;
  }
}
