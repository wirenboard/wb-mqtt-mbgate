Modbus TCP and RTU slave to MQTT gateway (C++)
================================

Зависимости
-----------

* libwbmqtt1 (>= 1.0.6)
* libmodbus
* libjsoncpp
* python и python-mosquitto для генератора конфигурации


Использование
-------------

При первом запуске на основе данных из MQTT генерируется конфигурационный файл, содержащий описания каналов
всех доступных устройств. Для каждого устройства на основе имени канала задаётся уникальная пара 
Unit ID + стартовый адрес. По умолчанию все каналы отключены; для включения каналов наобходимо поставить галочку
в поле Enabled в редакторе конфигурации.

Доступные типы и размеры данных:

* Signed integer - знаковое целое, размеры: 2, 4, 8 байт (1, 2, 4 регистра соответственно)
* Unsigned integer - беззнаковое целое, размеры те же
* BCD - беззнаковое целое в двоично-десятичной нотации (123 => 0x123), размеры те же
* Float - число с плавающей точкой (IEEE 754), размеры - 4, 8 байт (2, 4 регистра соответственно)
* Varchar - текст посимвольно в смежных регистрах. Требуется задать фиксированный размер поля (количество регистров == количеству символов)

Если после первого запуска шлюза в системе появились новые устройства, они будут добавлены в конфигурационный файл при
первом перезапуске демона.


Начальная настройка
-------------------

При начальной конфигурации требуется включить все экспортируемые значения и определить их размеры. Для того, чтобы избежать наложений
адресов при изменении размеров полей, можно воспользоваться пунктом Remap addresses after edit в блоке Registers сверху. Если установить
этот флаг, то после редактирования генератор конфигурационного файла пересчитает адреса регистров в случае наложения. (Для того, чтобы
увидеть изменения адресов, следует перезагрузить конфигурационный файл после сохранения. Возможно, на подготовку нового файла конфигурации
потребуется некоторое время).


См. также
---------

Основная документация для wb-mqtt-mbgate публикуется в wiki: http://contactless.ru/wiki/index.php/%D0%A8%D0%BB%D1%8E%D0%B7_Modbus_TCP 
