#pragma once
#include <Arduino.h>
#include <EEPROM.h>

// класс списка сохраненных значений таймера
class DataList
{
private:
  uint8_t data_count = 10;           // количество сохраняемых записей
  uint16_t first_index = 100;        // начальный индекс для сохранения
  uint16_t max_index = 118;          // конечный индекс для сохранения
  uint16_t last_index = first_index; // последний индекс с сохраненным значением; дальше идут пустые ячейки
  uint16_t cur_index = first_index;  // текущий индекс списка
  uint16_t max_data = 1439;          // максимальное значение, которое может храниться в ячейке
  uint8_t data_size = 2;            // размер данных в байтах

  uint16_t read_eeprom_16(uint16_t _index)
  {
    uint16_t _data;
    EEPROM.get(_index, _data);
    return (_data);
  }

  uint16_t getData(uint16_t _index)
  {
    uint16_t result = 0;
    if ((_index >= first_index) &&
        (_index <= max_index) &&
        ((_index - first_index) % data_size == 0))
    {
      result = read_eeprom_16(_index);
      if (result > max_data)
      {
        result = max_data;
        EEPROM.put(_index, result);
      }
    }

    return (result);
  }

  void getNextIndex()
  {
    if ((cur_index += data_size) > last_index)
    {
      cur_index = first_index;
    }
  }

  void getLastIndex()
  {
    last_index = first_index;
    for (uint16_t i = first_index; i <= max_index; i += data_size)
    {
      if (read_eeprom_16(i) > 0)
      {
        last_index = i;
      }
      else
      {
        break;
      }
    }
    if (last_index < max_index)
    {
      for (uint16_t i = last_index + data_size; i <= max_index; i += data_size)
      {
        EEPROM.put(i, 0);
      }
    }
  }

  int16_t findData(uint16_t _data)
  {
    int16_t result = -1;
    for (uint16_t i = first_index; i <= last_index; i += data_size)
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
  DataList(uint16_t _first_index, uint8_t _count, uint16_t _max_data)
  {
    data_count = _count;
    first_index = _first_index;
    max_index = first_index + data_count * data_size - data_size;
    max_data = _max_data;
    for (uint16_t i = first_index; i <= max_index; i += data_size)
    {
      if (read_eeprom_16(i) > max_data)
      {
        EEPROM.put(i, 0);
      }
    }
    getLastIndex();
  }

  // получение данных из первой ячейки
  uint16_t getFirst()
  {
    cur_index = first_index;
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
      n = (last_index == max_index) ? last_index - data_size : last_index;
    }
    else
    {
      n -= data_size;
    }
    if (n >= first_index)
    {
      for (uint16_t i = n; i >= first_index; i -= data_size)
      {
        EEPROM.put(i + data_size, getData(i));
      }
    }
    EEPROM.put(first_index, _data);
    getLastIndex();
  }
};
