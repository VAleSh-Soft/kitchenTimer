#include <TM1637Display.h>
#include <Wire.h>   // Подключаем библиотеку для работы с I2C устройствами
#include <DS3231.h> // Подключаем библиотеку для работы с RTC DS3231
#include "kitchenTimer.h"
#include <shButton.h>
#include <shTaskManager.h>

// ==== настройки ====================================
#define MIN_DISPLAY_BRIGHTNESS 1 // минимальная яркость дисплея, 1-7
#define MAX_DISPLAY_BRIGHTNESS 7 // максимальная яркость дисплея, 1-7
#define AUTO_EXIT_TIMEOUT 6      // время автоматического возврата в режим показа текущего времени из любых других режимов при отсутствии активности пользователя, секунд
// ===================================================

TM1637Display tm(11, 10); // CLK, DAT
DS3231 clock;             // SDA - A4, SCL - A5
RTClib RTC;

#ifdef USE_LIGHT_SENSOR
shTaskManager tasks(5); // создаем список задач
#else
shTaskManager tasks(4); // создаем список задач
#endif

shHandle blink_timer;            // таймер блинка
shHandle return_to_default_mode; // таймер автовозврата в режим показа времени из любого режима настройки
shHandle set_time_mode;          // таймер настройки времени
shHandle show_temp_mode;         // таймер показа температуры
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
  checkUpDownButton();
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

void checkUDbtn(ktButton &btn)
{
  switch (btn.getButtonState())
  {
  case BTN_DOWN:
  case BTN_DBLCLICK:
  case BTN_LONGCLICK:
    btn.setBtnFlag(BTN_FLAG_NEXT);
    break;
  }
}

void checkUpDownButton()
{
  switch (displayMode)
  {
  case DISPLAY_MODE_SHOW_TIME:
    if (btnDown.getButtonState() == BTN_ONECLICK)
    {
      displayMode = DISPLAY_MODE_SHOW_TEMP;
    }
    break;
  case DISPLAY_MODE_SET_HOUR:
  case DISPLAY_MODE_SET_MINUTE:
    checkUDbtn(btnUp);
    checkUDbtn(btnDown);
    break;
  case DISPLAY_MODE_SHOW_TEMP:
    if (btnDown.getButtonState() == BTN_ONECLICK)
    {
      displayMode = DISPLAY_MODE_SHOW_TIME;
      tasks.stopTask(return_to_default_mode);
      tasks.stopTask(show_temp_mode);
    }
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
  case DISPLAY_MODE_SHOW_TEMP:
    displayMode = DISPLAY_MODE_SHOW_TIME;
    tasks.stopTask(show_temp_mode);
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
  if (btnSet.getBtnFlag() > BTN_FLAG_NONE)
  {
    if (time_checked)
    {
      saveTime(curHour, curMinute);
      time_checked = false;
    }
    if (btnSet.getBtnFlag() == BTN_FLAG_NEXT)
    {
      checkData(displayMode, DISPLAY_MODE_SET_MINUTE, true);
    }
    else
    {
      displayMode = DISPLAY_MODE_SHOW_TIME;
    }
    btnSet.setBtnFlag(BTN_FLAG_NONE);
    if (displayMode == DISPLAY_MODE_SHOW_TIME)
    {
      tasks.stopTask(set_time_mode);
      tasks.stopTask(return_to_default_mode);
      return;
    }
  }

  if ((btnUp.getBtnFlag() == BTN_FLAG_NEXT) || (btnDown.getBtnFlag() == BTN_FLAG_NEXT))
  {
    bool dir = btnUp.getBtnFlag() == BTN_FLAG_NEXT;
    switch (displayMode)
    {
    case DISPLAY_MODE_SET_HOUR:
      checkData(curHour, 23, dir);
      break;
    case DISPLAY_MODE_SET_MINUTE:
      checkData(curMinute, 59, dir);
      break;
    }
    time_checked = true;
    btnUp.setBtnFlag(BTN_FLAG_NONE);
    btnDown.setBtnFlag(BTN_FLAG_NONE);
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

void showTime(byte hour, byte minute, bool force)
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

void showTemp()
{
  if (!tasks.getTaskState(show_temp_mode))
  {
    tasks.startTask(return_to_default_mode);
    tasks.startTask(show_temp_mode);
  }

  uint8_t data[] = {0x00, 0x00, 0x00, 0x63};
  int temp = int(clock.getTemperature());
  // если температура выходит за диапазон, сформировать строку минусов
  if (temp > 99 || temp < -99)
  {
    for (byte i = 0; i < 4; i++)
    {
      data[i] = 0x40;
    }
  }
  else
  { // если температура отрицательная, сформировать минус впереди
    if (temp < 0)
    {
      temp = -temp;
      data[1] = 0x40;
    }
    if (temp > 9)
    { // если температура ниже -9, переместить минус на крайнюю левую позицию
      if (data[1] == 0x40)
      {
        data[0] = 0x40;
      }
      data[1] = tm.encodeDigit(temp / 10);
    }
    data[2] = tm.encodeDigit(temp % 10);
  }
  // вывести данные на индикатор
  tm.setSegments(data);
}

// ===================================================
void setup()
{
  //  Serial.begin(9600);

  // ==== часы =========================================
  Wire.begin();
  clock.setClockMode(false); // 24-часовой режим

  // ==== кнопки Up/Down ===============================
  btnUp.setLongClickMode(LCM_CLICKSERIES);
  btnUp.setLongClickTimeout(100);
  btnDown.setLongClickMode(LCM_CLICKSERIES);
  btnDown.setLongClickTimeout(100);

  // ==== задачи =======================================
  blink_timer = tasks.addTask(500, blink);
  return_to_default_mode = tasks.addTask(AUTO_EXIT_TIMEOUT * 1000, returnToDefMode, false);
  set_time_mode = tasks.addTask(100, showTimeSetting, false);
  show_temp_mode = tasks.addTask(500, showTemp, false);
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
  case DISPLAY_MODE_SHOW_TEMP:
    if (!tasks.getTaskState(show_temp_mode))
    {
      showTemp();
    }
    break;
  default:
    break;
  }
}
