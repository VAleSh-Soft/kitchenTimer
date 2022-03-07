#include <Wire.h>
#include <DS3231.h> // https://github.com/NorthernWidget/DS3231
#include "dataList.h"
#include "display.h"
#include "timer.h"
#include "kitchenTimer.h"
#include <shButton.h>      // https://github.com/VAleSh-Soft/shButton
#include <shTaskManager.h> // https://github.com/VAleSh-Soft/shTaskManager

// ==== настройки ====================================
#define MIN_DISPLAY_BRIGHTNESS 1 // минимальная яркость дисплея, 1-7
#define MAX_DISPLAY_BRIGHTNESS 7 // максимальная яркость дисплея, 1-7
#define LIGHT_THRESHOLD 300      // порог переключения для датчика света
#define AUTO_EXIT_TIMEOUT 6      // время автоматического возврата в режим показа текущего времени из любых других режимов при отсутствии активности пользователя, секунд
// ===================================================

Display disp;
DS3231 clock; // SDA - A4, SCL - A5
RTClib RTC;
DataList data_list(DATA_LIST_INDEX, 10, MAX_DATA); // данные хранятся в EEPROM по адресам 100-118 (0x64-0x76), uint16_t, 10 записей, максимальное значение - 1439
Timer timer_1(IS_TIMER, LED_TIMER1_GREEN_PIN, LED_TIMER1_RED_PIN);
Timer timer_2(IS_TIMER, LED_TIMER2_GREEN_PIN, LED_TIMER2_RED_PIN);

#ifdef USE_LIGHT_SENSOR
shTaskManager tasks(9); // создаем список задач
#else
shTaskManager tasks(8); // создаем список задач
#endif

shHandle blink_timer;            // блинк
shHandle return_to_default_mode; // таймер автовозврата в режим показа времени из любого режима настройки
shHandle set_time_mode;          // режим настройки времени
shHandle show_temp_mode;         // режим показа температуры
shHandle leds_guard;             // управление индикаторными светодиодами
shHandle display_guard;          // вывод данных на экран
shHandle show_timer_mode;        // режим показа таймера ))
shHandle run_buzzer;             // пищалка
#ifdef USE_LIGHT_SENSOR
shHandle light_sensor_guard; // отслеживание показаний датчика света
#endif

byte displayMode = DISPLAY_MODE_SHOW_TIME;
bool blink_flag = false; // флаг блинка, используется всем, что должно мигать

// ==== класс кнопок с предварительной настройкой ====
#define BTN_FLAG_NONE 0 // флаг кнопки - ничего не делать
#define BTN_FLAG_NEXT 1 // флаг кнопки - изменить значение
#define BTN_FLAG_EXIT 2 // флаг кнопки - возврат в режим показа текущего времени

class ktButton : public shButton
{
private:
  byte _flag = BTN_FLAG_NONE;

public:
  ktButton(byte button_pin) : shButton(button_pin)
  {
    shButton::setTimeout(800);
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

ktButton btnSet(BTN_SET_PIN);     // кнопка Set - смена режима работы часов
ktButton btnUp(BTN_UP_PIN);       // кнопка Up - изменение часов/минут в режиме настройки
ktButton btnDown(BTN_DOWN_PIN);   // кнопка Down - изменение часов/минут в режиме настройки
ktButton btnTimer(BTN_TIMER_PIN); // кнопка Timer - работа с таймерами

// ===================================================
void clearStopFlag(Timer &tmr)
{
  if (tmr.getTimerFlag() == TIMER_FLAG_STOP)
  {
    tmr.setTimerFlag(TIMER_FLAG_NONE);
  }
}

void checkButton()
{
  checkSetButton();
  checkUpDownButton();
  checkTimerButton();
  // отключение сигнала, если есть сработка какого-то таймера
  if ((btnSet.getLastState() == BTN_DOWN) || (btnUp.getLastState() == BTN_DOWN) ||
      (btnDown.getLastState() == BTN_DOWN) || (btnTimer.getLastState() == BTN_DOWN))
  {
    clearStopFlag(timer_1);
    clearStopFlag(timer_2);
  }
}

void checkSetButton()
{
  switch (btnSet.getButtonState())
  {
  case BTN_ONECLICK:
    switch (displayMode)
    {
    case DISPLAY_MODE_SET_HOUR:
    case DISPLAY_MODE_SET_MINUTE:
    case DISPLAY_MODE_SHOW_TIMER_1:
    case DISPLAY_MODE_SHOW_TIMER_2:
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
    case DISPLAY_MODE_SHOW_TIMER_1:
    case DISPLAY_MODE_SHOW_TIMER_2:
      returnToDefMode();
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
    if (btnUp.getButtonState() == BTN_ONECLICK)
    {
      displayMode = DISPLAY_MODE_SHOW_TEMP;
    }
    break;
  case DISPLAY_MODE_SET_HOUR:
  case DISPLAY_MODE_SET_MINUTE:
  case DISPLAY_MODE_SHOW_TIMER_1:
  case DISPLAY_MODE_SHOW_TIMER_2:
    checkUDbtn(btnUp);
    checkUDbtn(btnDown);
    break;
  case DISPLAY_MODE_SHOW_TEMP:
    if (btnUp.getButtonState() == BTN_ONECLICK)
    {
      returnToDefMode();
    }
    break;
  }
}

void checkTimerButton()
{
  switch (btnTimer.getButtonState())
  {
  case BTN_ONECLICK:
    switch (displayMode)
    {
    case DISPLAY_MODE_SHOW_TIMER_1:
    case DISPLAY_MODE_SHOW_TIMER_2:
      btnTimer.setBtnFlag(BTN_FLAG_NEXT);
      break;
    }
    break;
  case BTN_DBLCLICK:
    switch (displayMode)
    {
    case DISPLAY_MODE_SHOW_TIMER_1:
    case DISPLAY_MODE_SHOW_TIMER_2:
      btnTimer.setBtnFlag(BTN_FLAG_EXIT);
      break;
    }
    break;
  case BTN_LONGCLICK:
    switch (displayMode)
    {
    case DISPLAY_MODE_SHOW_TIME:
      displayMode = DISPLAY_MODE_SHOW_TIMER_1;
      break;
    case DISPLAY_MODE_SHOW_TIMER_1:
      displayMode = DISPLAY_MODE_SHOW_TIMER_2;
      tasks.stopTask(show_timer_mode); // это чтобы переинициализировались значения таймера
      break;
    case DISPLAY_MODE_SHOW_TIMER_2:
      returnToDefMode();
      break;
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
  case DISPLAY_MODE_SHOW_TIMER_1:
  case DISPLAY_MODE_SHOW_TIMER_2:
    displayMode = DISPLAY_MODE_SHOW_TIME;
    showTime(RTC.now());
    tasks.stopTask(show_timer_mode);
    break;
  }
  tasks.stopTask(return_to_default_mode);
  tasks.setTaskInterval(return_to_default_mode, AUTO_EXIT_TIMEOUT * 1000ul, false);
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
      showTime(RTC.now());
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

  // вывод данных на экран ============
  showTimeData(curHour, curMinute);
}

void showTemp()
{
  if (!tasks.getTaskState(show_temp_mode))
  {
    tasks.startTask(return_to_default_mode);
    tasks.startTask(show_temp_mode);
  }

  disp.clear();
  disp.setDispData(3, 0x63);
  int temp = int(clock.getTemperature());
  // если температура выходит за диапазон, сформировать строку минусов
  if (temp > 99 || temp < -99)
  {
    for (byte i = 0; i < 4; i++)
    {
      disp.setDispData(i, 0x40);
    }
  }
  else
  { // если температура отрицательная, сформировать минус впереди
    if (temp < 0)
    {
      temp = -temp;
      disp.setDispData(1, 0x40);
    }
    if (temp > 9)
    { // если температура ниже -9, переместить минус на крайнюю левую позицию
      if (disp.getDispData(1) == 0x40)
      {
        disp.setDispData(0, 0x40);
      }
      disp.setDispData(1, disp.encodeDigit(temp / 10));
    }
    disp.setDispData(2, disp.encodeDigit(temp % 10));
  }
}

void setStateLed(Timer &tmr)
{
  byte mask = 0;

  switch (tmr.getTimerFlag())
  {
  case TIMER_FLAG_RUN:
  case TIMER_FLAG_STOP:
    if (blink_flag)
    {
      mask = (tmr.getTimerFlag() == TIMER_FLAG_RUN) ? 1 : 2; // 0b0001 : 0b0010
    }
    break;
  case TIMER_FLAG_PAUSED:
    mask = 1;
    break;
  }

  tmr.setLed(mask);
}

void setLeds()
{
  digitalWrite(LED_CLOCK_PIN, displayMode <= DISPLAY_MODE_SET_MINUTE);
  digitalWrite(LED_TIMER1_PIN, displayMode == DISPLAY_MODE_SHOW_TIMER_1);
  digitalWrite(LED_TIMER2_PIN, displayMode == DISPLAY_MODE_SHOW_TIMER_2);
  setStateLed(timer_1);
  setStateLed(timer_2);
}

void setDisp()
{
  disp.show();
}

void showTimerChar(byte _type)
{
  // IS_TIMER - dur, IS_ALARM - End
  disp.setDispData(0, (_type == IS_TIMER) ? 0b01011110 : 0b01111001);
  disp.setDispData(1, (_type == IS_TIMER) ? 0b00011100 : 0b01010100);
  disp.setDispData(2, (_type == IS_TIMER) ? 0b01010000 : 0b01011110);
  disp.setDispData(3, 0x00);
}

void showTimerMode()
{
  static byte n = 0;
  Timer *tmr;
  // определить действующий таймер
  switch (displayMode)
  {
  case DISPLAY_MODE_SHOW_TIMER_1:
    tmr = &timer_1;
    break;
  case DISPLAY_MODE_SHOW_TIMER_2:
    tmr = &timer_2;
    break;
  }

  if (!tasks.getTaskState(show_timer_mode))
  {
    n = 0;
    tasks.startTask(show_timer_mode);
    tasks.setTaskInterval(return_to_default_mode, AUTO_EXIT_TIMEOUT * 2000ul);
    restartBlink();
    // инициализировать начальное значение таймера, если оно нулевое, и таймер в состоянии покоя
    if (tmr->getTimerFlag() == TIMER_FLAG_NONE)
    {
      if (tmr->getTimerCount() == 0)
      {
        switch (tmr->getTimerType())
        {
        case IS_TIMER:
          tmr->setTimerCount(data_list.getFirst());
          break;
        case IS_ALARM:
          tmr->setTimerCount(data_list.getFirst() + getCurMinuteCount());
          break;
        }
      }
    }
  }

  if (n < 20)
  {
    showTimerChar(tmr->getTimerType());
    n++;
    return;
  }

  // опрос кнопок =====================
  if (btnSet.getBtnFlag() == BTN_FLAG_NEXT)
  {
    switch (tmr->getTimerType())
    {
    case IS_TIMER:
      tmr->setTimerCount(data_list.getNext());
      break;
    case IS_ALARM:
      tmr->setTimerCount(data_list.getNext() + getCurMinuteCount());
      break;
    }
    btnSet.setBtnFlag(BTN_FLAG_NONE);
  }

  if ((btnUp.getBtnFlag() == BTN_FLAG_NEXT) || (btnDown.getBtnFlag() == BTN_FLAG_NEXT))
  {
    uint16_t t_data = tmr->getTimerCount();
    checkTimerData(t_data, MAX_DATA, btnUp.getBtnFlag() == BTN_FLAG_NEXT);
    switch (tmr->getTimerType())
    {
    case IS_TIMER:
      if (t_data <= MAX_DATA)
      {
        tmr->setTimerCount(t_data);
      }
      if (t_data == 0)
      {
        tmr->setTimerFlag(TIMER_FLAG_NONE);
      }
      break;
    case IS_ALARM:
      // исключаем кольцевой перебор значений; при увеличении значений останавливаемся за минуту до текущего времени, при уменьшении останавливаемся на текущем времени
      uint16_t _data = (btnUp.getBtnFlag() == BTN_FLAG_NEXT) ? t_data : tmr->getTimerCount();
      if (_data != getCurMinuteCount())
      {
        tmr->setTimerCount(t_data);
      }
      if (t_data == getCurMinuteCount())
      {
        tmr->setTimerFlag(TIMER_FLAG_NONE);
      }
      break;
    }
    btnUp.setBtnFlag(BTN_FLAG_NONE);
    btnDown.setBtnFlag(BTN_FLAG_NONE);
  }
  // сброс таймера по нажатию двух кнопок - Up+Down
  if (btnUp.isButtonClosed() && btnDown.isButtonClosed())
  {
    tmr->stop(true);
    btnUp.resetButtonState();
    btnDown.resetButtonState();
    if (tmr->getTimerType == IS_ALARM)
    {
      tmr->setTimerCount(getCurMinuteCount());
    }
  }

  switch (btnTimer.getBtnFlag())
  {
  case BTN_FLAG_NEXT:
    if ((tmr->getTimerType() == IS_TIMER && tmr->getTimerCount() > 0) ||
        (tmr->getTimerType() == IS_ALARM && tmr->getTimerCount() != getCurMinuteCount()))
    {
      if (tmr->getTimerFlag() < TIMER_FLAG_STOP)
      {
        if (tmr->getTimerType() == IS_TIMER &&
            tmr->getTimerFlag() == TIMER_FLAG_NONE &&
            tmr->getTimerCount() > 0)
        { // сохранять данные нужно только для таймера, для будильника не нужно
          data_list.saveNewData(tmr->getTimerCount());
        }
        tmr->startPause();
      }
    }
    break;
  case BTN_FLAG_EXIT:
    if (tmr->getTimerFlag() == TIMER_FLAG_NONE)
    { // смена типа таймера по двойному клику таймер-кнопки
      tmr->setTimerType(!tmr->getTimerType());
      tmr->setTimerCount(0);
      uint16_t x = (displayMode == DISPLAY_MODE_SHOW_TIMER_1) ? TIMER_1_TYPE_INDEX : TIMER_2_TYPE_INDEX;
      eeprom_update_byte(x, tmr->getTimerType());
      tasks.stopTask(show_timer_mode);
    }
    break;
  }
  btnTimer.setBtnFlag(BTN_FLAG_NONE);

  // вывод данных на экран ============
  switch (tmr->getTimerType())
  {
  case IS_TIMER:
    if (tmr->getTimerCount() > 1 || tmr->getTimerFlag() == TIMER_FLAG_NONE ||
        btnUp.isButtonClosed() || btnDown.isButtonClosed())
    {
      showTimeData(tmr->getTimerCount() / 60, tmr->getTimerCount() % 60);
    }
    else
    {
      (tmr->getTimerFlag() == TIMER_FLAG_STOP) ? showTimeData(0, 0) : showTimeData(0, tmr->getTimerSecond());
    }
    break;
  case IS_ALARM:
    showTimeData(tmr->getTimerCount() / 60, tmr->getTimerCount() % 60);
    break;
  }
}

void runBuzzer()
{
  static byte n = 0;
  static uint16_t k = 0;
  // "мелодия" пищалки: первая строка - частота, вторая строка - длительность
  static const PROGMEM uint32_t pick[2][8] = {
      {2000, 0, 2000, 0, 2000, 0, 2000, 0},
      {50, 100, 50, 100, 50, 100, 50, 500}};
  if (!tasks.getTaskState(run_buzzer))
  {
    tasks.startTask(run_buzzer);
    n = 0;
    k = 0;
  }
  else if ((timer_1.getTimerFlag() != TIMER_FLAG_STOP) &&
           (timer_2.getTimerFlag() != TIMER_FLAG_STOP))
  { // остановка пищалки, если таймеры сброшены
    tasks.stopTask(run_buzzer);
    return;
  }
  tone(BUZZER_PIN, pgm_read_dword(&pick[0][n]), pgm_read_dword(&pick[1][n]));
  tasks.setTaskInterval(run_buzzer, pgm_read_dword(&pick[1][n]), true);
  if (++n >= 8)
  {
    n = 0;
    if (++k >= 300)
    { // остановка пищалки через пять минут
      tasks.stopTask(run_buzzer);
      k = 0;
    }
  }
}

void restartBuzzer()
{
  tasks.stopTask(run_buzzer);
  runBuzzer();
}

#ifdef USE_LIGHT_SENSOR
void setBrightness()
{
  static word b;
  b = (b * 2 + analogRead(LIGHT_SENSOR_PIN)) / 3;
  byte c = (b < LIGHT_THRESHOLD) ? MIN_DISPLAY_BRIGHTNESS : MAX_DISPLAY_BRIGHTNESS;
  disp.setBrightness(c);
}
#endif

// ===================================================
void setDisplayData(int8_t num_left, int8_t num_right, bool show_colon)
{
  disp.clear();
  if (num_left >= 0)
  {
    disp.setDispData(0, disp.encodeDigit(num_left / 10));
    disp.setDispData(1, disp.encodeDigit(num_left % 10));
  }
  if (num_right >= 0)
  {
    disp.setDispData(2, disp.encodeDigit(num_right / 10));
    disp.setDispData(3, disp.encodeDigit(num_right % 10));
  }
  if (show_colon)
  {
    byte x = disp.getDispData(1);
    x |= (0x80);
    disp.setDispData(1, x); // для показа двоеточия установить старший бит во второй цифре
  }
}

void showTime(DateTime dt)
{
  setDisplayData(dt.hour(), dt.minute(), blink_flag);
}

void showTimeData(byte hour, byte minute)
{
  // если наступило время блинка и кнопки Up/Down не нажаты, то стереть соответствующие разряды; при нажатых кнопках Up/Down во время изменения данных ничего не мигает
  if (!blink_flag && !btnUp.isButtonClosed() && !btnDown.isButtonClosed())
  {
    switch (displayMode)
    {
    case DISPLAY_MODE_SET_HOUR:
      hour = -1;
      break;
    case DISPLAY_MODE_SET_MINUTE:
      minute = -1;
      break;
    }
    // в таймерных режимах мигает сразу все, но только в состоянии покоя и при ненажатых кнопках Up/Down
    if ((displayMode == DISPLAY_MODE_SHOW_TIMER_1 && timer_1.getTimerFlag() == TIMER_FLAG_NONE) ||
        (displayMode == DISPLAY_MODE_SHOW_TIMER_2 && timer_2.getTimerFlag() == TIMER_FLAG_NONE))
    {
      hour = -1;
      minute = -1;
    }
  }
  // двоеточие отображается только в таймерных режимах
  setDisplayData(hour, minute, displayMode >= DISPLAY_MODE_SHOW_TIMER_1);
}

// ===================================================
void saveTime(byte hour, byte minute)
{
  clock.setSecond(0);
  clock.setHour(hour);
  clock.setMinute(minute);
}

// ===================================================
void _checkTimer(Timer &tmr)
{
  if (tmr.getTimerFlag() == TIMER_FLAG_RUN)
  {
    tmr.tick(RTC.now());
  }
}

void checkTimers()
{
  _checkTimer(timer_1);
  _checkTimer(timer_2);

  if ((timer_1.getCheckFlag()) || (timer_2.getCheckFlag()))
  {
    restartBuzzer();
  }
}

// ===================================================
void checkData(byte &dt, byte max, bool toUp)
{
  (toUp) ? dt++ : dt--;
  if (dt > max)
  {
    dt = (toUp) ? 0 : max;
  }
}

void checkTimerData(uint16_t &dt, uint16_t max, bool toUp)
{
  (toUp) ? dt++ : ((dt > 0) ? dt-- : 0);
  if (dt > max)
  {
    dt = (toUp) ? 0 : max;
  }
}

void setDisplay()
{
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
  case DISPLAY_MODE_SHOW_TIMER_1:
  case DISPLAY_MODE_SHOW_TIMER_2:
    if (!tasks.getTaskState(show_timer_mode))
    {
      showTimerMode();
    }
    break;
  }
}

uint16_t getCurMinuteCount()
{
  DateTime dt = RTC.now();
  return (dt.hour() * 60 + dt.minute());
}

// ===================================================
void setup()
{
  //  Serial.begin(9600);

  // ==== таймеры ======================================
  if (eeprom_read_byte(TIMER_1_TYPE_INDEX) > IS_ALARM)
  {
    eeprom_update_byte(TIMER_1_TYPE_INDEX, IS_TIMER);
  }
  if (eeprom_read_byte(TIMER_2_TYPE_INDEX) > IS_ALARM)
  {
    eeprom_update_byte(TIMER_2_TYPE_INDEX, IS_TIMER);
  }

  timer_1.setTimerType(eeprom_read_byte(TIMER_1_TYPE_INDEX));
  timer_2.setTimerType(eeprom_read_byte(TIMER_2_TYPE_INDEX));

  // ==== часы =========================================
  Wire.begin();
  clock.setClockMode(false); // 24-часовой режим

  // ==== кнопки Up/Down ===============================
  btnUp.setLongClickMode(LCM_CLICKSERIES);
  btnUp.setLongClickTimeout(50);
  btnDown.setLongClickMode(LCM_CLICKSERIES);
  btnDown.setLongClickTimeout(50);

  // ==== пины =========================================
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_CLOCK_PIN, OUTPUT);
  pinMode(LED_TIMER1_PIN, OUTPUT);
  pinMode(LED_TIMER2_PIN, OUTPUT);
  pinMode(LED_TIMER1_RED_PIN, OUTPUT);
  pinMode(LED_TIMER1_GREEN_PIN, OUTPUT);
  pinMode(LED_TIMER2_RED_PIN, OUTPUT);
  pinMode(LED_TIMER2_GREEN_PIN, OUTPUT);

  // ==== задачи =======================================
  blink_timer = tasks.addTask(500, blink);
  return_to_default_mode = tasks.addTask(AUTO_EXIT_TIMEOUT * 1000, returnToDefMode, false);
  set_time_mode = tasks.addTask(100, showTimeSetting, false);
  show_temp_mode = tasks.addTask(500, showTemp, false);
  leds_guard = tasks.addTask(100, setLeds);
  display_guard = tasks.addTask(50, setDisp);
  show_timer_mode = tasks.addTask(50, showTimerMode, false);
  run_buzzer = tasks.addTask(100, runBuzzer, false);
#ifdef USE_LIGHT_SENSOR
  light_sensor_guard = tasks.addTask(100, setBrightness);
#else
  disp.setBrightness(MAX_DISPLAY_BRIGHTNESS);
#endif
}

void loop()
{
  checkButton();
  tasks.tick();
  setDisplay();
  checkTimers();
}
