#pragma once
#include <Arduino.h>
#include <Wire.h>

#define CLOCK_ADDRESS 0x68

#define SECONDS_FROM_1970_TO_2000 946684800

static const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// ==== shDateTime =====================================

// shDateTime (все вместе) от JeeLabs/Adafruit
// Простой класс даты/времени общего назначения (без обработки TZ/DST/дополнительных секунд!)
class shDateTime
{
public:
  shDateTime(uint32_t t = 0);

  shDateTime(uint16_t year, uint8_t month, uint8_t day,
             uint8_t hour = 0, uint8_t min = 0, uint8_t sec = 0);

  shDateTime(const shDateTime &copy);

  shDateTime(const char *date, const char *time);
  uint16_t year() const;
  uint8_t month() const;
  uint8_t day() const;
  uint8_t hour() const;
  uint8_t minute() const;
  uint8_t second() const;
  uint8_t dayOfTheWeek() const;

  // 32-битное время в секундах с 01.01.2000
  long secondstime() const;

  /*
   * 32-битное время в секундах с 01.01.1970
   *
   * УТВЕРЖДЕНИЕ КОРРЕКТНО ДЛЯ ЛОКАЛЬНОГО ВРЕМЕНИ; ЧТОБЫ ИСПОЛЬЗОВАТЬ
   * ЭТОТ МЕТОД ДЛЯ ПОЛУЧЕНИЯ ПРАВИЛЬНОГО ВРЕМЕНИ UNIX, ВЫ ДОЛЖНЫ
   * ВЫЗВАТЬ ЭТОТ МЕТОД ПОСЛЕ УСТАНОВКИ ЧАСОВ В UTC.
   */
  uint32_t unixtime(void) const;

protected:
  uint8_t yOff, m, d, hh, mm, ss;

private:
  static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d);

  static long time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s);
};

// ---- shDateTime private ------------------------

uint16_t shDateTime::date2days(uint16_t y, uint8_t m, uint8_t d)
{
  if (y >= 2000)
    y -= 2000;
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i)
    days += pgm_read_byte(daysInMonth + i - 1);
  if (m > 2 && y % 4 == 0)
    ++days;
  return days + 365 * y + (y + 3) / 4 - 1;
}

long shDateTime::time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s)
{
  return ((days * 24L + h) * 60 + m) * 60 + s;
}

// ---- shDateTime public -------------------------

shDateTime::shDateTime(uint32_t t)
{
  t -= SECONDS_FROM_1970_TO_2000; // переместить точку времени с 1970 года на 2000

  ss = t % 60;
  t /= 60;
  mm = t % 60;
  t /= 60;
  hh = t % 24;
  uint16_t days = t / 24;
  uint8_t leap;
  for (yOff = 0;; ++yOff)
  {
    leap = yOff % 4 == 0;
    if (days < 365u + leap)
      break;
    days -= 365 + leap;
  }
  for (m = 1;; ++m)
  {
    uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
    if (leap && m == 2)
      ++daysPerMonth;
    if (days < daysPerMonth)
      break;
    days -= daysPerMonth;
  }
  d = days + 1;
}

shDateTime::shDateTime(uint16_t year, uint8_t month, uint8_t day,
                       uint8_t hour, uint8_t min, uint8_t sec)
{
  yOff = year % 100;
  m = (month <= 12 && month > 0) ? month : 1;
  d = (day <= 31 && day > 0) ? day : 1;
  hh = (hour <= 23) ? hour : 0;
  mm = (min <= 59) ? min : 0;
  ss = (sec <= 59) ? sec : 0;
}

shDateTime::shDateTime(const shDateTime &copy) : yOff(copy.yOff), m(copy.m), d(copy.d),
                                                 hh(copy.hh), mm(copy.mm), ss(copy.ss) {}

uint16_t shDateTime::year() const { return yOff; }
uint8_t shDateTime::month() const { return m; }
uint8_t shDateTime::day() const { return d; }
uint8_t shDateTime::hour() const { return hh; }
uint8_t shDateTime::minute() const { return mm; }
uint8_t shDateTime::second() const { return ss; }

/*
 * 32-битное время в секундах с 01.01.1970
 *
 * УТВЕРЖДЕНИЕ КОРРЕКТНО ДЛЯ ЛОКАЛЬНОГО ВРЕМЕНИ; ЧТОБЫ ИСПОЛЬЗОВАТЬ
 * ЭТОТ МЕТОД ДЛЯ ПОЛУЧЕНИЯ ПРАВИЛЬНОГО ВРЕМЕНИ UNIX, ВЫ ДОЛЖНЫ
 * ВЫЗВАТЬ ЭТОТ МЕТОД ПОСЛЕ УСТАНОВКИ ЧАСОВ В UTC.
 */
uint32_t shDateTime::unixtime(void) const
{
  uint32_t t;
  uint16_t days = date2days(yOff, m, d);
  t = time2long(days, hh, mm, ss);
  t += SECONDS_FROM_1970_TO_2000; // seconds from 1970 to 2000

  return t;
}

// ==== end shDateTime =================================

// ==== shSimpleRTC ==================================

class shSimpleRTC
{
private:
  shDateTime cur_time;

  uint8_t decToBcd(uint8_t val);
  uint8_t bcdToDec(uint8_t val);

  bool isClockPresent();

  uint8_t read_register(uint8_t reg);

  void write_register(uint8_t reg, uint8_t data);

public:
  /**
   * @brief конструктор объекта RTC
   *
   */
  shSimpleRTC();

  /**
   * @brief запрос текущих времени и даты из RTC и сохранение их во внутреннем буфере
   *
   */
  void now();

  /**
   * @brief получение текущего времени и даты из внутреннего буфера
   *
   * @return shDateTime
   */
  shDateTime getCurTime();

  /**
   * @brief установка текущего времени
   *
   * @param _hour часы для установки
   * @param _minute минуты для установки
   * @param _second секунды для установки
   */
  void setCurTime(uint8_t _hour, uint8_t _minute, uint8_t _second);

  /**
   * @brief установка текущей даты
   *
   * @param _date число месяца для установки
   * @param _month номер месяца для установки (1..12)
   */
  void setCurDate(uint8_t _date, uint8_t _month);

  /**
   * @brief установка текущего года
   *
   * @param _year год для установки (0..99 )
   */
  void setCurYear(uint8_t _year);

  /**
   * @brief возвращает температуру внутреннего датчика DS3231; работает только с DS3231
   *
   * @return int8_t
   */
  int16_t getTemperature();

  /**
   * устанавливает режим 12-часовой (true) или 24-часовой (false).
   * Одна вещь, которая меня беспокоит в том, как я это написал,
   * заключается в том, что если чтение и правка происходят в правильную
   * почасовую миллисекунду, часы будут переведены на час назад.
   * Однако не знаю, как это сделать лучше, и пока режим не устанавливается
   * часто, риск очень минимален.
   * Риск нулевой, если вы вызываете это ДО установки часа, поскольку
   * функция setHour() не меняет этот режим.
   */
  void setClockMode(bool h12);

  /**
   * @brief проверяем, запущен ли генератор
   *
   * @return true
   * @return false
   */
  bool isRunning();

  /**
   * @brief запускаем генератор, в том числе включаем работу от батареи
   *
   */
  void startRTC();
};

// ---- shSimpleRTC private ---------------------

uint8_t shSimpleRTC::decToBcd(uint8_t val) { return ((val / 10 * 16) + (val % 10)); }
uint8_t shSimpleRTC::bcdToDec(uint8_t val) { return ((val / 16 * 10) + (val % 16)); }

bool shSimpleRTC::isClockPresent()
{
  Wire.beginTransmission(CLOCK_ADDRESS);
  return (Wire.endTransmission() == 0);
}

uint8_t shSimpleRTC::read_register(uint8_t reg)
{
  Wire.beginTransmission(CLOCK_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(CLOCK_ADDRESS, 1);
  return Wire.read();
}

void shSimpleRTC::write_register(uint8_t reg, uint8_t data)
{
  Wire.beginTransmission(CLOCK_ADDRESS);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

// ---- shSimpleRTC public ----------------------

shSimpleRTC::shSimpleRTC() {}

void shSimpleRTC::now()
{
  if (isClockPresent())
  {
    uint8_t b0 = read_register(0x00);
    uint8_t b1 = read_register(0x01);
    uint8_t b2 = read_register(0x02);
    uint8_t b4 = read_register(0x04);
    uint8_t b5 = read_register(0x05);
    uint8_t b6 = read_register(0x06);

    cur_time = shDateTime((uint16_t)bcdToDec(b6), bcdToDec(b5 & 0x7F), bcdToDec(b4),
                          bcdToDec(b2), bcdToDec(b1), bcdToDec(b0 & 0x7F));
  }
  else
  {
    cur_time = shDateTime(0, 1, 1, 0, 0, 0);
  }
}

shDateTime shSimpleRTC::getCurTime() { return (cur_time); }

void shSimpleRTC::setCurTime(uint8_t _hour, uint8_t _minute, uint8_t _second)
{
  if (isClockPresent())
  {
    write_register(0x00, decToBcd(_second));
    write_register(0x01, decToBcd(_minute));
    // устанавливаем режим 24 часа
    setClockMode(false);
    write_register(0x02, decToBcd(_hour));
    // Очищаем флаг OSF
    uint8_t temp_buffer = read_register(0x0f);
    write_register(0x0f, (temp_buffer & 0x7F));
  }
}

void shSimpleRTC::setCurDate(uint8_t _date, uint8_t _month)
{
  if (isClockPresent())
  {
    write_register(0x04, decToBcd(_date));
    write_register(0x05, decToBcd(_month));
  }
}

void shSimpleRTC::setCurYear(uint8_t _year)
{
  if (isClockPresent())
  {
    write_register(0x06, decToBcd(_year % 100));
  }
}

int16_t shSimpleRTC::getTemperature()
{
  uint8_t tMSB, tLSB;
  int16_t temp3231 = -127;

  if (isClockPresent())
  {                             // температурные регистры (11h-12h) обновляются автоматически каждые 64 секунды.
    tMSB = read_register(0x11); // целая часть температуры
    tLSB = read_register(0x12); // дробная часть температуры (два старших бита) с шагом 0.25 градуса

    // t - температура в четвертях градуса, одно знаковое число;
    // для отрицательных значений tMSB - это ceil(t), а не floor(t),
    // поэтому округлять дробь по модулю нельзя, нужно работать со всем числом
    int16_t t = ((int16_t)(int8_t)tMSB << 2) | (tLSB >> 6);
    // округление к ближайшему целому: дробная часть >= 0.5 увеличивает значение на 1 градус
    if (t & 0x02)
    {
      t += 4;
    }
    temp3231 = (t & (int16_t)0xFFFC) >> 2;
  }

  return (temp3231);
}

void shSimpleRTC::setClockMode(bool h12)
{
  if (isClockPresent())
  {
    uint8_t temp_buffer;

    // старт считывания байта 0x02.
    temp_buffer = read_register(0x02);

    // установка заданного флага:
    if (h12)
    {
      temp_buffer = temp_buffer | 0b01000000;
    }
    else
    {
      temp_buffer = temp_buffer & 0b10111111;
    }

    // запись байта
    write_register(0x02, temp_buffer);
  }
}

bool shSimpleRTC::isRunning()
{
  if (isClockPresent())
  {
    return (!(read_register(0x0f) >> 7));
  }

  return false;
}

void shSimpleRTC::startRTC()
{
  uint8_t temp_buffer = read_register(0x0e) & 0b11100111;
  // поднимаем флаг BBSQW - работа от батареи
  temp_buffer = temp_buffer | 0b01000000;
  // устанавливаем ~EOSC и INTCN в 0 - запускаем генератор
  temp_buffer = temp_buffer & 0b01111011;
  // записываем контрольный бит
  write_register(0x0e, temp_buffer);
}

// ==== end shSimpleRTC ==============================
