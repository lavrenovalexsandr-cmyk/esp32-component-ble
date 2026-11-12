#pragma once

void platform_ble_scan_on_sync(void);
void platform_ble_device_dispatch_packet(
    const struct platform_ble_adv_packet *packet);
