#include "platform_ble.h"

#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

#include "platform_ble_scan.h"
#include "platform_ble_internal.h"

static const char *TAG = "platform_ble";

static bool s_initialized;
static bool s_host_synced;

static void platform_ble_host_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    ESP_LOGI(TAG, "NimBLE host task stopped");
    nimble_port_freertos_deinit();
}

static void platform_ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE host reset, reason=%d", reason);
    s_host_synced = false;
}

static void platform_ble_on_sync(void)
{
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Unable to infer BLE address type: %d", rc);
        return;
    }

    s_host_synced = true;
    ESP_LOGI(TAG, "NimBLE host synchronized");
    platform_ble_scan_on_sync();
}

esp_err_t platform_ble_init(const platform_ble_config_t *config)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_host_synced = false;
    platform_ble_scan_reset();

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.reset_cb = platform_ble_on_reset;
    ble_hs_cfg.sync_cb = platform_ble_on_sync;

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    if (config != NULL && config->device_name != NULL) {
        int rc = ble_svc_gap_device_name_set(config->device_name);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to set BLE device name: %d", rc);
            esp_err_t deinit_err = nimble_port_deinit();
            if (deinit_err != ESP_OK) {
                ESP_LOGE(TAG, "nimble_port_deinit after GAP name failure: %s",
                         esp_err_to_name(deinit_err));
            }
            return ESP_FAIL;
        }
    }
#else
    (void)config;
#endif

    s_initialized = true;
    s_host_synced = false;
    nimble_port_freertos_init(platform_ble_host_task);
    return ESP_OK;
}

esp_err_t platform_ble_deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t scan_err = platform_ble_scan_stop();
    if (scan_err != ESP_OK && scan_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Unable to stop BLE scan cleanly: %s",
                 esp_err_to_name(scan_err));
    }

    int rc = nimble_port_stop();
    if (rc != 0) {
        ESP_LOGE(TAG, "nimble_port_stop failed: %d", rc);
        return ESP_FAIL;
    }

    /*
     * nimble_port_stop() waits until the host stop procedure and its final
     * event have been processed. No more GAP callbacks can run after it
     * returns, so scan state can now be reset safely.
     */
    s_host_synced = false;
    platform_ble_scan_reset();

    esp_err_t deinit_err = nimble_port_deinit();
    if (deinit_err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_deinit failed: %s",
                 esp_err_to_name(deinit_err));
        return deinit_err;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "NimBLE stopped");
    return ESP_OK;
}

bool platform_ble_is_initialized(void)
{
    return s_initialized;
}

bool platform_ble_is_host_synced(void)
{
    return s_host_synced;
}
