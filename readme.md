# KitchenTimer v 3.2

Двухканальный кухонный таймер с часами, построенный на модуле DS3231 и семисегментном индикаторе на драйвере TM1637. Помещается в атмегу 168, возможно, будет работать и на восьмой атмеге, но проверить возможности не было.

<hr>

- <a href=#l1>Индикация</a>
- <a href=#l2>Управление</a>
1. <a href=#l3>Режим отображения текущего времени</a>
1. <a href=#l4>Режим настройки времени, настройка часов</a>
1. <a href=#l5>Режим настройки времени, настройка минут</a>
1. <a href=#l6>Режим отображения температуры</a>
1. <a href=#l7>Режим отображения таймера 1</a>
1. <a href=#l8>Режим отображения таймера 2</a>
- <a href=#l9>Подключение компонентов</a>

<hr>

Два независимых канала могут использоваться как в режиме таймера, т.е. для отсчета заданного количества минут, так и в режиме будильника, т.е. для срабатывания в заранее заданное время. 

Последние десять использованных значений времени сохраняются в EEPROM и могут быть использованы при следующем запуске таймера.

Дополнительно предусмотрен вывод на экран показаний температурного датчика микросхемы DS3231. Для этого в режиме отображения текущего времени нужно нажать на кнопку Up.

Предусмотрена возможность управления яркостью индикатора по датчику света - фоторезистор, подключенный к пину A3. Схема подключения: GND -> R(10k) -> Rl <- VCC, где R - резистор 10кОм, Rl - фоторезистор. Если эта возможность нужна, раскомментируйте строку `#define USE_LIGHT_SENSOR` в файле **kitchenTimer.h**, в противном случае индикатор всегда будет работать с максимальной яркостью.

<a name=l1></a>
## Индикация

Кроме семисегментного индикатора прибор располагает светодиодными индикаторами. Три индикатора, показывающих, что в данным момент отображается на экране 
- текущее время или настройка текущего времени;
- таймер 1;
- таймер 2;

И два двухцветных светодиода с общим катодом, отображающих состояние каналов таймера
- мигающий зеленый - канал запущен, идет отсчет времени;
- непрерывный зеленый - канал поставлен на паузу;
- мигающий красный - канал сработал;

При срабатывании канала на пять минут включается прерывистый звуковой сигнал. 

<a name=l2></a>
## Управление

Для управления используются четыре кнопки
- **Set** - настройка часов;
- **Up** - увеличение настраиваемой величины;
- **Down** - уменьшение настраиваемой величины;
- **Timer** - управление таймерами;

Для сброса сработавшего канала нужно нажать на любую кнопку. При этом звуковой сигнал отключится, индикатор состояния канала погаснет.

Для сброса еще не сработавшего запущенного канала нужно переключиться на него, а дальше можно пойти двумя путями:
1. кнопкой Down уменьшить время канал до нуля (если канал используется в режиме таймера) или до текущего времени (если канал используется в режиме будильника); в этом случае канал будет сброшен, и вы сможете установить новое время для его запуска;
1. одновременно нажать кнопки Up и Down; в этом случае канал будет сброшен, и прибор будет переключен на режим отображения текущего времени;

Из любого режима предусмотрен автоматический выход в режим отображения текущего времени при отсутствии активности пользователя, т.е. если за определенный промежуток времени не была нажата ни одна кнопка. В прошивке задан интервал 6 секунд. Для режимов отображения таймеров этот интервал удваивается - 12 секунд. На состояние каналов это не влияет, если они запущены, то отсчет времени продолжится в фоновом режиме.

<a name=l3></a>
### Режим отображения текущего времени

**Кнопка Set** 
- длинный клик (т.е. удержание кнопки нажатой более 800 милисекунд) - переход в режим настройки времени - настройка часов;

**Кнопка Up** 
- короткий клик - переход в режим отображения температуры;

<a name=l4></a>
### Режим настройки времени, настройка часов

**Кнопка Set** 
- длинный клик - выход в режим отбражения текущего времени; внесенные изменения при этом сохраняются;
- короткий клик - переход в режим настройки минут; внесенные изменения при этом сохраняются;

**Кнопка Up** 
- короткий клик - увеличение часа на одну единицу;
- длинный клик с удержанием кнопки нажатой - непрерывное увеличение часа со скоростью примерно десять единиц в секунду;

**Кнопка Down** 
- короткий клик - уменьшение часа на одну единицу;
- длинный клик с удержанием кнопки нажатой - непрерывное уменьшение часа со скоростью примерно десять единиц в секунду;

Значение часа может изменяться в пределах от 0 до 23, при достижении этих значений происходит переход к противоположному значению, т.е. изменение значения закольцовано.

<a name=l5></a>
### Режим настройки времени, настройка минут

**Кнопка Set** 
- длинный клик - выход в режим отбражения текущего времени; внесенные изменения при этом сохраняются;
- короткий клик - то же самое;

**Кнопка Up** 
- короткий клик - увеличение минут на одну единицу;
- длинный клик с удержанием кнопки нажатой - непрерывное увеличение минут со скоростью примерно десять единиц в секунду;

**Кнопка Down** 
- короткий клик - уменьшение минут на одну единицу;
- длинный клик с удержанием кнопки нажатой - непрерывное уменьшение минут со скоростью примерно десять единиц в секунду;

Значение минут может изменяться в пределах от 0 до 59, при достижении этих значений происходит переход к противоположному значению, т.е. изменение значения закольцовано.

При любом сохранении времени значение секунд обнуляется.

<a name=l6></a>
### Режим отображения температуры

**Кнопка Up** 
- короткий клик - переход в режим отображения текущего времени;

<a name=l7></a>
### Режим отображения таймера 1

При включении этого режима на экране устанавливается последнее использованное значение времени (для режима таймера) или текущее время + последнее использованное значение времени (для режима будильника).

**Кнопка Set** 
- длинный клик - выход в режим отбражения текущего времени; на состояние канала это не влияет, т.е. если он был запущен, то отсчет времени продолжится в фоновом режиме;
- короткий клик - вывод на экран следующего сохраненного значения ранее использованного времени;

**Кнопка Up** 
- короткий клик - увеличение минут на одну единицу; при достижении значения 59 при следующем клике происходит увеличение на единицу значения часов;
- длинный клик с удержанием кнопки нажатой - непрерывное увеличение минут со скоростью примерно двадцать единиц в секунду;

**Кнопка Down** 
- короткий клик - уменьшение минут на одну единицу; при достижении значения 0 при следующем клике происходит уменьшение на единицу значения часов;
- длинный клик с удержанием кнопки нажатой - непрерывное уменьшение минут со скоростью примерно десять единиц в секунду;

Значение на экране может меняться в интервале от 00:00 до 23:59. В режиме таймера перейти через эти значения не получится. В режиме будильника время сработки можно устанавливать в интервале от текущего времени до текущего времени минут одна минута.

**Кнопка Up + кнопка Down** 
- одновременный короткий клик - сброс канала и выход в режим отображения текущего времени

**Кнопка Timer** 
- короткий клик - старт или постановка канала на паузу; для режима будильника возможен только старт, постановка будильника на паузу не предумотрена, т.к. не имеет смысла;
- длинный клик - переход в режим отбражения таймера 2; на состояние канала это не влияет, т.е. если он был запущен, то отсчет времени продолжится в фоновом режиме;
- двойной клик - (только в режиме покоя) переключение режима канала (таймер / будильник);

Текущий режим канала отображается в течение секунды при входе в него:
- **dur** - режима таймера;
- **End** - режим будильника;

Текущий режим сохраняется в памяти и используется при следующем включении таймера.

<a name=l8></a>
### Режим отображения таймера 2 (практически идентично первому таймеру)

При включении этого режима на экране так же устанавливается последнее использованное значение времени (для режима таймера) или текущее время + последнее использованное значение времени (для режима будильника).

**Кнопка Set** 
- длинный клик - выход в режим отбражения текущего времени; на состояние канала это не влияет, т.е. если он был запущен, то отсчет времени продолжится в фоновом режиме;
- короткий клик - вывод на экран следующего сохраненного значения ранее использованного времени;

**Кнопка Up** 
- короткий клик - увеличение минут на одну единицу; при достижении значения 59 при следующем клике происходит увеличение на единицу значения часов;
- длинный клик с удержанием кнопки нажатой - непрерывное увеличение минут со скоростью примерно двадцать единиц в секунду;

**Кнопка Down** 
- короткий клик - уменьшение минут на одну единицу; при достижении значения 0 при следующем клике происходит уменьшение на единицу значения часов;
- длинный клик с удержанием кнопки нажатой - непрерывное уменьшение минут со скоростью примерно десять единиц в секунду;

Значение на экране может меняться в интервале от 00:00 до 23:59. В режиме таймера перейти через эти значения не получится. В режиме будильника время сработки можно устанавливать в интервале от текущего времени до текущего времени минут одна минута.

**Кнопка Up + кнопка Down** 
- одновременный короткий клик - сброс канала и выход в режим отображения текущего времени

**Кнопка Timer** 
- короткий клик - старт или постановка канала на паузу; для режима будильника возможен только старт, постановка будильника на паузу не предумотрена, т.к. не имеет смысла;
- длинный клик - переход в режим отбражения текущего времени; на состояние канала это не влияет, т.е. если он был запущен, то отсчет времени продолжится в фоновом режиме;
- двойной клик - (только в режиме покоя) переключение режима канала (таймер / будильник);

Текущий режим канала отображается в течение секунды при входе в него:
- **dur** - режима таймера;
- **End** - режим будильника;

Текущий режим сохраняется в памяти и используется при следующем включении таймера.

<a name=l9></a>
## Подключение компонентов

Пины для подключения кнопок, пищалки, светодиодов и датчика света определены в файле kitchenTimer.h

Пины для подключения экрана определены в файле display.h

Модуль DS3231 подключен к пинам A4 (SDA) и A5 (SCL)