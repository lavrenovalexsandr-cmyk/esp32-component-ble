#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "platform_ble_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque driver event envelope. event_id and payload schema belong to
 * the driver. payload is valid only during the event callback.
 */
typedef struct {
    const char *driver_id;
    platform_ble_addr_t address;
    uint32_t event_id;
    const void *payload;
    size_t payload_size;
} platform_ble_device_event_t;

typedef void (*platform_ble_device_event_callback_t)(
    const platform_ble_device_event_t *event,
    void *context);

/**
 * Driver descriptor. Descriptor and context must remain valid until
 * unregistration. Callbacks execute in BLE host callback context.
 */
typedef struct {
    const char *id;
    bool (*matches)(
        const platform_ble_adv_packet_t *packet,
        void *context);
    esp_err_t (*on_advertisement)(
        const platform_ble_adv_packet_t *packet,
        void *context);
    void *context;
} platform_ble_device_driver_t;

/** Set or clear the application event callback for all drivers. */
esp_err_t platform_ble_device_set_event_callback(
    platform_ble_device_event_callback_t callback,
    void *context);

/** Register a driver. Driver IDs must be unique; descriptor is not copied. */
esp_err_t platform_ble_device_register(
    const platform_ble_device_driver_t *driver);

/** Unregister a previously registered driver by ID. */
esp_err_t platform_ble_device_unregister(const char *driver_id);

/**
 * Emit an application event from a registered driver.
 * Payload is not copied and must remain valid until callback returns.
 */
esp_err_t platform_ble_device_emit_event(
    const char *driver_id,
    const platform_ble_addr_t *address,
    uint32_t event_id,
    const void *payload,
    size_t payload_size);

#ifdef __cplusplus
}
#endif
