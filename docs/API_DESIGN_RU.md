# Проектирование публичного API ESP32 Platform BLE

## 1. Статус

Документ фиксирует публичный контракт первого этапа. Lifecycle, scan, driver registry и event API реализованы. Проект собран для ESP32-C3; на плате подтверждены NimBLE host sync, passive scan, приём advertising reports, обработка пакетов тестовым драйвером и доставка application event. В тестовое приложение добавлены startup self-tests duplicate ID, capacity/overflow, unregister, освобождения слота и event callback; live scan проверяет фильтрацию драйвера с matches() == false. Эти проверки успешно собраны и выполнены на ESP32-C3: startup self-tests прошли, live scan подтвердил фильтрацию несовпадающего драйвера и получение advertising reports. В тестовое приложение добавлены проверки явной остановки и повторного запуска scan, finite-duration passive scan, active scan и изоляции ошибки одного driver callback; новая версия ещё требует сборки и аппаратной проверки.

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

### 8.1 Рекомендации по стоимости callback

Каждый advertising report последовательно проверяется всеми зарегистрированными драйверами. При текущем лимите 8 драйверов это не более 8 вызовов matches() на один report. Например, 500 reports/s дают до 4000 вызовов matches()/s. Это расчёт количества вызовов, а не измерение времени CPU; реальная стоимость зависит от реализации callbacks и должна быть проверена на целевом ESP32-C3.

Для сохранения низкой нагрузки:

- matches() должен быть коротким фильтром по доступным полям пакета, без побочных эффектов;
- не выполнять в matches() и on_advertisement() криптографию, обращения к NVS, MQTT/HTTP, ожидания или другие длительные операции;
- если обработка сложная, передавать необходимые данные в собственную FreeRTOS queue/task;
- копировать payload перед постановкой в очередь, так как advertising payload не гарантирован после возврата из callback;
- не логировать каждый принятый пакет.

На текущем этапе сохраняется линейный dispatch. Предварительная фильтрация по общим признакам (например, manufacturer ID или service UUID) может быть рассмотрена позднее, а индексированный dispatch — только если измерения покажут, что текущая схема стала узким местом. Усложнять API до появления такого подтверждения не планируется.

## 9. Проверки перед фиксацией API

1. Сборка на ESP-IDF 6.1 для ESP32-C3 — пройдена после добавления registry.
2. Базовая аппаратная проверка NimBLE sync, passive scan, advertising reports, dispatch в тестовый драйвер и application event — пройдена.
3. Startup self-tests повторной регистрации ID, capacity/overflow, unregister, повторного unregister и повторного использования слота — пройдены на ESP32-C3.
4. Проверки event callback, отсутствующего callback и неизвестного driver ID — пройдены.
5. Live scan подтвердил фильтрацию драйвера с matches() == false и получение advertising reports.
6. Изоляция ошибки on_advertisement() одного драйвера — пройдена на ESP32-C3: первый тестовый драйвер один раз вернул ESP_FAIL, следующий продолжил обработку; итоговые счётчики обоих драйверов — 145.
7. Scan stop/restart и active scan с конечным duration — пройдены на ESP32-C3: passive scan остановлен вручную, active scan запущен повторно и автоматически завершился (reason=0; platform_ble_scan_is_active() == false).
8. Duplicate filtering, фактическое получение Scan Response, проверка адресации и порядка байтов BLE address, повторная инициализация и deinit — требуют отдельной проверки.