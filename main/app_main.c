#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "platform_ble.h"
#include "platform_ble_device.h"
#include "platform_ble_scan.h"

static const char *TAG = "ble_test";
static uint32_t s_test_driver_packet_count;

static void on_advertisement(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)context;

    ESP_LOGI(TAG,
             "ADV %02X:%02X:%02X:%02X:%02X:%02X RSSI=%d len=%u connectable=%u scan_rsp=%u",
             packet->address.val[5],
             packet->address.val[4],
             packet->address.val[3],
             packet->address.val[2],
             packet->address.val[1],
             packet->address.val[0],
             packet->rssi,
             (unsigned)packet->data_len,
             packet->connectable,
             packet->scan_response);
}

/* Test driver: accepts non-empty advertising reports and emits a counter event. */
static bool test_driver_matches(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)context;
    return packet != NULL && packet->data != NULL && packet->data_len > 0;
}

static esp_err_t test_driver_on_advertisement(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)context;
    ++s_test_driver_packet_count;

    if ((s_test_driver_packet_count % 100) == 0) {
        return platform_ble_device_emit_event(
            "ble_test_driver",
            &packet->address,
            1,
            &s_test_driver_packet_count,
            sizeof(s_test_driver_packet_count));
    }

    return ESP_OK;
}

static void on_driver_event(
    const platform_ble_device_event_t *event,
    void *context)
{
    (void)context;

    if (event == NULL || event->event_id != 1 ||
        event->payload == NULL ||
        event->payload_size != sizeof(uint32_t)) {
        ESP_LOGW(TAG, "Received unexpected test driver event");
        return;
    }

    const uint32_t count = *(const uint32_t *)event->payload;
    ESP_LOGI(TAG, "Driver event from %s: processed %lu advertising reports",
             event->driver_id, (unsigned long)count);
}

static const platform_ble_device_driver_t s_test_driver = {
    .id = "ble_test_driver",
    .matches = test_driver_matches,
    .on_advertisement = test_driver_on_advertisement,
    .context = NULL,
};

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 Platform BLE test");

    /* ESP-IDF NimBLE controller initialization requires NVS to be ready.
     * NVS ownership remains with the application, not platform_ble. */
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(platform_ble_scan_set_callback(on_advertisement, NULL));
    ESP_ERROR_CHECK(platform_ble_device_set_event_callback(on_driver_event, NULL));
    ESP_ERROR_CHECK(platform_ble_device_register(&s_test_driver));
    ESP_ERROR_CHECK(platform_ble_init(NULL));

    platform_ble_scan_config_t scan_config =
        PLATFORM_BLE_SCAN_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(platform_ble_scan_start(&scan_config));
    ESP_LOGI(TAG, "BLE scan requested");
}
