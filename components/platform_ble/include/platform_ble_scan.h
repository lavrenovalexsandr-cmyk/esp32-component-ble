#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "platform_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generic BLE advertising report.
 * data is a temporary buffer valid only during the callback.
 */
typedef struct {
    platform_ble_addr_t address;
    platform_ble_addr_type_t address_type;
    int8_t rssi;
    const uint8_t *data;
    size_t data_len;
    bool connectable;
    bool scan_response;
} platform_ble_adv_packet_t;

/**
 * Scan parameters. interval/window use BLE units of 0.625 ms.
 * duration_ms == 0 means scan until explicitly stopped.
 */
typedef struct {
    bool active;
    uint16_t interval;
    uint16_t window;
    uint32_t duration_ms;
    bool filter_duplicates;
} platform_ble_scan_config_t;

#define PLATFORM_BLE_SCAN_DEFAULT_CONFIG() \
    ((platform_ble_scan_config_t) {        \
        .active = false,                   \
        .interval = 0x0060,                \
        .window = 0x0030,                  \
        .duration_ms = 0,                  \
        .filter_duplicates = true          \
    })

/** Runs in BLE host callback context; do not block. */
typedef void (*platform_ble_scan_callback_t)(
    const platform_ble_adv_packet_t *packet,
    void *context);

/** Set or clear the application callback for raw advertising reports. */
esp_err_t platform_ble_scan_set_callback(
    platform_ble_scan_callback_t callback,
    void *context);

/** Start scanning. NULL selects default parameters. */
esp_err_t platform_ble_scan_start(const platform_ble_scan_config_t *config);

/** Stop scanning. */
esp_err_t platform_ble_scan_stop(void);

/** Return whether scanning is currently active. */
bool platform_ble_scan_is_active(void);

#ifdef __cplusplus
}
#endif
