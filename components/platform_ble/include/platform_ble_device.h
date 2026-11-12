#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "platform_ble_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of drivers that can be registered simultaneously. */
#define PLATFORM_BLE_DEVICE_MAX_DRIVERS 8

/** Opaque driver event; payload is valid only during the event callback. */
typedef struct {
    const char *driver_id;
    platform_ble_addr_t address;
    uint32_t event_id;
    const void *payload;
    size_t payload_size;
} platform_ble_device_event_t;

/** Runs in BLE host callback context; do not block. */
typedef void (*platform_ble_device_event_callback_t)(
    const platform_ble_device_event_t *event, void *context);

/**
 * Descriptor and context should have static lifetime. Unregistration removes
 * the driver from future dispatch snapshots; a dispatch already in progress
 * may still invoke its callbacks once. Do not free descriptor/context memory.
 * matches() should be a fast, side-effect-free filter.
 * on_advertisement() is called only when matches() returns true.
 */
typedef struct {
    const char *id;
    bool (*matches)(const platform_ble_adv_packet_t *packet, void *context);
    esp_err_t (*on_advertisement)(const platform_ble_adv_packet_t *packet, void *context);
    void *context;
} platform_ble_device_driver_t;

esp_err_t platform_ble_device_set_event_callback(
    platform_ble_device_event_callback_t callback, void *context);
esp_err_t platform_ble_device_register(const platform_ble_device_driver_t *driver);
esp_err_t platform_ble_device_unregister(const char *driver_id);

/** Payload is not copied and must remain valid until the event callback returns. */
esp_err_t platform_ble_device_emit_event(
    const char *driver_id,
    const platform_ble_addr_t *address,
    uint32_t event_id,
    const void *payload,
    size_t payload_size);

#ifdef __cplusplus
}
#endif
