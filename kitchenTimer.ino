#include <TM1637Display.h>
#include <Wire.h>   // Подключаем библиотеку для работы с I2C устройствами
#include <DS3231.h> // Подключаем библиотеку для работы с RTC DS3231
#include "kitchenTimer.h"
#include <shButton.h>
#include <shTaskManager.h>

// ==== настройки ====================================
#define MIN_DISPLAY_BRIGHTNESS 1 // минимальная яркость дисплея, 1-7
#define MAX_DISPLAY_BRIGHTNESS 7 // максимальная яркость дисплея, 1-7
#define AUTO_EXIT_TIMEOUT 10     // время автоматического возврата в режим показа текущего времени из любых других режимов при отсутствии активности пользователя, секунд
// ===================================================

TM1637Display tm(11, 10); // CLK, DAT
DS3231 clock;             // SDA - A4, SCL - A5
RTClib RTC;

#ifdef USE_LIGHT_SENSOR
shTaskManager tasks(4); // создаем список задач
#else
shTaskManager tasks(3); // создаем список задач
#endif

shHandle blink_timer;            // таймер блинка
shHandle return_to_default_mode; // таймер автовозврата в режим показа времени из любого режима настройки
shHandle set_time_mode;          // таймер настройки времени
#ifdef USE_LIGHT_SENSOR
shHandle light_sensor_guard; // отслеживание показаний датчика света
#endif

byte displayMode = DISPLAY_MODE_SHOW_TIME;
bool blink_flag = false;

// ==== класс кнопок с предварительной настройкой ====
class ktButton : public shButton
{
private:
  byte _flag = BTN_FLAG_NONE;

public:
  ktButton(byte button_pin) : shButton(button_pin)
  {
    shButton::setTimeout(1000);
    shButton::setLongClickMode(LCM_ONLYONCE);
    shButton::setVirtualClickOn(true);
    shButton::setDebounce(50);
  }

  byte getBtnFlag()
  {
    return (_flag);
  }

  void setBtnFlag(byte flag)
  {
    _flag = flag;
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
  switch (btnSet.getButtonState())
  {
  case BTN_DOWN:
  case BTN_DBLCLICK:

    break;
  case BTN_ONECLICK:
    switch (displayMode)
    {
    case DISPLAY_MODE_SET_HOUR:
    case DISPLAY_MODE_SET_MINUTE:
      btnSet.setBtnFlag(BTN_FLAG_NEXT);
      break;
    }
    break;
  case BTN_LONGCLICK:
    switch (displayMode)
    {
    case DISPLAY_MODE_SHOW_TIME:
      displayMode = DISPLAY_MODE_SET_HOUR;
      break;
    case DISPLAY_MODE_SET_HOUR:
    case DISPLAY_MODE_SET_MINUTE:
      btnSet.setBtnFlag(BTN_FLAG_EXIT);
      break;
    }
    break;
  }
}

void checkUpButton()
{
  switch (btnUp.getButtonState())
  {
  case BTN_ONECLICK:

    break;
  case BTN_LONGCLICK:

    break;
  }
}

void checkDownButton()
{
  switch (btnDown.getButtonState())
  {
  case BTN_ONECLICK:

    break;
  case BTN_LONGCLICK:

    break;
  }
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
  switch (displayMode)
  {
  case DISPLAY_MODE_SET_HOUR:
  case DISPLAY_MODE_SET_MINUTE:
    btnSet.setBtnFlag(BTN_FLAG_EXIT);
    break;
  }
  tasks.stopTask(return_to_default_mode);
}

void showTimeSetting()
{
  static bool time_checked = false;
  static byte curHour = 0;
  static byte curMinute = 0;

  if (!tasks.getTaskState(set_time_mode))
  {
    tasks.startTask(set_time_mode);
    tasks.startTask(return_to_default_mode);
    restartBlink();
    time_checked = false;
  }

  if (!time_checked)
  {
    DateTime dt = RTC.now();
    curHour = dt.hour();
    curMinute = dt.minute();
  }
  // опрос кнопок =====================
  switch (btnSet.getBtnFlag())
  {
  case BTN_FLAG_NEXT:
    checkData(displayMode, DISPLAY_MODE_SET_MINUTE, true);
    btnSet.setBtnFlag(BTN_FLAG_NONE);
    if (displayMode == DISPLAY_MODE_SHOW_TIME)
    {
      if (time_checked)
      {
        saveTime(curHour, curMinute);
        time_checked = false;
      }
      tasks.stopTask(set_time_mode);
      return;
    }
    break;
  case BTN_FLAG_EXIT:
    btnSet.setBtnFlag(BTN_FLAG_NONE);
    displayMode = DISPLAY_MODE_SHOW_TIME;
    if (time_checked)
    {
      saveTime(curHour, curMinute);
      time_checked = false;
    }
    tasks.stopTask(set_time_mode);
    return;
  }
  // вывод данных на индикатор ======
  showTimeSettingData(curHour, curMinute);
}

#ifdef USE_LIGHT_SENSOR
void setBrightness()
{
  static word b;
  b = (b * 2 + analogRead(LIGHT_SENSOR_PIN)) / 3;
  byte c = (b < 300) ? MIN_DISPLAY_BRIGHTNESS : MAX_DISPLAY_BRIGHTNESS;
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

void showTimeSettingData(byte hour, byte minute)
{
  uint8_t data[] = {0xff, 0xff, 0xff, 0xff};
  data[0] = tm.encodeDigit(hour / 10);
  data[1] = tm.encodeDigit(hour % 10);
  data[2] = tm.encodeDigit(minute / 10);
  data[3] = tm.encodeDigit(minute % 10);
  // если наступило время блинка и кнопки Up/Down не нажата, то стереть соответствующие разряды; при нажатых кнопках Up/Down во время изменения данных ничего не мигает
  if (!blink_flag && !btnUp.isButtonClosed() && !btnDown.isButtonClosed())
  {
    byte i = (displayMode == DISPLAY_MODE_SET_HOUR) ? 0 : 2;
    data[i] = 0x00;
    data[i + 1] = 0x00;
  }
  tm.setSegments(data);
}

void saveTime(byte hour, byte minute)
{
  clock.setHour(hour);
  clock.setMinute(minute);
  clock.setSecond(0);
}

// ===================================================
// изменение данных по клику кнопки с контролем выхода за предельное значение
void checkData(byte &dt, byte max, bool toUp)
{
  (toUp) ? dt++ : dt--;
  if (dt > max)
  {
    dt = (toUp) ? 0 : max;
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
  return_to_default_mode = tasks.addTask(AUTO_EXIT_TIMEOUT * 1000, returnToDefMode, false);
  set_time_mode = tasks.addTask(100, showTimeSetting, false);
#ifdef USE_LIGHT_SENSOR
  light_sensor_guard = tasks.addTask(100, setBrightness);
#else
  tm.setBrightness(MAX_DISPLAY_BRIGHTNESS);
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
  case DISPLAY_MODE_SET_HOUR:
  case DISPLAY_MODE_SET_MINUTE:
    if (!tasks.getTaskState(set_time_mode))
    {
      showTimeSetting();
    }
    break;
  default:
    break;
  }
}
