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

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    char ssid[NM_SSID_MAX_LEN];
    char bssid[18];
    uint32_t freq_mhz;
    uint32_t bitrate_mbps;
    uint32_t bandwidth_mhz;
    uint8_t strength;
    uint32_t wpa_flags;
    uint32_t rsn_flags;
    NM80211Mode mode;
} ap_info_t;

/**********************
 *      MACROS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  GLOBAL PROTOTYPES
 **********************/
NMClient *get_nm_client();
void set_nm_client(NMClient *client);

const char *nm_state_to_str(NMState state);
void print_nm_state(NMState state);
int32_t init_network_manager_client();
void deinit_network_manager_client();
int32_t disconnect_interface(const char *iface);
NMDevice * get_nm_dev_by_iface(const char *iface);

const char *nm_device_type_to_str(NMDeviceType type);
const char *nm_device_state_to_str(NMDeviceState state);
const char *nm_device_state_desc(NMDeviceState state);
const char *nm_device_state_reason_to_str(NMDeviceStateReason reason);
void log_device_ip_info(NMDevice *device);
void handle_nm_device_state(NMDevice *device);

int32_t enable_wifi_device(void);
int32_t disable_wifi_device(void);
int32_t disconnect_wifi_device(void);
int32_t get_available_wifi_access_points(void);
int32_t request_wifi_rescan_access_point(void);
int32_t get_ap_info_from_nm_ap(NMAccessPoint *ap, ap_info_t *info);

int32_t wifi_scan_and_get_results(const char *iface, int32_t scan);
int32_t wifi_connect_to_ssid(const char *iface, const char *ssid, \
                         const char *password);

#endif /* G_NETWORK_H */
