/**
 * @file network.h
 *
 */

#ifndef G_NETWORK_H
#define G_NETWORK_H

#include <NetworkManager.h>

int network_manager_comm_init();
void network_manager_comm_deinit();

int disconnect_interface(const char *exp_iface);

NMDevice * g_nm_device_get_by_iface(const char *exp_iface);

int wifi_scan_and_get_results(const char *iface, int scan);

#endif /* G_NETWORK_H */
