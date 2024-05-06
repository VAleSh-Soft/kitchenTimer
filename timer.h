#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "shSimpleRTC.h"

#define MAX_DATA 1439 // максимальное количество минут для установки таймера (23 ч, 59 мин)

// получение минут с 1 января 2000 года
uint32_t minutstime(DateTime tm)
{
  static const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint16_t y = tm.year();
  uint8_t m = tm.month();
  if (y >= 2000)
    y -= 2000;
  uint16_t days = tm.day();
  for (uint8_t i = 1; i < m; ++i)
    days += pgm_read_byte(&daysInMonth[i - 1]);
  if (m > 2 && y % 4 == 0)
    ++days;
  days = days + 365 * y + (y + 3) / 4 - 1;
  return ((days * 24L + tm.hour()) * 60 + tm.minute());
}

// получение секунд с 1 января 2000 года
uint32_t secondstime(DateTime tm)
{
  return (minutstime(tm)) * 60 + tm.second();
}

// получение времени суток из секунд с 1 января 2000 года
void timeinseconds(uint32_t _seconds, uint8_t &_hour, uint8_t &_min, uint8_t &_sec)
{
  _seconds = _seconds % 86400; // получение количества секунд, прошедших с начала суток
  _hour = _seconds / 3600;
  _seconds = _seconds % 3600; // получение количества секунд, прошедших с начала часа
  _min = _seconds / 60;
  _sec = _seconds % 60;
}

enum TimerType : uint8_t
{
  IS_TIMER, // флаг типа таймера - собственно, таймер
  IS_ALARM  // флаг типа таймера - будильник
};

enum TimerFlag : uint8_t
{
  TIMER_FLAG_NONE,   // флаг состояния таймера - в покое
  TIMER_FLAG_RUN,    // флаг состояния таймера - работает
  TIMER_FLAG_PAUSED, // флаг состояния таймера - на паузе
  TIMER_FLAG_STOP    // флаг состояния таймера - сработал
};

enum IndexOffset : uint8_t // смещение от стартового индекса в EEPROM для хранения настроек
/* общий размер настроек - 6 байт */
{
  TIMER_TYPE = 0,     // тип таймера, uint8_t
  TIMER_FLAG = 1,     // флаг таймера, uint8_t
  TIMER_END_POINT = 2 // точка останова таймера, uint32_t
};

enum LedsColor : uint8_t
{
  LED_COLOR_NONE,  // 0b0000
  LED_COLOR_GREEN, // 0b0001
  LED_COLOR_RED    // 0b0010
};

// класс таймера
class Timer
{
private:
  uint16_t eeprom_start;
  int32_t timer_count = 0;
  uint32_t end_point = 0;
  bool check_flag = false;
  uint8_t led_green, led_red;

  uint8_t read_eeprom_8(IndexOffset _index)
  {
    return (EEPROM.read(eeprom_start + _index));
  }

  uint32_t read_eeprom_32(IndexOffset _index)
  {
    uint32_t _data;
    EEPROM.get(eeprom_start + _index, _data);
    return (_data);
  }

  void write_eeprom_8(IndexOffset _index, uint8_t _data)
  {
    EEPROM.update(eeprom_start + _index, _data);
  }

  void write_eeprom_32(IndexOffset _index, uint32_t _data)
  {
    EEPROM.put(eeprom_start + _index, _data);
  }

  void checkTimerCount(DateTime dt)
  {
    uint32_t x = (getTimerType() == IS_TIMER) ? secondstime(dt) : minutstime(dt);
    timer_count = end_point - x;
    if (timer_count <= 0)
    {
      stop();
    }
  }

public:
  Timer(uint16_t _eeprom_index, uint8_t _led_green, uint8_t _led_red)
  {
    eeprom_start = _eeprom_index;
    led_green = _led_green;
    led_red = _led_red;
    pinMode(led_red, OUTPUT);
    pinMode(led_green, OUTPUT);

    if (read_eeprom_8(TIMER_TYPE) > (uint8_t)IS_ALARM)
    {
      write_eeprom_8(TIMER_TYPE, (uint8_t)IS_TIMER);
    }
    if (read_eeprom_8(TIMER_FLAG) > (uint8_t)TIMER_FLAG_STOP)
    {
      write_eeprom_8(TIMER_FLAG, (uint8_t)TIMER_FLAG_NONE);
    }
  }

  void restoreState(DateTime _time)
  {
    switch (getTimerFlag())
    {
    case TIMER_FLAG_RUN:
      end_point = read_eeprom_32(TIMER_END_POINT);
      checkTimerCount(_time);
      break;
    case TIMER_FLAG_PAUSED:
      timer_count = read_eeprom_32(TIMER_END_POINT);
      break;
    case TIMER_FLAG_STOP:
      setTimerFlag(TIMER_FLAG_NONE);
      break;
    default:
      break;
    }
  }

  void saveState()
  {
    write_eeprom_32(TIMER_END_POINT, end_point);
  }

  void stop(bool clear = false)
  {
    timer_count = 0;
    (clear) ? setTimerFlag(TIMER_FLAG_NONE) : setTimerFlag(TIMER_FLAG_STOP);
    check_flag = !clear;
  }

  void startPause(DateTime _time)
  {
    if (getTimerFlag() != TIMER_FLAG_STOP)
    {
      if (getTimerFlag() == TIMER_FLAG_RUN)
      {
        // на паузу можно поставить только таймер, для будильника это бессмысленно
        if (getTimerType() == IS_TIMER)
        {
          setTimerFlag(TIMER_FLAG_PAUSED);
          // сохранить оставшееся время
          end_point = timer_count;
          saveState();
        }
      }
      else
      {
        // задать точку останова при старте или при перезапуске после паузы
        end_point = (getTimerType() == IS_TIMER) ? secondstime(_time) : minutstime(_time);
        end_point += timer_count;
        saveState();
        setTimerFlag(TIMER_FLAG_RUN);
      }
    }
  }

  void tick(DateTime _time)
  {
    if (getTimerFlag() == TIMER_FLAG_RUN)
    {
      checkTimerCount(_time);
    }
  }

  void setLed(LedsColor _mask) // включается по маске: 0 - все выключены, 1 - зеленый, 2 - красный
  {
    digitalWrite(led_green, ((uint8_t(_mask) >> (0)) & 0x01));
    digitalWrite(led_red, ((uint8_t(_mask) >> (1)) & 0x01));
  }

  int32_t getTimerCount()
  {
    return (timer_count);
  }

  void setTimerCount(int32_t _count)
  {
    timer_count = _count;
  }

  uint32_t getEndPoint()
  {
    return (end_point);
  }

  void setEndPoint(uint32_t _data)
  {
    end_point = _data;
  }

  TimerFlag getTimerFlag()
  {
    return ((TimerFlag)read_eeprom_8(TIMER_FLAG));
  }

  void setTimerFlag(TimerFlag _flag)
  {
    write_eeprom_8(TIMER_FLAG, (uint8_t)_flag);
  }

  TimerType getTimerType()
  {
    return ((TimerType)read_eeprom_8(TIMER_TYPE));
  }

  void setTimerType(TimerType _type)
  {
    write_eeprom_8(TIMER_TYPE, (uint8_t)_type);
  }

  bool checkTimerState()
  {
    bool result = check_flag;
    check_flag = false;
    return (result);
  }
};
