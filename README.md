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

**Этапы 1–2 — NimBLE lifecycle, scan, generic driver registry и dispatch реализованы и проверены на ESP32-C3.**

Реализованы инициализация/остановка NimBLE, ожидание синхронизации host, запуск/остановка сканирования, преобразование advertising report в универсальную структуру и callback приложения. Добавлено тестовое ESP-IDF приложение для ESP32-C3.

Реализованы статический registry до 8 драйверов, регистрация/снятие регистрации, фильтр matches(), вызов on_advertisement() и доставка непрозрачных событий приложению. Пакеты сначала передаются зарегистрированным драйверам, затем raw callback приложения. После добавления registry проект успешно собран и проверен на ESP32-C3: NimBLE host синхронизируется, passive scan принимает advertising reports, тестовый драйвер обрабатывает пакеты и передаёт application event после каждых 100 пакетов. В тестовое приложение добавлены startup self-tests для duplicate ID, лимита/переполнения registry, unregister, освобождения слота и event API. Также добавлен live-check, что драйвер с matches() == false не получает on_advertisement(). Startup self-tests успешно выполнены на ESP32-C3: подтверждены duplicate ID, capacity/overflow, unregister, повторное удаление, повторное использование слота, event callback, отсутствие callback и неизвестный driver ID. Во время live scan подтверждена фильтрация драйвера с matches() == false; advertising reports продолжают поступать. На ESP32-C3 также проверены явная остановка passive scan, повторный запуск active scan, автоматическое завершение active scan по duration_ms и изоляция ошибки on_advertisement(): тестовый драйвер вернул ESP_FAIL, но следующий драйвер обработал столько же пакетов (145).

Аппаратная проверка duplicate filtering и Scan Response пройдена на ESP32-C3. При passive scan с filter_duplicates=false получено 153 reports: 27 уникальных сигнатур и 126 повторов. При filter_duplicates=true получено 29 reports и 0 повторов. Во время active scan получено 383 reports, включая 132 Scan Response. Результаты подтверждают работу duplicate filtering и приём Scan Response через общий advertising callback.

## Производительность dispatch

Текущая реализация последовательно проверяет advertising packet всеми зарегистрированными драйверами. При установленном лимите в 8 драйверов это простой и предсказуемый подход; преждевременно вводить индексирование или сложную систему фильтров не требуется.

Например, при 500 advertising reports в секунду и 8 зарегистрированных драйверах будет выполнено до 4000 вызовов matches() в секунду. Для ESP32-C3 это ориентировочно небольшая нагрузка, если matches() — короткая проверка заголовков/полей пакета без выделения памяти и тяжёлых вычислений. Фактическую нагрузку следует оценивать измерениями на целевом устройстве.

Правила для драйверов:

- matches() должен быть быстрым, не иметь побочных эффектов и не выполнять протокольную обработку;
- on_advertisement() должен выполнять только необходимую быструю обработку принятого пакета;
- криптографию, длительный parsing, обращения к NVS, сетевые операции и ожидания следует переносить в собственную FreeRTOS task;
- для передачи данных в task используйте собственную очередь и копируйте данные, если исходный payload должен пережить callback;
- не выводите лог для каждого advertising report: интенсивное логирование может стоить дороже самого dispatch.

Если количество драйверов или измеренная нагрузка существенно вырастут, можно добавить общие предварительные фильтры по manufacturer data / service UUID, а затем при необходимости перейти к индексированному dispatch. До появления такой необходимости текущий линейный вариант остаётся базовым.

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