#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "platform_ble.h"
#include "platform_ble_device.h"
#include "platform_ble_scan.h"

static const char *TAG = "ble_test";
static uint32_t s_test_driver_packet_count;
static uint32_t s_registry_event_count;
static uint32_t s_nonmatching_callback_count;
static bool s_nonmatching_filter_verified;
static uint32_t s_after_error_driver_count;
static uint32_t s_failing_driver_call_count;

typedef enum {
    SCAN_TEST_PHASE_NONE = 0,
    SCAN_TEST_PHASE_DUPLICATES_OFF,
    SCAN_TEST_PHASE_DUPLICATES_ON,
    SCAN_TEST_PHASE_ACTIVE_SCAN,
} scan_test_phase_t;

#define SCAN_TEST_UNIQUE_MAX 128

typedef struct {
    platform_ble_addr_t address;
    platform_ble_addr_type_t address_type;
    uint32_t payload_hash;
    size_t payload_len;
} scan_test_signature_t;

typedef struct {
    uint32_t reports;
    uint32_t scan_responses;
    uint32_t unique_signatures;
    uint32_t duplicate_signatures;
} scan_test_stats_t;

static volatile scan_test_phase_t s_scan_test_phase;
static scan_test_stats_t s_scan_test_stats;
static scan_test_signature_t s_scan_test_signatures[SCAN_TEST_UNIQUE_MAX];

static void scan_test_begin(scan_test_phase_t phase)
{
    memset(&s_scan_test_stats, 0, sizeof(s_scan_test_stats));
    memset(s_scan_test_signatures, 0, sizeof(s_scan_test_signatures));
    s_scan_test_phase = phase;
}

static uint32_t scan_test_payload_hash(const uint8_t *data, size_t length)
{
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < length; ++i) {
        hash = (hash ^ data[i]) * 16777619U;
    }
    return hash;
}

static void scan_test_record_packet(const platform_ble_adv_packet_t *packet)
{
    if (s_scan_test_phase == SCAN_TEST_PHASE_NONE) {
        return;
    }

    ++s_scan_test_stats.reports;
    if (packet->scan_response) {
        ++s_scan_test_stats.scan_responses;
    }

    uint32_t hash = scan_test_payload_hash(packet->data, packet->data_len);
    for (size_t i = 0; i < s_scan_test_stats.unique_signatures; ++i) {
        const scan_test_signature_t *signature = &s_scan_test_signatures[i];
        if (signature->address_type == packet->address_type &&
            memcmp(signature->address.val, packet->address.val,
                   sizeof(packet->address.val)) == 0 &&
            signature->payload_hash == hash &&
            signature->payload_len == packet->data_len) {
            ++s_scan_test_stats.duplicate_signatures;
            return;
        }
    }

    if (s_scan_test_stats.unique_signatures < SCAN_TEST_UNIQUE_MAX) {
        scan_test_signature_t *signature =
            &s_scan_test_signatures[s_scan_test_stats.unique_signatures++];
        signature->address = packet->address;
        signature->address_type = packet->address_type;
        signature->payload_hash = hash;
        signature->payload_len = packet->data_len;
    }
}

static void scan_test_log_stats(const char *label)
{
    ESP_LOGI(TAG,
             "%s: reports=%lu unique=%lu repeated=%lu scan_responses=%lu",
             label,
             (unsigned long)s_scan_test_stats.reports,
             (unsigned long)s_scan_test_stats.unique_signatures,
             (unsigned long)s_scan_test_stats.duplicate_signatures,
             (unsigned long)s_scan_test_stats.scan_responses);
}


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

    static const uint8_t expected_address[6] = {0, 1, 2, 3, 4, 5};

    if (event != NULL &&
        strcmp(event->driver_id, "registry_test_0") == 0 &&
        memcmp(event->address.val, expected_address,
               sizeof(expected_address)) == 0 &&
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

    char formatted_address[18];
    snprintf(formatted_address, sizeof(formatted_address),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             test_address.val[5], test_address.val[4],
             test_address.val[3], test_address.val[2],
             test_address.val[1], test_address.val[0]);
    ESP_ERROR_CHECK(strcmp(formatted_address, "05:04:03:02:01:00") == 0
                        ? ESP_OK : ESP_FAIL);
    ESP_LOGI(TAG, "BLE address byte order test passed: %s", formatted_address);

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

static bool s_failing_driver_failed_once;

static esp_err_t failing_driver_on_advertisement(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)packet;
    bool *failed_once = context;
    ++s_failing_driver_call_count;
    if (!*failed_once) {
        *failed_once = true;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static const platform_ble_device_driver_t s_failing_driver = {
    .id = "ble_test_failing_driver",
    .matches = test_driver_matches,
    .on_advertisement = failing_driver_on_advertisement,
    .context = &s_failing_driver_failed_once,
};

static esp_err_t after_error_driver_on_advertisement(
    const platform_ble_adv_packet_t *packet,
    void *context)
{
    (void)packet;
    (void)context;
    ++s_after_error_driver_count;
    return ESP_OK;
}

static const platform_ble_device_driver_t s_after_error_driver = {
    .id = "ble_test_after_error_driver",
    .matches = test_driver_matches,
    .on_advertisement = after_error_driver_on_advertisement,
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
    scan_test_record_packet(packet);

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

static esp_err_t wait_for_host_sync(uint32_t timeout_ms)
{
    const TickType_t poll_interval = pdMS_TO_TICKS(50);
    uint32_t elapsed_ms = 0;

    while (elapsed_ms < timeout_ms) {
        if (platform_ble_is_host_synced()) {
            return ESP_OK;
        }
        vTaskDelay(poll_interval);
        elapsed_ms += 50;
    }

    return platform_ble_is_host_synced() ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void run_ble_reinit_lifecycle_tests(void)
{
    ESP_LOGI(TAG, "BLE lifecycle: stopping initial BLE instance");

    ESP_ERROR_CHECK(platform_ble_deinit());
    ESP_ERROR_CHECK(!platform_ble_is_initialized() &&
                    !platform_ble_is_host_synced() &&
                    !platform_ble_scan_is_active()
                        ? ESP_OK : ESP_FAIL);

    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "BLE lifecycle: initializing second BLE instance");
    ESP_ERROR_CHECK(platform_ble_init(NULL));
    ESP_ERROR_CHECK(wait_for_host_sync(5000));

    uint32_t packets_before = s_test_driver_packet_count;
    platform_ble_scan_config_t scan_config =
        PLATFORM_BLE_SCAN_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(platform_ble_scan_start(&scan_config));
    ESP_LOGI(TAG, "BLE lifecycle: continuous scan started");
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_ERROR_CHECK(platform_ble_scan_is_active() ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(platform_ble_scan_stop());
    ESP_LOGI(TAG, "BLE lifecycle: continuous scan stop requested");
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_ERROR_CHECK(!platform_ble_scan_is_active() ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(s_test_driver_packet_count > packets_before
                        ? ESP_OK : ESP_FAIL);

    ESP_LOGI(TAG, "BLE lifecycle: deinitializing second BLE instance");
    ESP_ERROR_CHECK(platform_ble_deinit());
    ESP_ERROR_CHECK(!platform_ble_is_initialized() &&
                    !platform_ble_is_host_synced() &&
                    !platform_ble_scan_is_active()
                        ? ESP_OK : ESP_FAIL);

    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "BLE lifecycle: initializing third BLE instance");
    ESP_ERROR_CHECK(platform_ble_init(NULL));
    ESP_ERROR_CHECK(wait_for_host_sync(5000));

    packets_before = s_test_driver_packet_count;
    scan_config.duration_ms = 3000;
    ESP_ERROR_CHECK(platform_ble_scan_start(&scan_config));
    ESP_LOGI(TAG, "BLE lifecycle: finite scan started (3 seconds)");
    vTaskDelay(pdMS_TO_TICKS(3500));

    ESP_ERROR_CHECK(!platform_ble_scan_is_active() ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(s_test_driver_packet_count > packets_before
                        ? ESP_OK : ESP_FAIL);

    ESP_LOGI(TAG, "BLE lifecycle: deinitializing third BLE instance");
    ESP_ERROR_CHECK(platform_ble_deinit());
    ESP_ERROR_CHECK(!platform_ble_is_initialized() &&
                    !platform_ble_is_host_synced() &&
                    !platform_ble_scan_is_active()
                        ? ESP_OK : ESP_FAIL);

    ESP_LOGI(TAG, "BLE init/deinit lifecycle tests passed");
}

static void scan_lifecycle_test_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(2000));

    if (platform_ble_scan_is_active()) {
        ESP_ERROR_CHECK(platform_ble_scan_stop());
        ESP_LOGI(TAG, "Scan lifecycle: explicit stop requested");
    } else {
        ESP_LOGE(TAG, "Scan lifecycle: scan was not active before stop");
    }

    /* Allow NimBLE to deliver DISC_COMPLETE before starting a new procedure. */
    vTaskDelay(pdMS_TO_TICKS(500));

    platform_ble_scan_config_t active_config =
        PLATFORM_BLE_SCAN_DEFAULT_CONFIG();
    active_config.active = true;
    active_config.duration_ms = 3000;

    ESP_ERROR_CHECK(platform_ble_scan_start(&active_config));
    ESP_LOGI(TAG, "Scan lifecycle: active scan restarted (3 seconds)");

    vTaskDelay(pdMS_TO_TICKS(3500));
    ESP_LOGI(TAG, "Scan lifecycle: finite active scan complete, active=%u",
             platform_ble_scan_is_active());
    ESP_LOGI(TAG, "Driver error isolation: failing=%lu, following=%lu",
             (unsigned long)s_failing_driver_call_count,
             (unsigned long)s_after_error_driver_count);
    ESP_ERROR_CHECK(s_failing_driver_call_count > 0 &&
                    s_after_error_driver_count > 0 ? ESP_OK : ESP_FAIL);

    /* Compare identical passive-scan windows with controller duplicate
     * filtering disabled and enabled. Results are diagnostic: nearby
     * advertisers and changing payloads affect the observed counts. */
    platform_ble_scan_config_t duplicate_config =
        PLATFORM_BLE_SCAN_DEFAULT_CONFIG();
    duplicate_config.duration_ms = 6000;
    duplicate_config.filter_duplicates = false;
    scan_test_begin(SCAN_TEST_PHASE_DUPLICATES_OFF);
    ESP_ERROR_CHECK(platform_ble_scan_start(&duplicate_config));
    ESP_LOGI(TAG, "Duplicate filter test: passive scan, filtering disabled");
    vTaskDelay(pdMS_TO_TICKS(6500));
    scan_test_log_stats("Duplicate filter disabled");
    ESP_ERROR_CHECK(!platform_ble_scan_is_active() ? ESP_OK : ESP_FAIL);

    vTaskDelay(pdMS_TO_TICKS(500));
    duplicate_config.filter_duplicates = true;
    scan_test_begin(SCAN_TEST_PHASE_DUPLICATES_ON);
    ESP_ERROR_CHECK(platform_ble_scan_start(&duplicate_config));
    ESP_LOGI(TAG, "Duplicate filter test: passive scan, filtering enabled");
    vTaskDelay(pdMS_TO_TICKS(6500));
    scan_test_log_stats("Duplicate filter enabled");
    ESP_ERROR_CHECK(!platform_ble_scan_is_active() ? ESP_OK : ESP_FAIL);

    /* Active scan can only produce Scan Response reports from scannable
     * advertisers in range. The log reports the observed count. */
    vTaskDelay(pdMS_TO_TICKS(500));
    platform_ble_scan_config_t response_config =
        PLATFORM_BLE_SCAN_DEFAULT_CONFIG();
    response_config.active = true;
    response_config.duration_ms = 10000;
    scan_test_begin(SCAN_TEST_PHASE_ACTIVE_SCAN);
    ESP_ERROR_CHECK(platform_ble_scan_start(&response_config));
    ESP_LOGI(TAG, "Scan Response test: active scan for 10 seconds");
    vTaskDelay(pdMS_TO_TICKS(10500));
    scan_test_log_stats("Active scan / Scan Response");
    ESP_ERROR_CHECK(!platform_ble_scan_is_active() ? ESP_OK : ESP_FAIL);
    s_scan_test_phase = SCAN_TEST_PHASE_NONE;

    ESP_LOGI(TAG, "BLE scan tests finished");

    run_ble_reinit_lifecycle_tests();
    vTaskDelete(NULL);
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
    ESP_ERROR_CHECK(platform_ble_device_register(&s_failing_driver));
    ESP_ERROR_CHECK(platform_ble_device_register(&s_after_error_driver));
    ESP_ERROR_CHECK(platform_ble_init(NULL));

    platform_ble_scan_config_t scan_config =
        PLATFORM_BLE_SCAN_DEFAULT_CONFIG();
    scan_config.duration_ms = 10000;

    ESP_ERROR_CHECK(platform_ble_scan_start(&scan_config));
    ESP_LOGI(TAG, "BLE scan requested (passive, 10 seconds)");

    BaseType_t task_result = xTaskCreate(
        scan_lifecycle_test_task, "ble_scan_test", 3072, NULL, 5, NULL);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_FAIL);
}
