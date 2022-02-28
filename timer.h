#pragma once
#include <Arduino.h>
#include <DS3231.h>

#define MAX_DATA 1439 // максимальное количество минут для установки таймера (23 ч, 59 мин)

#define IS_TIMER 0 // флаг типа таймера - собственно, таймер
#define IS_ALARM 1 // флаг типа таймера - будильник

#define TIMER_FLAG_NONE 0   // флаг состояния таймера - в покое
#define TIMER_FLAG_RUN 1    // флаг состояния таймера - работает
#define TIMER_FLAG_PAUSED 2 // флаг состояния таймера - на паузе
#define TIMER_FLAG_STOP 3   // флаг состояния таймера - сработал

// класс таймера
class Timer
{
private:
  byte timer_type = IS_TIMER;
  uint16_t timer_count = 0;
  byte timer_second = 0;
  byte _second = 255;
  byte timer_flag = TIMER_FLAG_NONE;
  bool check_flag = false;
  byte led_green, led_red;

public:
  Timer(byte _type, byte _led_green, byte _led_red)
  {
    timer_type = _type;
    if (timer_type > IS_ALARM)
    {
      timer_type = IS_ALARM;
    }
    led_green = _led_green;
    led_red = _led_red;
  }

  void tick(DateTime _time)
  {
    if (timer_flag = TIMER_FLAG_RUN)
    {
      switch (timer_type)
      {
      case IS_TIMER:
        if (_time.second() != _second)
        {
          if (_second == 255)
          {
            timer_second = 60;
          }
          else if (--timer_second == 0)
          {
            timer_count--;
            timer_second = 60;
          }
          _second = _time.second();
        }
        if (timer_count == 0)
        {
          timer_flag = TIMER_FLAG_STOP;
          check_flag = true;
          _second = 255;
        }
        break;
      case IS_ALARM:
        if (timer_count == _time.hour() * 60 + _time.minute())
        {
          timer_flag = TIMER_FLAG_STOP;
          check_flag = true;
        }
        break;
      }
    }
  }

  void setLed(byte _mask) // включается по маске: 0 - все выключены, 1 - зеленый, 2 - красный
  {
    if (_mask > 2)
    {
      _mask = 0;
    }
    digitalWrite(led_green, (((_mask) >> (0)) & 0x01));
    digitalWrite(led_red, (((_mask) >> (1)) & 0x01));
  }

  uint16_t getTimerCount()
  {
    return (timer_count);
  }

  void setTimerCount(uint16_t _count)
  {
    timer_count = (_count > MAX_DATA) ? MAX_DATA : _count;
  }

  byte getTimerSecond()
  {
    return (timer_second);
  }

  byte getTimerFlag()
  {
    return (timer_flag);
  }

  void setTimerFlag(byte _flag)
  {
    timer_flag = (_flag > TIMER_FLAG_STOP) ? TIMER_FLAG_STOP : _flag;
  }

  bool getCheckFlag()
  {
    bool result = check_flag;
    check_flag = false;
    return (result);
  }
};
