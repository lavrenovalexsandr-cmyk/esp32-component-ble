# ESP32 Platform BLE

**A reusable BLE layer for ESP32 projects built with ESP-IDF.**

ESP32 Platform BLE is a standalone platform repository providing common BLE infrastructure for ESP32 firmware. It does not contain logic for a particular sensor, device family, or vendor.

## Purpose

- BLE initialization and lifecycle management.
- BLE advertising scan.
- A common platform representation of advertising reports.
- Registration of independent device drivers.
- Dispatching advertising reports to registered drivers.
- Delivering driver-defined events without imposing a measurement schema.

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
    ├── CMakeLists.txt
    └── include/
        ├── platform_ble.h
        ├── platform_ble_scan.h
        └── platform_ble_device.h

docs/
├── ARCHITECTURE_RU.md
└── API_DESIGN_RU.md
~~~

## Current status

**Initial API design stage.**

This stage defines responsibility boundaries and preliminary public contracts. NimBLE implementation, the driver registry, and a test application will be added in subsequent steps after the API is agreed upon. Header declarations do not mean that functions are already implemented or ready for linking.

## Documentation

- [Architecture](docs/ARCHITECTURE_RU.md)
- [Public API design](docs/API_DESIGN_RU.md)
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
