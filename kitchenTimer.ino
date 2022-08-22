#include <Wire.h>
#include <DS3231.h> // https://github.com/NorthernWidget/DS3231
#include "dataList.h"
#include "display_TM1637.h"
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

DisplayTM1637 disp(DISPLAY_CLK_PIN, DISPLAY_DAT_PIN);
DS3231 clock; // SDA - A4, SCL - A5
RTClib RTC;
DataList data_list(DATA_LIST_INDEX, 10, MAX_DATA); // данные хранятся в EEPROM по адресам 100-118 (0x64-0x76), uint16_t, 10 записей, максимальное значение - 1439
Timer timer_1(TIMER_1_EEPROM_INDEX, LED_TIMER1_GREEN_PIN, LED_TIMER1_RED_PIN);
Timer timer_2(TIMER_2_EEPROM_INDEX, LED_TIMER2_GREEN_PIN, LED_TIMER2_RED_PIN);

shTaskManager tasks; // создаем список задач, количество задач укажем в setup()

shHandle blink_timer;            // блинк
shHandle return_to_default_mode; // таймер автовозврата в режим показа времени из любого режима настройки
shHandle set_time_mode;          // режим настройки времени
shHandle show_temp_mode;         // режим показа температуры
shHandle leds_guard;             // управление индикаторными светодиодами
shHandle display_guard;          // вывод данных на экран
shHandle show_timer_mode;        // режим показа таймера
shHandle run_buzzer;             // пищалка
shHandle check_timers;           // проверка таймеров
shHandle backup_end_point;       // сохранение точки останова таймеров, если они изменены в процессе работы
#ifdef USE_LIGHT_SENSOR
shHandle light_sensor_guard; // отслеживание показаний датчика света
#endif

DisplayMode displayMode = DISPLAY_MODE_SHOW_TIME;
bool blink_flag = false; // флаг блинка, используется всем, что должно мигать

// ==== класс кнопок с предварительной настройкой ====
enum ButtonFlag : uint8_t
{
  BTN_FLAG_NONE, // флаг кнопки - ничего не делать
  BTN_FLAG_NEXT, // флаг кнопки - изменить значение
  BTN_FLAG_EXIT  // флаг кнопки - возврат в режим показа текущего времени
};

class ktButton : public shButton
{
private:
  ButtonFlag _flag = BTN_FLAG_NONE;

public:
  ktButton(byte button_pin) : shButton(button_pin)
  {
    shButton::setTimeoutOfLongClick(800);
    shButton::setLongClickMode(LCM_ONLYONCE);
    shButton::setVirtualClickOn(true);
    shButton::setTimeoutOfDebounce(50);
  }

  ButtonFlag getBtnFlag()
  {
    return (_flag);
  }

  void setBtnFlag(ButtonFlag flag)
  {
    _flag = flag;
  }

  void resetButtonState()
  {
    shButton::resetButtonState();
    _flag = BTN_FLAG_NONE;
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
      // сброс сработавших таймеров
      if ((_state == BTN_DOWN) && (clearStopFlag(timer_1) + clearStopFlag(timer_2)))
      {
        // если был сброшен таймер, то действие по кнопке отменяется
        resetButtonState();
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
#ifdef USE_MODE_BUTTON
ktButton btnMode(BTN_MODE_PIN); // кнопка Mode - переключение режима отображения индикатора
#endif

// ===================================================
bool clearStopFlag(Timer &tmr)
{
  bool result = false;
  if (tmr.getTimerFlag() == TIMER_FLAG_STOP)
  {
    tmr.setTimerFlag(TIMER_FLAG_NONE);
    result = true;
  }
  return (result);
}

void checkButton()
{
  checkSetButton();
  checkUpDownButton();
  checkTimerButton();
#ifdef USE_MODE_BUTTON
  checkModeButton();
#endif
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
    default:
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
    default:
      break;
    }
    break;
  }
}

void checkUDbtn(ktButton &btn)
{
  switch (btn.getLastState())
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
  btnUp.getButtonState();
  btnDown.getButtonState();
  switch (displayMode)
  {
  case DISPLAY_MODE_SHOW_TIME:
#ifdef USE_TEMP_DATA
    if (btnUp.getLastState() == BTN_ONECLICK)
    {
      displayMode = DISPLAY_MODE_SHOW_TEMP;
    }
#endif
    break;
  case DISPLAY_MODE_SET_HOUR:
  case DISPLAY_MODE_SET_MINUTE:
  case DISPLAY_MODE_SHOW_TIMER_1:
  case DISPLAY_MODE_SHOW_TIMER_2:
    checkUDbtn(btnUp);
    checkUDbtn(btnDown);
    break;
#ifdef USE_TEMP_DATA
  case DISPLAY_MODE_SHOW_TEMP:
    if (btnUp.getLastState() == BTN_ONECLICK)
    {
      returnToDefMode();
    }
    break;
#endif
  }
}

void setDisplayMode()
{
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
  default:
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
    default:
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
    default:
      break;
    }
    break;
  case BTN_LONGCLICK:
    setDisplayMode();
    break;
  }
}

#ifdef USE_MODE_BUTTON
void checkModeButton()
{
  if (btnMode.getButtonState() == BTN_ONECLICK)
  {
    setDisplayMode();
  }
}
#endif

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
#ifdef USE_TEMP_DATA
  case DISPLAY_MODE_SHOW_TEMP:
    displayMode = DISPLAY_MODE_SHOW_TIME;
    tasks.stopTask(show_temp_mode);
    break;
#endif
  case DISPLAY_MODE_SHOW_TIMER_1:
  case DISPLAY_MODE_SHOW_TIMER_2:
    displayMode = DISPLAY_MODE_SHOW_TIME;
    tasks.stopTask(show_timer_mode);
    break;
  default:
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
      displayMode = (displayMode < DISPLAY_MODE_SET_MINUTE) ? (DisplayMode)((byte)displayMode + 1)
                                                            : DISPLAY_MODE_SHOW_TIME;
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
    default:
      break;
    }
    time_checked = true;
    btnUp.setBtnFlag(BTN_FLAG_NONE);
    btnDown.setBtnFlag(BTN_FLAG_NONE);
  }

  // вывод данных на экран ============
  showTimeData(curHour, curMinute);
}

#ifdef USE_TEMP_DATA
void showTemp()
{
  if (!tasks.getTaskState(show_temp_mode))
  {
    tasks.startTask(return_to_default_mode);
    tasks.startTask(show_temp_mode);
  }

  disp.showTemp((int)clock.getTemperature());
}
#endif

void setStateLed(Timer &tmr)
{
  LedsColor mask = LED_COLOR_NONE;

  switch (tmr.getTimerFlag())
  {
  case TIMER_FLAG_RUN:
  case TIMER_FLAG_STOP:
    if (blink_flag)
    {
      mask = (tmr.getTimerFlag() == TIMER_FLAG_RUN) ? LED_COLOR_GREEN : LED_COLOR_RED;
    }
    break;
  case TIMER_FLAG_PAUSED:
    mask = LED_COLOR_GREEN;
    break;
  default:
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

void checkTimers()
{
  DateTime dt = RTC.now();
  timer_1.tick(dt);
  timer_2.tick(dt);

  if ((timer_1.checkTimerState()) || (timer_2.checkTimerState()))
  {
    restartBuzzer();
  }
}

void backupEndPoints()
{
  timer_1.saveState();
  timer_2.saveState();
  tasks.stopTask(backup_end_point);
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
  // определить действующий таймер
  Timer *tmr = (displayMode == DISPLAY_MODE_SHOW_TIMER_1) ? &timer_1 : &timer_2;

  if (!tasks.getTaskState(show_timer_mode))
  {
    n = 0;
    tasks.startTask(show_timer_mode);
    tasks.setTaskInterval(return_to_default_mode, AUTO_EXIT_TIMEOUT * 2000ul);
    restartBlink();
    // инициализировать начальное значение таймера, если оно нулевое, и таймер в состоянии покоя
    if (tmr->getTimerFlag() == TIMER_FLAG_NONE && tmr->getTimerCount() == 0)
    {
      tmr->setTimerCount(data_list.getFirst());
      if (tmr->getTimerType() == IS_TIMER)
      {
        tmr->setTimerCount(tmr->getTimerCount() * 60);
      }
    }
  }

  if (n < 10)
  {
    showTimerChar(tmr->getTimerType());
    n++;
    return;
  }

  // опрос кнопок =====================
  if (tmr->getTimerFlag() == TIMER_FLAG_NONE && btnSet.getBtnFlag() == BTN_FLAG_NEXT)
  {
    tmr->setTimerCount(data_list.getNext());
    if (tmr->getTimerType() == IS_TIMER)
    {
      tmr->setTimerCount(tmr->getTimerCount() * 60);
    }
  }
  btnSet.setBtnFlag(BTN_FLAG_NONE);

  // сброс таймера по нажатию двух кнопок - Up+Down
  if (btnUp.isButtonClosed() && btnDown.isButtonClosed())
  {
    tmr->stop(true);
    btnUp.resetButtonState();
    btnDown.resetButtonState();
  }

  if ((btnUp.getBtnFlag() == BTN_FLAG_NEXT) || (btnDown.getBtnFlag() == BTN_FLAG_NEXT))
  {
    uint32_t t_data = tmr->getTimerCount();
    bool toUp = btnUp.getBtnFlag() == BTN_FLAG_NEXT;
    int8_t d = (toUp) ? 1 : -1; // единица изменения данных
    if (tmr->getTimerType() == IS_TIMER)
    {
      d *= 60; // для будильника в минутах, для таймера - в секундах
    }
    t_data += d;
    uint32_t m = (tmr->getTimerType() == IS_TIMER) ? MAX_DATA * 60ul : MAX_DATA; // максимальное значение; для будильника в минутах, для таймера - в секундах
    if (t_data > m)
    {
      t_data = (!toUp) ? 0 : m;
    }

    tmr->setTimerCount(t_data);
    if (t_data == 0)
    {
      tmr->setTimerFlag(TIMER_FLAG_NONE);
    }
    else if (tmr->getTimerFlag() == TIMER_FLAG_RUN)
    {
      tasks.restartTask(backup_end_point);
      t_data += (tmr->getTimerType() == IS_TIMER) ? secondstime(RTC.now()) : minutstime(RTC.now());
      tmr->setEndPoint(t_data);
    }
    btnUp.setBtnFlag(BTN_FLAG_NONE);
    btnDown.setBtnFlag(BTN_FLAG_NONE);
  }

  switch (btnTimer.getBtnFlag())
  {
  case BTN_FLAG_NEXT:
    if (tmr->getTimerCount() > 0)
    {
      if (tmr->getTimerFlag() < TIMER_FLAG_STOP)
      {
        if (tmr->getTimerType() == IS_TIMER &&
            tmr->getTimerFlag() == TIMER_FLAG_NONE &&
            tmr->getTimerCount() > 0)
        { // сохранять данные нужно только для таймера, для будильника не нужно; и только в минутах
          data_list.saveNewData(tmr->getTimerCount() / 60);
        }
        tmr->startPause(RTC.now());
      }
    }
    break;
  case BTN_FLAG_EXIT:
    if (tmr->getTimerFlag() == TIMER_FLAG_NONE)
    { // смена типа таймера по двойному клику таймер-кнопки
      tmr->setTimerType((TimerType)(!(byte)tmr->getTimerType()));
      tmr->setTimerCount(0);
      tasks.stopTask(show_timer_mode);
    }
    break;
  default:
    break;
  }
  btnTimer.setBtnFlag(BTN_FLAG_NONE);

  // вывод данных на экран ============
  switch (tmr->getTimerType())
  {
  case IS_TIMER:
    if (tmr->getTimerCount() >= 60 || tmr->getTimerFlag() == TIMER_FLAG_NONE ||
        btnUp.isButtonClosed() || btnDown.isButtonClosed())
    {
      byte h, m, s;
      timeinseconds(tmr->getTimerCount(), h, m, s);
      if (s > 0)
      {
        m++;
        if (m == 60)
        {
          m = 0;
          h++;
        }
      }
      showTimeData(h, m);
    }
    else
    {
      (tmr->getTimerFlag() == TIMER_FLAG_STOP) ? showTimeData(0, 0)
                                               : showTimeData(0, tmr->getTimerCount());
    }
    break;
  case IS_ALARM:
    byte h, m, s;
    uint32_t t;
    t = (tmr->getTimerFlag() == TIMER_FLAG_RUN) ? tmr->getEndPoint()
                                                : minutstime(RTC.now()) + tmr->getTimerCount();

    timeinseconds(t * 60ul, h, m, s);
    showTimeData(h, m);
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
      {70, 70, 70, 70, 70, 70, 70, 510}};
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
  static uint16_t b;
  b = (b * 2 + analogRead(LIGHT_SENSOR_PIN)) / 3;
  if (b < LIGHT_THRESHOLD)
  {
    disp.setBrightness(MIN_DISPLAY_BRIGHTNESS);
  }
  else if (b > LIGHT_THRESHOLD + 50)
  {
    disp.setBrightness(MAX_DISPLAY_BRIGHTNESS);
  }
}
#endif

// ===================================================
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
    default:
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
  disp.showTime(hour, minute, displayMode >= DISPLAY_MODE_SHOW_TIMER_1);
}

// ===================================================
void saveTime(byte hour, byte minute)
{
  clock.setSecond(0);
  clock.setHour(hour);
  clock.setMinute(minute);
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

void setDisplay()
{
  switch (displayMode)
  {
  case DISPLAY_MODE_SET_HOUR:
  case DISPLAY_MODE_SET_MINUTE:
    if (!tasks.getTaskState(set_time_mode))
    {
      showTimeSetting();
    }
    break;
#ifdef USE_TEMP_DATA
  case DISPLAY_MODE_SHOW_TEMP:
    if (!tasks.getTaskState(show_temp_mode))
    {
      showTemp();
    }
    break;
#endif
  case DISPLAY_MODE_SHOW_TIMER_1:
  case DISPLAY_MODE_SHOW_TIMER_2:
    if (!tasks.getTaskState(show_timer_mode))
    {
      showTimerMode();
    }
    break;
  default:
    DateTime dt;
    dt = RTC.now();
    disp.showTime(dt.hour(), dt.minute(), blink_flag);
    break;
  }
}

// ===================================================
void setup()
{
  // Serial.begin(9600);

  // ==== часы =========================================
  Wire.begin();
  clock.setClockMode(false); // 24-часовой режим

  // ==== таймеры ======================================
  timer_1.restoreState(RTC.now());
  timer_2.restoreState(RTC.now());

  // ==== кнопки Up/Down ===============================
  btnUp.setLongClickMode(LCM_CLICKSERIES);
  btnUp.setIntervalOfSerial(50);
  btnDown.setLongClickMode(LCM_CLICKSERIES);
  btnDown.setIntervalOfSerial(50);

  // ==== пины =========================================
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_CLOCK_PIN, OUTPUT);
  pinMode(LED_TIMER1_PIN, OUTPUT);
  pinMode(LED_TIMER2_PIN, OUTPUT);

  // ==== задачи =======================================
  byte task_count = 9;
#ifdef USE_TEMP_DATA
  task_count++;
#endif
#ifdef USE_LIGHT_SENSOR
  task_count++;
#endif

  tasks.init(task_count);

  blink_timer = tasks.addTask(500, blink);
  return_to_default_mode = tasks.addTask(AUTO_EXIT_TIMEOUT * 1000ul, returnToDefMode, false);
  set_time_mode = tasks.addTask(100, showTimeSetting, false);
#ifdef USE_TEMP_DATA
  show_temp_mode = tasks.addTask(500, showTemp, false);
#endif
  leds_guard = tasks.addTask(100, setLeds);
  display_guard = tasks.addTask(50, setDisp);
  show_timer_mode = tasks.addTask(50, showTimerMode, false);
  run_buzzer = tasks.addTask(100, runBuzzer, false);
  check_timers = tasks.addTask(100, checkTimers);
  backup_end_point = tasks.addTask(3000, backupEndPoints, false);
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
}
