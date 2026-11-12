#include "esp_err.h"
#include "esp_log.h"

#include "platform_ble.h"
#include "platform_ble_scan.h"

static const char *TAG = "ble_test";

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

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 Platform BLE test");

    ESP_ERROR_CHECK(platform_ble_scan_set_callback(on_advertisement, NULL));
    ESP_ERROR_CHECK(platform_ble_init(NULL));

    platform_ble_scan_config_t scan_config =
        PLATFORM_BLE_SCAN_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(platform_ble_scan_start(&scan_config));
    ESP_LOGI(TAG, "BLE scan requested");
}
