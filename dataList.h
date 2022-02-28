#pragma once
#include <Arduino.h>
#include <avr/eeprom.h>

// класс списка сохраненных значений таймера
class DataList
{
private:
#define DATA_COUNT 10
#define FIRST_INDEX 100
#define MAX_INDEX 118
  uint16_t last_index = FIRST_INDEX;
  uint16_t cur_index = FIRST_INDEX;

  uint16_t getData(uint16_t _index)
  {
    uint16_t result = 0;
    if ((_index >= FIRST_INDEX) && (_index <= MAX_INDEX) && !((_index) >> (0) & 0x01))
    {
      result = eeprom_read_word(_index);
      if (result > MAX_DATA)
      {
        result = MAX_DATA;
        eeprom_update_word(_index, result);
      }
    }

    return (result);
  }

  void getNextIndex()
  {
    if ((cur_index += 2) > last_index)
    {
      cur_index = FIRST_INDEX;
    }
  }

  void getLastIndex()
  {
    last_index = FIRST_INDEX;
    for (uint16_t i = FIRST_INDEX; i <= MAX_INDEX; i += 2)
    {
      if (eeprom_read_word(i) > 0)
      {
        last_index = i;
      }
      else
      {
        break;
      }
    }
    if (last_index < MAX_INDEX)
    {
      for (uint16_t i = last_index + 2; i <= MAX_INDEX; i += 2)
      {
        eeprom_update_word(i, 0);
      }
    }
  }

  int16_t findData(uint16_t _data)
  {
    int16_t result = -1;
    for (uint16_t i = FIRST_INDEX; i <= last_index; i += 2)
    {
      if (getData(i) == _data)
      {
        result = i;
        break;
      }
    }
    return (result);
  }

public:
  DataList()
  {
    for (uint16_t i = FIRST_INDEX; i <= MAX_INDEX; i += 2)
    {
      if (eeprom_read_word(i) > MAX_DATA)
      {
        eeprom_update_word(i, 0);
      }
    }
    getLastIndex();
  }

  // получение данных из первой ячейки
  uint16_t getFirst()
  {
    cur_index = FIRST_INDEX;
    return (getData(cur_index));
  }

  // получение данных из ячейки, следующей за текущей
  uint16_t getNext()
  {
    getNextIndex();
    return (getData(cur_index));
  }

  // сохранение новых данных в первую ячейку со сдвигом остальных
  void saveNewData(uint16_t _data)
  {
    int16_t n = findData(_data);
    if (n < 0)
    {
      n = (last_index == MAX_INDEX) ? last_index - 2 : last_index;
    }
    else
    {
      n -= 2;
    }
    if (n >= FIRST_INDEX)
    {
      for (uint16_t i = n; i >= FIRST_INDEX; i -= 2)
      {
        eeprom_update_word(i + 2, getData(i));
      }
    }
    eeprom_update_word(FIRST_INDEX, _data);
    getLastIndex();
  }
};
