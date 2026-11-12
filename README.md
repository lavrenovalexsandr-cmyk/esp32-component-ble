# ESP32 Platform BLE

**Универсальный BLE-слой для проектов на ESP32 и ESP-IDF.**

ESP32 Platform BLE — отдельный платформенный репозиторий, предоставляющий базовую BLE-инфраструктуру для прошивок ESP32. Он не содержит логики конкретных датчиков или производителей.

## Назначение

- Инициализация и управление жизненным циклом BLE через NimBLE.
- Сканирование BLE advertising-пакетов.
- Единый платформенный формат рекламного пакета.
- Callback для получения необработанных advertising reports.
- Регистрация независимых драйверов устройств и диспетчеризация advertising-пакетов.
- Передача событий драйверов приложению без навязывания формата измерений.

## Архитектурное разделение

- **ESP32 Platform Core** — общая инфраструктура устройства: Wi-Fi, MQTT, OTA, Home Assistant и системные сервисы.
- **ESP32 Platform BLE** — BLE transport, сканирование, диспетчеризация пакетов и общий контракт драйверов.
- **BLE device drivers** — протоколы конкретных устройств и производителей. Xiaomi LYWSD03MMC будет отдельным проектом.
- **Firmware application** — выбирает нужные драйверы и связывает их с MQTT, Home Assistant или другим прикладным уровнем.

BLE-слой можно использовать без Xiaomi и без MQTT/Home Assistant.

## Структура

~~~text
components/
└── platform_ble/
    ├── include/
    │   ├── platform_ble.h
    │   ├── platform_ble_scan.h
    │   └── platform_ble_device.h
    ├── platform_ble.c
    ├── platform_ble_scan.c
    └── CMakeLists.txt

main/
├── app_main.c
└── CMakeLists.txt

docs/
├── ARCHITECTURE_RU.md
├── API_DESIGN_RU.md
└── CORE_TEST_APPLICATION_RU.md
~~~

## Текущее состояние

**Этапы 1–2 — NimBLE lifecycle, scan, generic driver registry и dispatch реализованы предварительно.**

Реализованы инициализация/остановка NimBLE, ожидание синхронизации host, запуск/остановка сканирования, преобразование advertising report в универсальную структуру и callback приложения. Добавлено тестовое ESP-IDF приложение для ESP32-C3.

Реализованы статический registry до 8 драйверов, регистрация/снятие регистрации, фильтр matches(), вызов on_advertisement() и доставка непрозрачных событий приложению. Пакеты сначала передаются зарегистрированным драйверам, затем raw callback приложения. Новая реализация registry пока не проверена отдельной сборкой и на реальной плате.

## Сборка тестового приложения

Требуется ESP-IDF 6.1 и плата ESP32-C3.

~~~bash
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
~~~

Подробности: [Тестовое приложение](docs/CORE_TEST_APPLICATION_RU.md).

## Документация

- [Архитектура](docs/ARCHITECTURE_RU.md)
- [Проектирование публичного API](docs/API_DESIGN_RU.md)
- [Тестовое приложение](docs/CORE_TEST_APPLICATION_RU.md)
- [English README](README_EN.md)

## Связанные проекты

- [ESP32 Platform Core](https://github.com/lavrenovalexsandr-cmyk/esp32-platform-core)
- [ESP32 Xiaomi LYWSD03MMC firmware (legacy)](https://github.com/lavrenovalexsandr-cmyk/esp32_mqtt_xiaomi_lywsd03)

## Принципы

1. BLE Core не зависит от конкретных моделей, производителей, MQTT или Home Assistant.
2. Advertising-пакет остаётся универсальным и не интерпретирует manufacturer/service data.
3. Драйвер владеет своим протоколом, состоянием, ключами, парсингом и форматом событий.
4. Core предоставляет только общие BLE-типы и механизмы доставки событий.
5. API должен допускать будущее расширение: active scan, GATT, notifications и bonding.
6. Каждый этап проверяется отдельно на реальном оборудовании.

## Лицензия

Лицензия будет выбрана до первой публичной версии.