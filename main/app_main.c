#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "platform_ble.h"
#include "platform_ble_device.h"
#include "platform_ble_scan.h"

static const char *TAG = "ble_test";
static uint32_t s_test_driver_packet_count;
static uint32_t s_registry_event_count;
static uint32_t s_nonmatching_callback_count;
static bool s_nonmatching_filter_verified;

static bool registry_test_matches(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)packet;
    (void)context;
    return true;
}

static esp_err_t registry_test_on_advertisement(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)packet;
    (void)context;
    return ESP_OK;
}

#define REGISTRY_TEST_DRIVER(n) { \
    .id = "registry_test_" #n, \
    .matches = registry_test_matches, \
    .on_advertisement = registry_test_on_advertisement, \
    .context = NULL, \
}

static const platform_ble_device_driver_t s_registry_test_drivers[] = {
    REGISTRY_TEST_DRIVER(0),
    REGISTRY_TEST_DRIVER(1),
    REGISTRY_TEST_DRIVER(2),
    REGISTRY_TEST_DRIVER(3),
    REGISTRY_TEST_DRIVER(4),
    REGISTRY_TEST_DRIVER(5),
    REGISTRY_TEST_DRIVER(6),
    REGISTRY_TEST_DRIVER(7),
};
static const platform_ble_device_driver_t s_registry_overflow_driver =
    REGISTRY_TEST_DRIVER(overflow);
static const platform_ble_device_driver_t s_invalid_registry_driver = {
    .id = "",
    .matches = registry_test_matches,
    .on_advertisement = registry_test_on_advertisement,
    .context = NULL,
};

static void registry_test_event_callback(
    const platform_ble_device_event_t *event,
    void *context)
{
    (void)context;

    if (event != NULL &&
        strcmp(event->driver_id, "registry_test_0") == 0 &&
        event->event_id == 0xA5 &&
        event->payload != NULL &&
        event->payload_size == sizeof(uint32_t) &&
        *(const uint32_t *)event->payload == 0x12345678U) {
        ++s_registry_event_count;
    }
}

static void run_registry_self_tests(void)
{
    ESP_LOGI(TAG, "Running BLE driver registry self-tests");

    ESP_ERROR_CHECK(platform_ble_device_register(NULL) == ESP_ERR_INVALID_ARG
                        ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(platform_ble_device_register(&s_invalid_registry_driver) ==
                            ESP_ERR_INVALID_ARG
                        ? ESP_OK : ESP_FAIL);

    for (size_t i = 0; i < PLATFORM_BLE_DEVICE_MAX_DRIVERS; ++i) {
        ESP_ERROR_CHECK(platform_ble_device_register(&s_registry_test_drivers[i]));
    }

    ESP_ERROR_CHECK(platform_ble_device_register(&s_registry_test_drivers[0]) ==
                            ESP_ERR_INVALID_STATE
                        ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(platform_ble_device_register(&s_registry_overflow_driver) ==
                            ESP_ERR_NO_MEM
                        ? ESP_OK : ESP_FAIL);

    const platform_ble_addr_t test_address = { .val = {0, 1, 2, 3, 4, 5} };
    const uint32_t test_payload = 0x12345678U;

    ESP_ERROR_CHECK(platform_ble_device_set_event_callback(
        registry_test_event_callback, NULL));
    ESP_ERROR_CHECK(platform_ble_device_emit_event(
        "registry_test_0", &test_address, 0xA5,
        &test_payload, sizeof(test_payload)));
    ESP_ERROR_CHECK(s_registry_event_count == 1 ? ESP_OK : ESP_FAIL);

    ESP_ERROR_CHECK(platform_ble_device_set_event_callback(NULL, NULL));
    ESP_ERROR_CHECK(platform_ble_device_emit_event(
        "registry_test_0", &test_address, 0xA5,
        &test_payload, sizeof(test_payload)) == ESP_ERR_INVALID_STATE
            ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(platform_ble_device_emit_event(
        "unknown_registry_driver", &test_address, 0xA5,
        &test_payload, sizeof(test_payload)) == ESP_ERR_NOT_FOUND
            ? ESP_OK : ESP_FAIL);

    ESP_ERROR_CHECK(platform_ble_device_unregister("registry_test_0"));
    ESP_ERROR_CHECK(platform_ble_device_unregister("registry_test_0") ==
                            ESP_ERR_NOT_FOUND
                        ? ESP_OK : ESP_FAIL);

    /* Removing one entry frees a slot; the previously rejected driver fits. */
    ESP_ERROR_CHECK(platform_ble_device_register(&s_registry_overflow_driver));

    for (size_t i = 1; i < PLATFORM_BLE_DEVICE_MAX_DRIVERS; ++i) {
        char driver_id[24];
        snprintf(driver_id, sizeof(driver_id), "registry_test_%u", (unsigned)i);
        ESP_ERROR_CHECK(platform_ble_device_unregister(driver_id));
    }
    ESP_ERROR_CHECK(platform_ble_device_unregister("registry_test_overflow"));

    ESP_LOGI(TAG, "BLE driver registry self-tests passed");
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

static bool nonmatching_driver_matches(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)packet;
    (void)context;
    return false;
}

static esp_err_t nonmatching_driver_on_advertisement(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)packet;
    (void)context;
    ++s_nonmatching_callback_count;
    return ESP_OK;
}

static const platform_ble_device_driver_t s_test_driver = {
    .id = "ble_test_driver",
    .matches = test_driver_matches,
    .on_advertisement = test_driver_on_advertisement,
    .context = NULL,
};

static const platform_ble_device_driver_t s_nonmatching_driver = {
    .id = "ble_test_nonmatching_driver",
    .matches = nonmatching_driver_matches,
    .on_advertisement = nonmatching_driver_on_advertisement,
    .context = NULL,
};

static void on_advertisement(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)context;

    if (!s_nonmatching_filter_verified) {
        if (s_nonmatching_callback_count == 0) {
            ESP_LOGI(TAG, "Non-matching driver filter verified");
            s_nonmatching_filter_verified = true;
        } else {
            ESP_LOGE(TAG, "Non-matching driver callback was unexpectedly called");
        }
    }

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

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 Platform BLE test");

    /* ESP-IDF NimBLE controller initialization requires NVS to be ready.
     * NVS ownership remains with the application, not platform_ble. */
    ESP_ERROR_CHECK(nvs_flash_init());

    run_registry_self_tests();

    ESP_ERROR_CHECK(platform_ble_scan_set_callback(on_advertisement, NULL));
    ESP_ERROR_CHECK(platform_ble_device_set_event_callback(on_driver_event, NULL));
    ESP_ERROR_CHECK(platform_ble_device_register(&s_test_driver));
    ESP_ERROR_CHECK(platform_ble_device_register(&s_nonmatching_driver));
    ESP_ERROR_CHECK(platform_ble_init(NULL));

    platform_ble_scan_config_t scan_config =
        PLATFORM_BLE_SCAN_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(platform_ble_scan_start(&scan_config));
    ESP_LOGI(TAG, "BLE scan requested");
}
