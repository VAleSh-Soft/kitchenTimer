#pragma once
#include <Arduino.h>
#include <TM1637Display.h> // https://github.com/avishorp/TM1637

TM1637Display tm(11, 10); // CLK, DAT

// ==== класс для вывода данных на экран =============
class Display
{
private:
  byte *data = NULL;

public:
  Display()
  {
    data = new byte[4];
    clear();
  }

  void clear()
  {
    for (byte i = 0; i < 4; i++)
    {
      data[i] = 0x00;
    }
  }

  void setDispData(byte _index, byte _data)
  {
    if (_index < 4)
    {
      data[_index] = _data;
    }
  }

  byte getDispData(byte _index)
  {
    byte result = 0;
    if (_index < 4)
    {
      result = data[_index];
    }
    return (result);
  }

  void show()
  {
    bool flag = false;
    static byte _data[4] = {0x00, 0x00, 0x00, 0x00};
    for (byte i = 0; i < 4; i++)
    {
      flag = _data[i] != data[i];
      if (flag)
      {
        break;
      }
    }
    if (flag)
    {
      for (byte i = 0; i < 4; i++)
      {
        _data[i] = data[i];
      }
      tm.setSegments(data);
    }
  }

  void setBrightness(byte brightness, bool on = true)
  {
    tm.setBrightness(brightness, on);
  }

  byte encodeDigit(byte num)
  {
    return (tm.encodeDigit(num));
  }
};
