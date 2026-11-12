# ESP32 Platform BLE

**A reusable BLE layer for ESP32 projects built with ESP-IDF.**

ESP32 Platform BLE is a standalone platform repository providing common BLE infrastructure for ESP32 firmware. It does not contain logic for a particular sensor, device family, or vendor.

## Purpose

- NimBLE initialization and lifecycle management.
- BLE advertising scan.
- A common platform representation of advertising reports.
- An application callback for raw advertising reports.
- A generic device-driver registration contract (designed; implementation is planned for the next stage).
- Driver-defined events without imposing a measurement schema.

## Architecture

- **ESP32 Platform Core** — common device infrastructure such as Wi-Fi, MQTT, OTA, Home Assistant, and system services.
- **ESP32 Platform BLE** — BLE transport, scanning, packet dispatch, and the generic driver contract.
- **BLE device drivers** — protocols for specific devices and vendors. Xiaomi LYWSD03MMC will live in a separate project.
- **Firmware application** — selects drivers and connects them to MQTT, Home Assistant, or another application layer.

The BLE layer can be used without Xiaomi, MQTT, or Home Assistant.

## Repository layout

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

## Current status

**Stage 1 — preliminary NimBLE lifecycle and passive scanning implementation.**

NimBLE initialization/shutdown, host synchronization, scan start/stop, generic advertising report conversion, and the application callback are implemented. A minimal ESP-IDF test application for ESP32-C3 is included.

The implementation has not yet been built or validated on physical hardware. The driver registry and packet dispatch to registered drivers are the next stage. platform_ble_device.h currently documents the planned contract; its registration and event functions are not implemented yet.

## Build the test application

ESP-IDF 6.1 and an ESP32-C3 board are required.

~~~bash
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
~~~

See [test application instructions](docs/CORE_TEST_APPLICATION_RU.md).

## Documentation

- [Architecture](docs/ARCHITECTURE_RU.md)
- [Public API design](docs/API_DESIGN_RU.md)
- [Test application](docs/CORE_TEST_APPLICATION_RU.md)
- [Русский README](README.md)

## Related projects

- [ESP32 Platform Core](https://github.com/lavrenovalexsandr-cmyk/esp32-platform-core)
- [Legacy ESP32 Xiaomi LYWSD03MMC firmware](https://github.com/lavrenovalexsandr-cmyk/esp32_mqtt_xiaomi_lywsd03)

## Design principles

1. BLE Core must not depend on a particular model, vendor, MQTT, or Home Assistant.
2. Advertising reports remain generic; manufacturer and service data are not interpreted by Core.
3. A driver owns its protocol, state, keys, parsing, and event schema.
4. Core provides only common BLE types and event delivery mechanisms.
5. Public APIs should allow future active scanning, GATT, notifications, and bonding without breaking the basic contracts.
6. Each implementation stage is validated separately on real hardware.

## License

A license will be selected before the first public release.