#pragma once

#include "platform_ble_scan.h"

void platform_ble_scan_on_sync(void);
void platform_ble_device_dispatch_packet(
    const platform_ble_adv_packet_t *packet);
