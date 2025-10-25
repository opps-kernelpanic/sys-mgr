/**
 * @file network.h
 *
 */

#ifndef G_NETWORK_H
#define G_NETWORK_H

#include <NetworkManager.h>

NMClient *get_nm_client();
int32_t network_manager_comm_init();
void network_manager_comm_deinit();

int32_t disconnect_interface(const char *exp_iface);

NMDevice * g_nm_device_get_by_iface(const char *exp_iface);

int32_t wifi_scan_and_get_results(const char *iface, int32_t scan);
int32_t wifi_connect_to_ssid(const char *iface, const char *ssid, \
                         const char *password);

#endif /* G_NETWORK_H */
