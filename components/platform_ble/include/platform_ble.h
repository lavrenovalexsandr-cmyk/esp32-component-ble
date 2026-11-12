#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Generic BLE address type. Does not expose NimBLE types. */
typedef enum {
    PLATFORM_BLE_ADDR_PUBLIC = 0,
    PLATFORM_BLE_ADDR_RANDOM = 1,
    PLATFORM_BLE_ADDR_PUBLIC_ID = 2,
    PLATFORM_BLE_ADDR_RANDOM_ID = 3,
    PLATFORM_BLE_ADDR_ANONYMOUS = 4,
} platform_ble_addr_type_t;

/** BLE address in the byte order supplied by the platform API. */
typedef struct {
    uint8_t val[6];
} platform_ble_addr_t;

/** BLE subsystem configuration. NULL device_name leaves the GAP name unchanged. */
typedef struct {
    const char *device_name;
} platform_ble_config_t;

#define PLATFORM_BLE_DEFAULT_CONFIG() \
    ((platform_ble_config_t) { .device_name = NULL })

/** Initialize the BLE subsystem. NULL selects default configuration. */
esp_err_t platform_ble_init(const platform_ble_config_t *config);

/** Stop BLE activity and release resources owned by this component. */
esp_err_t platform_ble_deinit(void);

/** Return whether the BLE subsystem has been initialized. */
bool platform_ble_is_initialized(void);

/** Return whether the NimBLE host has completed synchronization. */
bool platform_ble_is_host_synced(void);

#ifdef __cplusplus
}
#endif
