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

/** BLE subsystem configuration. */
typedef struct {
    bool release_classic_memory;
} platform_ble_config_t;

#define PLATFORM_BLE_DEFAULT_CONFIG() \
    ((platform_ble_config_t) {        \
        .release_classic_memory = true \
    })

/** Initialize the BLE subsystem. NULL selects default configuration. */
esp_err_t platform_ble_init(const platform_ble_config_t *config);

/** Stop BLE activity and release resources owned by this component. */
esp_err_t platform_ble_deinit(void);

#ifdef __cplusplus
}
#endif
