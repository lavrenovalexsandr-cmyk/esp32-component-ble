#include "platform_ble_scan.h"

#include <limits.h>
#include <string.h>

#include "esp_log.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"

static const char *TAG = "platform_ble_scan";

static platform_ble_scan_callback_t s_scan_callback;
static void *s_scan_context;
static bool s_scan_requested;
static bool s_scan_active;

extern bool platform_ble_is_host_synced(void);

static platform_ble_addr_type_t map_address_type(uint8_t type)
{
    switch (type) {
    case BLE_ADDR_PUBLIC:
        return PLATFORM_BLE_ADDR_PUBLIC;
    case BLE_ADDR_RANDOM:
        return PLATFORM_BLE_ADDR_RANDOM;
    default:
        return PLATFORM_BLE_ADDR_ANONYMOUS;
    }
}

static bool is_connectable_event(uint8_t event_type)
{
    return event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
           event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND ||
           event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_IND;
}

static int platform_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        platform_ble_adv_packet_t packet = {
            .address_type = map_address_type(event->disc.addr.type),
            .rssi = event->disc.rssi,
            .data = event->disc.data,
            .data_len = event->disc.length_data,
            .connectable = is_connectable_event(event->disc.event_type),
            .scan_response =
                event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP,
        };
        memcpy(packet.address.val, event->disc.addr.val,
               sizeof(packet.address.val));

        if (s_scan_callback != NULL) {
            s_scan_callback(&packet, s_scan_context);
        }
        return 0;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE:
        s_scan_active = false;
        s_scan_requested = false;
        ESP_LOGI(TAG, "BLE scan completed, reason=%d",
                 event->disc_complete.reason);
        return 0;

    default:
        return 0;
    }
}

static esp_err_t start_scan_internal(const platform_ble_scan_config_t *config)
{
    struct ble_gap_disc_params params = {0};
    uint8_t own_addr_type;
    int32_t duration;
    int rc;

    if (!platform_ble_is_host_synced()) {
        s_scan_requested = true;
        return ESP_OK;
    }

    if (s_scan_active) {
        return ESP_ERR_INVALID_STATE;
    }

    platform_ble_scan_config_t cfg =
        config != NULL ? *config : (platform_ble_scan_config_t)
            PLATFORM_BLE_SCAN_DEFAULT_CONFIG();

    if (cfg.interval == 0 || cfg.window == 0 ||
        cfg.window > cfg.interval) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg.duration_ms > INT32_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Unable to infer own address type: %d", rc);
        return ESP_FAIL;
    }

    params.passive = cfg.active ? 0 : 1;
    params.itvl = cfg.interval;
    params.window = cfg.window;
    params.filter_duplicates = cfg.filter_duplicates ? 1 : 0;
    params.filter_policy = BLE_HS_SCAN_FILT_NO_WL;
    params.limited = 0;

    duration = cfg.duration_ms == 0 ? BLE_HS_FOREVER : (int32_t)cfg.duration_ms;
    rc = ble_gap_disc(own_addr_type, duration, &params,
                      platform_ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_disc failed: %d", rc);
        s_scan_requested = false;
        return ESP_FAIL;
    }

    s_scan_active = true;
    s_scan_requested = false;
    ESP_LOGI(TAG, "BLE scan started (%s)",
             cfg.active ? "active" : "passive");
    return ESP_OK;
}

void platform_ble_scan_on_sync(void)
{
    if (s_scan_requested && !s_scan_active) {
        platform_ble_scan_config_t config =
            PLATFORM_BLE_SCAN_DEFAULT_CONFIG();
        (void)start_scan_internal(&config);
    }
}

esp_err_t platform_ble_scan_set_callback(
    platform_ble_scan_callback_t callback,
    void *context)
{
    s_scan_callback = callback;
    s_scan_context = context;
    return ESP_OK;
}

esp_err_t platform_ble_scan_start(const platform_ble_scan_config_t *config)
{
    if (s_scan_active || s_scan_requested) {
        return ESP_ERR_INVALID_STATE;
    }

    if (config != NULL) {
        platform_ble_scan_config_t cfg = *config;
        if (cfg.interval == 0 || cfg.window == 0 ||
            cfg.window > cfg.interval || cfg.duration_ms > INT32_MAX) {
            return ESP_ERR_INVALID_ARG;
        }
        if (!platform_ble_is_host_synced()) {
            /* A scan requested before host synchronization uses the supplied
             * configuration once the host becomes ready. */
            s_pending_config = cfg;
            s_has_pending_config = true;
            s_scan_requested = true;
            return ESP_OK;
        }
    }

    return start_scan_internal(config);
}

esp_err_t platform_ble_scan_stop(void)
{
    if (s_scan_requested && !s_scan_active) {
        s_scan_requested = false;
        s_has_pending_config = false;
        return ESP_OK;
    }

    if (!s_scan_active) {
        return ESP_ERR_INVALID_STATE;
    }

    int rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_disc_cancel failed: %d", rc);
        return ESP_FAIL;
    }

    s_scan_active = false;
    s_scan_requested = false;
    return ESP_OK;
}

bool platform_ble_scan_is_active(void)
{
    return s_scan_active;
}
