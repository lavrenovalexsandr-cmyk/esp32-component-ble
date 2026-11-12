# Тестовое приложение ESP32 Platform BLE

## Назначение

В корне репозитория находится минимальное ESP-IDF приложение для проверки инициализации NimBLE и получения BLE advertising reports. Приложение не подключает драйверы конкретных устройств.

## Требования

- ESP-IDF 6.1;
- плата на ESP32-C3;
- BLE-устройства или телефон с включённым Bluetooth advertising поблизости.

## Сборка

Из корня репозитория:

~~~bash
idf.py set-target esp32c3
idf.py build
~~~

## Прошивка и монитор

~~~bash
idf.py -p PORT flash monitor
~~~

В логе ожидаются сообщения о запуске NimBLE host, синхронизации и запуске сканирования, после чего должны появляться строки ADV с BLE-адресом, RSSI и длиной advertising payload.

## Что проверяет этот тест

1. Инициализацию NimBLE через platform_ble_init().
2. Синхронизацию NimBLE host.
3. Запуск passive scan.
4. Преобразование GAP advertising report в platform_ble_adv_packet_t.
5. Передачу пакета приложению через scan callback.

## Ограничения текущего этапа

- Driver registry пока не реализован.
- GATT client, соединения, bonding и notifications пока не реализованы.
- Тест подтверждает только базовое обнаружение advertising reports.
- До сборки и проверки на реальной плате реализация считается предварительной.
