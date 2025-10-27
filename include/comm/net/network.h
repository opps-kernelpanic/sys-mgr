/**
 * @file network.h
 *
 */

#ifndef G_NETWORK_H
#define G_NETWORK_H

/*********************
 *      INCLUDES
 *********************/
#include <NetworkManager.h>

/*********************
 *      DEFINES
 *********************/
#define NM_SSID_MAX_LEN                 33  /* IEEE 802.11 */
#define WIFI_STATE_WAIT_TIMEOUT_MS      10000  /* 10 seconds */
#define WIFI_STATE_POLL_INTERVAL_MS     200   /* 200ms */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  GLOBAL PROTOTYPES
 **********************/
int32_t init_network_manager_client();
void deinit_network_manager_client();
NMClient *get_nm_client();

NMDevice * get_nm_dev_by_iface(const char *iface);
int32_t disconnect_interface(const char *iface);

int32_t enable_wifi_device(void);
int32_t disable_wifi_device(void);
int32_t disconnect_wifi_device(void);
int32_t get_available_wifi_access_points(void);
int32_t request_wifi_rescan_access_point(void);

int32_t wifi_scan_and_get_results(const char *iface, int32_t scan);
int32_t wifi_connect_to_ssid(const char *iface, const char *ssid, \
                         const char *password);

#endif /* G_NETWORK_H */
