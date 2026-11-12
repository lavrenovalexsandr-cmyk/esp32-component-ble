# Проектирование публичного API ESP32 Platform BLE

## 1. Статус

Документ фиксирует публичный контракт первого этапа. Lifecycle, scan, driver registry и event API реализованы предварительно; registry и dispatch требуют повторной сборки и аппаратной проверки. До аппаратной проверки и обратной связи от тестового приложения API может уточняться.

## 2. Общие правила

- Публичные функции возвращают esp_err_t там, где возможна ошибка выполнения.
- Конфигурации передаются через структуры.
- Callback NimBLE host выполняются в контексте BLE host task. Они не должны блокироваться или выполнять длительные операции.
- Данные, передаваемые через указатели callback, действительны только в пределах callback и не должны сохраняться без копирования.
- Core не владеет памятью пользовательского driver payload.
- Публичные заголовки не раскрывают типы NimBLE.

## 3. Lifecycle — platform_ble.h

Реализованы:

- platform_ble_init(config) — инициализация NimBLE host и запуск host task;
- platform_ble_deinit() — остановка сканирования, остановка host task и освобождение NimBLE ресурсов;
- platform_ble_is_initialized() — состояние инициализации;
- platform_ble_is_host_synced() — состояние синхронизации NimBLE host.

В конфигурации можно передать device_name для установки GAP device name, если соответствующая GAP service включена в конфигурации ESP-IDF. NULL использует настройки по умолчанию.

Приложение должно инициализировать NVS самостоятельно, если оно использует NVS для других подсистем или в дальнейшем подключит BLE bonding/storage. BLE Core не форматирует и не стирает NVS.

## 4. Scan — platform_ble_scan.h

Реализованы:

- platform_ble_scan_set_callback() — установить callback необработанных рекламных пакетов;
- platform_ble_scan_start(config) — запустить сканирование;
- platform_ble_scan_stop() — остановить сканирование;
- platform_ble_scan_is_active() — получить состояние сканирования.

Scan config предусматривает active/passive scan, interval и window в единицах BLE 0.625 ms, duration в миллисекундах (0 означает без ограничения по времени), duplicate filtering. По умолчанию используется passive scan, interval 60 ms, window 30 ms и duplicate filtering выключен: для датчиков важно получать повторяющиеся рекламные пакеты, даже если их содержимое не изменилось.

Если scan запрошен до синхронизации NimBLE host, запрос сохраняется и запускается после sync callback. Параметры проверяются до запуска: interval/window должны быть ненулевыми, window не может превышать interval.

## 5. Advertising report

platform_ble_adv_packet_t содержит BLE address, address type, RSSI, указатель и длину advertising payload, connectable и scan_response.

Payload не копируется Core и валиден только на время callback. Core не разбирает AD structures и не интерпретирует manufacturer/service data.

## 6. Generic driver API — platform_ble_device.h

Descriptor драйвера содержит стабильный строковый id, callback matches для определения интереса к пакету, callback on_advertisement для протокольной обработки и пользовательский context.

Registry предоставляет регистрацию и снятие регистрации драйвера. Registry поддерживает до 8 драйверов. Descriptor не копируется; descriptor и context должны иметь статическое время жизни. Снятие регистрации исключает драйвер из новых snapshots, но уже начавшаяся диспетчеризация может вызвать его callback ещё один раз. Не освобождайте память descriptor/context после unregister. Регистрация с пустым ID или отсутствующими callbacks возвращает ESP_ERR_INVALID_ARG; повторный ID — ESP_ERR_INVALID_STATE; переполнение — ESP_ERR_NO_MEM. Снятие отсутствующего ID возвращает ESP_ERR_NOT_FOUND.

## 7. Driver events

platform_ble_device_event_t — транспортная оболочка, а не схема измерений. Она содержит driver_id, BLE address, event_id, определяемый драйвером, payload и payload_size.

Драйвер публикует событие через platform_ble_device_emit_event(). Приложение устанавливает общий callback приёма событий. Callback события общий для всех драйверов. События принимаются только от зарегистрированного driver_id. Если callback приложения не установлен, emit_event() возвращает ESP_ERR_INVALID_STATE. Payload не копируется и действителен до возврата из event callback.

Payload может указывать на стековую или временную память драйвера и гарантированно валиден только на время callback. Если приложению нужно отложить обработку, оно копирует payload в собственную очередь/структуру.

## 8. Потоки выполнения

Dispatch драйверов, scan callback и event callback выполняются в контексте NimBLE host task. Никакие длительные операции, сетевые публикации или блокирующие ожидания не должны выполняться внутри этого контекста. При необходимости приложение переносит обработку в собственную FreeRTOS queue/task.

## 9. Проверки перед фиксацией API

1. Сборка на ESP-IDF 6.1 для ESP32-C3 после добавления registry.
2. Регистрация уникального драйвера, повторная регистрация ID, снятие регистрации и проверка лимита в 8 драйверов.
3. Проверка matches()/on_advertisement() и доставки event callback.
4. Проверка старта и остановки scan.
5. Проверка конечного duration и непрерывного scan.
6. Проверка active/passive режима и duplicate filtering.
7. Проверка адресации и порядка байтов BLE address.
8. Проверка повторной инициализации и deinit.