#include "platform_ble_device.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static const char *TAG = "platform_ble_device";
static const platform_ble_device_driver_t *s_drivers[PLATFORM_BLE_DEVICE_MAX_DRIVERS];
static size_t s_driver_count;
static platform_ble_device_event_callback_t s_event_callback;
static void *s_event_context;
static portMUX_TYPE s_registry_lock = portMUX_INITIALIZER_UNLOCKED;

static bool driver_is_registered_locked(const char *driver_id)
{
    for (size_t i = 0; i < s_driver_count; ++i) {
        if (strcmp(s_drivers[i]->id, driver_id) == 0) return true;
    }
    return false;
}

esp_err_t platform_ble_device_set_event_callback(
    platform_ble_device_event_callback_t callback, void *context)
{
    portENTER_CRITICAL(&s_registry_lock);
    s_event_callback = callback;
    s_event_context = callback != NULL ? context : NULL;
    portEXIT_CRITICAL(&s_registry_lock);
    return ESP_OK;
}

esp_err_t platform_ble_device_register(const platform_ble_device_driver_t *driver)
{
    if (driver == NULL || driver->id == NULL || driver->id[0] == '\0' ||
        driver->matches == NULL || driver->on_advertisement == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&s_registry_lock);
    if (driver_is_registered_locked(driver->id)) {
        portEXIT_CRITICAL(&s_registry_lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (s_driver_count >= PLATFORM_BLE_DEVICE_MAX_DRIVERS) {
        portEXIT_CRITICAL(&s_registry_lock);
        return ESP_ERR_NO_MEM;
    }
    s_drivers[s_driver_count++] = driver;
    portEXIT_CRITICAL(&s_registry_lock);

    ESP_LOGI(TAG, "Registered BLE driver: %s", driver->id);
    return ESP_OK;
}

esp_err_t platform_ble_device_unregister(const char *driver_id)
{
    if (driver_id == NULL || driver_id[0] == '\0') return ESP_ERR_INVALID_ARG;

    portENTER_CRITICAL(&s_registry_lock);
    size_t index = 0;
    while (index < s_driver_count && strcmp(s_drivers[index]->id, driver_id) != 0) {
        ++index;
    }
    if (index == s_driver_count) {
        portEXIT_CRITICAL(&s_registry_lock);
        return ESP_ERR_NOT_FOUND;
    }
    for (size_t i = index; i + 1 < s_driver_count; ++i) {
        s_drivers[i] = s_drivers[i + 1];
    }
    s_drivers[--s_driver_count] = NULL;
    portEXIT_CRITICAL(&s_registry_lock);

    ESP_LOGI(TAG, "Unregistered BLE driver: %s", driver_id);
    return ESP_OK;
}

esp_err_t platform_ble_device_emit_event(
    const char *driver_id,
    const platform_ble_addr_t *address,
    uint32_t event_id,
    const void *payload,
    size_t payload_size)
{
    if (driver_id == NULL || driver_id[0] == '\0' || address == NULL ||
        (payload == NULL && payload_size != 0)) return ESP_ERR_INVALID_ARG;

    platform_ble_device_event_callback_t callback;
    void *context;
    portENTER_CRITICAL(&s_registry_lock);
    const char *registered_driver_id = NULL;
    for (size_t i = 0; i < s_driver_count; ++i) {
        if (strcmp(s_drivers[i]->id, driver_id) == 0) {
            registered_driver_id = s_drivers[i]->id;
            break;
        }
    }
    callback = s_event_callback;
    context = s_event_context;
    portEXIT_CRITICAL(&s_registry_lock);

    if (registered_driver_id == NULL) return ESP_ERR_NOT_FOUND;
    if (callback == NULL) return ESP_ERR_INVALID_STATE;

    const platform_ble_device_event_t event = {
        .driver_id = registered_driver_id,
        .address = *address,
        .event_id = event_id,
        .payload = payload,
        .payload_size = payload_size,
    };
    callback(&event, context);
    return ESP_OK;
}

void platform_ble_device_dispatch_packet(const platform_ble_adv_packet_t *packet)
{
    if (packet == NULL) return;

    const platform_ble_device_driver_t *snapshot[PLATFORM_BLE_DEVICE_MAX_DRIVERS];
    size_t count;
    portENTER_CRITICAL(&s_registry_lock);
    count = s_driver_count;
    for (size_t i = 0; i < count; ++i) snapshot[i] = s_drivers[i];
    portEXIT_CRITICAL(&s_registry_lock);

    for (size_t i = 0; i < count; ++i) {
        const platform_ble_device_driver_t *driver = snapshot[i];
        if (driver->matches(packet, driver->context)) {
            esp_err_t err = driver->on_advertisement(packet, driver->context);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Driver '%s' returned error: %s",
                         driver->id, esp_err_to_name(err));
            }
        }
    }
}
