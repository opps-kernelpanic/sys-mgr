/**
 * @file network.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#define LOG_LEVEL LOG_LEVEL_TRACE
#if defined(LOG_LEVEL)
#warning "LOG_LEVEL defined locally will override the global setting in this file"
#endif
#include "log.h"

#include <stdio.h>
#include <errno.h>
#include <glib.h>
#include <NetworkManager.h>

#include "main.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void disconnect_interface_cb(GObject *source_obj, GAsyncResult *res, \
                                    gpointer user_data)
{
    NMDevice *dev;
    g_autoptr(GError) error = NULL;

    dev = NM_DEVICE(source_obj);

    if (!nm_device_disconnect_finish(dev, res, &error)) {
        LOG_ERROR("Async disconnect failed: %s", error->message);
        return;
    }

    LOG_INFO("Interface %s disconnected successfully", \
             nm_device_get_iface(dev));
}

static void network_state_changed_cb(GObject *object, GParamSpec *pspec, \
                                     gpointer user_data)
{
    NMClient *client;
    NMState state;

    client = NM_CLIENT(object);
    if (!client)
        return;

    print_nm_state(nm_client_get_state(client));
}

static void device_state_changed_cb(NMDevice *device,
                                    NMDeviceState new_state,
                                    NMDeviceState old_state,
                                    NMDeviceStateReason reason,
                                    gpointer user_data)
{
    const char *iface;

    iface = nm_device_get_iface(device);
    LOG_DEBUG("Device [%s] state changed: %s -> %s (reason: %d)",
              iface,
              nm_state_to_str(old_state),
              nm_state_to_str(new_state),
              reason);

    handle_nm_device_state(device);
}

static int32_t init_nm_device_callback(NMClient *client)
{
    const GPtrArray *devices;

    if (!client)
        return -EINVAL;

    devices = nm_client_get_devices(client);
    if (!devices)
        return -EIO;

    for (int32_t i = 0; i < devices->len; i++) {
        NMDevice *dev = g_ptr_array_index(devices, i);
        // TODO: store and handle watch ID from signal connect
        g_signal_connect(dev, "state-changed", \
                         G_CALLBACK(device_state_changed_cb), NULL);
    }

    return 0;
}
/**********************
 *   GLOBAL FUNCTIONS
 **********************/
NMClient *get_nm_client()
{
    return (NMClient *)get_ctx()->comm.nm_client;
}

void set_nm_client( NMClient *client)
{
    // Mutex ?
    get_ctx()->comm.nm_client = client;
}

const char *nm_state_to_str(NMState state)
{
    switch (state) {
    case NM_STATE_UNKNOWN:
        return "UNKNOWN";
    case NM_STATE_ASLEEP:
        return "ASLEEP";
    case NM_STATE_DISCONNECTED:
        return "DISCONNECTED";
    case NM_STATE_DISCONNECTING:
        return "DISCONNECTING";
    case NM_STATE_CONNECTING:
        return "CONNECTING";
    case NM_STATE_CONNECTED_LOCAL:
        return "CONNECTED_LOCAL";
    case NM_STATE_CONNECTED_SITE:
        return "CONNECTED_SITE";
    case NM_STATE_CONNECTED_GLOBAL:
        return "CONNECTED_GLOBAL";
    default:
        return "INVALID";
    }
}

void print_nm_state(NMState state)
{
    const char *state_str = nm_state_to_str(state);

    if (state < NM_STATE_CONNECTING)
        LOG_DEBUG("NetworkManager state: %s (%d)", state_str, state);
    else if (state < NM_STATE_CONNECTED_GLOBAL)
        LOG_TRACE("NetworkManager state: %s (%d)", state_str, state);
    else
        LOG_INFO("NetworkManager state: %s (%d)", state_str, state);
}

const char *nm_device_type_to_str(NMDeviceType type)
{
    switch (type) {
    case NM_DEVICE_TYPE_ETHERNET:
        return "Ethernet";
    case NM_DEVICE_TYPE_WIFI:
        return "Wi-Fi";
    case NM_DEVICE_TYPE_MODEM:
        return "Cellular";
    case NM_DEVICE_TYPE_BT:
        return "Bluetooth";
    case NM_DEVICE_TYPE_VLAN:
        return "VLAN";
    case NM_DEVICE_TYPE_BRIDGE:
        return "Bridge";
    case NM_DEVICE_TYPE_LOOPBACK:
        return "Loopback";
    default:
        return "Unknown";
    }
}

const char *nm_device_state_to_str(NMDeviceState state)
{
    switch (state) {
    case NM_DEVICE_STATE_UNKNOWN:
        return "NM_DEVICE_STATE_UNKNOWN";

    case NM_DEVICE_STATE_UNMANAGED:
        return "NM_DEVICE_STATE_UNMANAGED";

    case NM_DEVICE_STATE_UNAVAILABLE:
        return "NM_DEVICE_STATE_UNAVAILABLE";

    case NM_DEVICE_STATE_DISCONNECTED:
        return "NM_DEVICE_STATE_DISCONNECTED";

    case NM_DEVICE_STATE_PREPARE:
        return "NM_DEVICE_STATE_PREPARE";

    case NM_DEVICE_STATE_CONFIG:
        return "NM_DEVICE_STATE_CONFIG";

    case NM_DEVICE_STATE_NEED_AUTH:
        return "NM_DEVICE_STATE_NEED_AUTH";

    case NM_DEVICE_STATE_IP_CONFIG:
        return "NM_DEVICE_STATE_IP_CONFIG";

    case NM_DEVICE_STATE_IP_CHECK:
        return "NM_DEVICE_STATE_IP_CHECK";

    case NM_DEVICE_STATE_SECONDARIES:
        return "NM_DEVICE_STATE_SECONDARIES";

    case NM_DEVICE_STATE_ACTIVATED:
        return "NM_DEVICE_STATE_ACTIVATED";

    case NM_DEVICE_STATE_DEACTIVATING:
        return "NM_DEVICE_STATE_DEACTIVATING";

    case NM_DEVICE_STATE_FAILED:
        return "NM_DEVICE_STATE_FAILED";

    default:
        return "INVALID";
    }
}

const char *nm_device_state_desc(NMDeviceState state)
{
    switch (state) {
    case NM_DEVICE_STATE_UNKNOWN:
        return "Device state unknown";
    case NM_DEVICE_STATE_UNMANAGED:
        return "Recognized but not managed by NM";
    case NM_DEVICE_STATE_UNAVAILABLE:
        return "Managed but unavailable (no carrier, firmware, etc)";
    case NM_DEVICE_STATE_DISCONNECTED:
        return "Idle, ready but not connected";
    case NM_DEVICE_STATE_PREPARE:
        return "Preparing connection (MAC, link, etc)";
    case NM_DEVICE_STATE_CONFIG:
        return "Connecting (associate/dial/etc)";
    case NM_DEVICE_STATE_NEED_AUTH:
        return "Waiting for credentials or secrets";
    case NM_DEVICE_STATE_IP_CONFIG:
        return "Requesting IP configuration";
    case NM_DEVICE_STATE_IP_CHECK:
        return "Checking connectivity (e.g. captive portal)";
    case NM_DEVICE_STATE_SECONDARIES:
        return "Waiting for secondary (VPN, etc)";
    case NM_DEVICE_STATE_ACTIVATED:
        return "Network connection established";
    case NM_DEVICE_STATE_DEACTIVATING:
        return "Disconnecting, cleaning resources";
    case NM_DEVICE_STATE_FAILED:
        return "Connection failed or aborted";
    default:
        return "Invalid or unknown state";
    }
}

/* Log IPv4/IPv6 addresses if available */
static void log_device_ip_info(NMDevice *device)
{
    const NMIPConfig *ip4;
    const NMIPConfig *ip6;
    const GPtrArray *addrs;
    const NMIPAddress *ip;
    char buf[64] = {0};

    /* IPv4 */
    ip4 = nm_device_get_ip4_config(device);
    if (ip4) {
        addrs = nm_ip_config_get_addresses(ip4);
        if (addrs && addrs->len > 0) {
            ip = g_ptr_array_index(addrs, 0);
            snprintf(buf, sizeof(buf), "%s",
                 nm_ip_address_get_address(ip));
            LOG_TRACE("IPv4 address: %s", buf);
        }
    }

    /* IPv6 (first address) */
    ip6 = nm_device_get_ip6_config(device);
    if (ip6) {
        addrs = nm_ip_config_get_addresses(ip6);
        if (addrs && addrs->len > 0) {
            ip = g_ptr_array_index(addrs, 0);
            snprintf(buf, sizeof(buf), "%s",
                 nm_ip_address_get_address(ip));
            LOG_TRACE("IPv6 address: %s", buf);
        }
    }
}

/* Wi-Fi specific handling */
static void handle_wifi_device_state(NMDevice *device, NMDeviceState state)
{
    const char *iface = nm_device_get_iface(device);
    ap_info_t ap_info;
    int32_t ret;

    LOG_DEBUG("[%s] wifi: %s", iface, nm_device_state_desc(state));

    switch (state) {
    case NM_DEVICE_STATE_PREPARE:
        break;
    case NM_DEVICE_STATE_CONFIG:
        break;
    case NM_DEVICE_STATE_NEED_AUTH:
        break;
    case NM_DEVICE_STATE_IP_CONFIG:
        break;
    case NM_DEVICE_STATE_IP_CHECK:
        break;
    case NM_DEVICE_STATE_SECONDARIES:
        break;
    case NM_DEVICE_STATE_ACTIVATED:
        ret = get_ap_info_from_nm_ap(NM_ACCESS_POINT( \
                                     nm_device_wifi_get_active_access_point( \
                                     NM_DEVICE_WIFI(device))), &ap_info);
        if (!ret)
            LOG_INFO("[%s] wifi: activated -> AP=%s (%u%%)", iface,
                 ap_info.ssid, ap_info.strength);
        else
            LOG_INFO("[%s] wifi: activated (no AP details)", iface);
        log_device_ip_info(device);
        break;
    case NM_DEVICE_STATE_DEACTIVATING:
        break;
    case NM_DEVICE_STATE_DISCONNECTED:
        break;
    case NM_DEVICE_STATE_FAILED:
        LOG_ERROR("[%s] wifi: connection failed", iface);
        break;
    case NM_DEVICE_STATE_UNAVAILABLE:
        LOG_DEBUG("[%s] wifi: unavailable (rfkill/firmware/no-supplicant?)",
              iface);
        break;
    case NM_DEVICE_STATE_UNMANAGED:
        break;
    case NM_DEVICE_STATE_UNKNOWN:
    default:
        LOG_DEBUG("[%s] wifi: state=%s",
                  iface, nm_device_state_to_str(state));
        break;
    }
}

/* Ethernet specific handling */
static void handle_ethernet_device_state(NMDevice *device, NMDeviceState state)
{
    const char *iface = nm_device_get_iface(device);
    gboolean carrier;
    gint speed;

    LOG_DEBUG("[%s] ethernet: %s", iface, nm_device_state_desc(state));

    switch (state) {
    case NM_DEVICE_STATE_PREPARE:
        break;
    case NM_DEVICE_STATE_CONFIG:
        break;
    case NM_DEVICE_STATE_IP_CONFIG:
        break;
    case NM_DEVICE_STATE_ACTIVATED:
        carrier = nm_device_ethernet_get_carrier(NM_DEVICE_ETHERNET(device));
        speed = nm_device_ethernet_get_speed(NM_DEVICE_ETHERNET(device));
        if (carrier)
            LOG_INFO("[%s] ethernet: link up (%d Mbit/s)", iface, speed);
        else
            LOG_INFO("[%s] ethernet: activated (no carrier)", iface);
        log_device_ip_info(device);
        break;
    case NM_DEVICE_STATE_DEACTIVATING:
        break;
    case NM_DEVICE_STATE_DISCONNECTED:
        break;
    case NM_DEVICE_STATE_FAILED:
        LOG_ERROR("[%s] ethernet: connection failed", iface);
        break;
    case NM_DEVICE_STATE_UNAVAILABLE:
        break;
    case NM_DEVICE_STATE_UNMANAGED:
        break;
    case NM_DEVICE_STATE_UNKNOWN:
    default:
        LOG_DEBUG("[%s] ethernet: state=%s", iface, \
                  nm_device_state_to_str(state));
        break;
    }
}

/* Modem (cellular) specific handling */
static void handle_modem_device_state(NMDevice *device, NMDeviceState state)
{
    const char *iface = nm_device_get_iface(device);

    LOG_DEBUG("[%s] modem: %s", iface, nm_device_state_desc(state));

    switch (state) {
    case NM_DEVICE_STATE_PREPARE:
        LOG_DEBUG("[%s] modem: preparing (register/dial)", iface);
        break;
    case NM_DEVICE_STATE_CONFIG:
        LOG_DEBUG("[%s] modem: configuring (pdp/ppp/conn)", iface);
        break;
    case NM_DEVICE_STATE_ACTIVATED:
        LOG_INFO("[%s] modem: connected", iface);
        log_device_ip_info(device);
        break;
    case NM_DEVICE_STATE_DISCONNECTED:
        LOG_TRACE("[%s] modem: disconnected", iface);
        break;
    case NM_DEVICE_STATE_FAILED:
        LOG_ERROR("[%s] modem: connection failed", iface);
        break;
    case NM_DEVICE_STATE_UNAVAILABLE:
        LOG_DEBUG("[%s] modem: unavailable (sim/firmware?)", iface);
        break;
    default:
        LOG_DEBUG("[%s] modem: state=%s", \
                  iface, nm_device_state_to_str(state));
        break;
    }
}

/* Generic/fallback device handler */
static void handle_generic_device_state(NMDevice *device, NMDeviceState state)
{
    const char *iface = nm_device_get_iface(device);

    LOG_DEBUG("[%s] device: %s", iface, nm_device_state_desc(state));

    switch (state) {
    case NM_DEVICE_STATE_PREPARE:
        break;
    case NM_DEVICE_STATE_CONFIG:
        break;
    case NM_DEVICE_STATE_IP_CONFIG:
        break;
    case NM_DEVICE_STATE_ACTIVATED:
        LOG_INFO("[%s] device: activated", iface);
        log_device_ip_info(device);
        break;
    case NM_DEVICE_STATE_DISCONNECTED:
        break;
    case NM_DEVICE_STATE_DEACTIVATING:
        break;
    case NM_DEVICE_STATE_FAILED:
        LOG_ERROR("[%s] device: failed", iface);
        break;
    case NM_DEVICE_STATE_UNAVAILABLE:
        break;
    case NM_DEVICE_STATE_UNMANAGED:
        break;
    case NM_DEVICE_STATE_UNKNOWN:
    default:
        LOG_DEBUG("[%s] device: state=%s", \
                  iface, nm_device_state_to_str(state));
        break;
    }
}

/*
 * Public entry: handle device state based on NMDevice internal state.
 * This function is exhaustive over NMDeviceState and delegates to
 * device-type-specific handlers for detailed processing.
 */
void handle_nm_device_state(NMDevice *device)
{
    NMDeviceType type;
    NMDeviceState state;
    const char *iface;
    const char *type_str;
    const char *state_str;
    const char *state_desc;

    if (!device) {
        LOG_ERROR("handle_nm_device_state: device is NULL");
        return;
    }

    state = nm_device_get_state(device);
    state_str = nm_device_state_to_str(state);
    state_desc = nm_device_state_desc(state);
    type = nm_device_get_device_type(device);
    type_str = nm_device_type_to_str(type);
    iface = nm_device_get_iface(device);

    LOG_TRACE("[%s] \n\tdevice=%s \n\ttype=%s state=%s (%d) \n\t-> %s",
          iface ? iface : "unknown",
          nm_object_get_path(NM_OBJECT(device)),
          type_str ? type_str : "unknown",
          state_str ? state_str : "unknown", state,
          state_desc ? state_desc : "");

    /* Delegate to per-type handlers */
    switch (type) {
    case NM_DEVICE_TYPE_WIFI:
        handle_wifi_device_state(device, state);
        break;
    case NM_DEVICE_TYPE_ETHERNET:
        handle_ethernet_device_state(device, state);
        break;
    case NM_DEVICE_TYPE_MODEM:
        handle_modem_device_state(device, state);
        break;
    default:
        handle_generic_device_state(device, state);
        break;
    }
}

int32_t init_network_manager_client(void)
{
    NMClient *client;
    g_autoptr(GError) error = NULL;
    int32_t ret;

    client = nm_client_new(NULL, &error);
    if (!client) {
        LOG_ERROR("Failed to create NMClient: %s", error->message);
        return -EIO;
    }

    if (!g_signal_handler_find(client, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                               G_CALLBACK(network_state_changed_cb), NULL)) {
        g_signal_connect(client, "notify::state",
                         G_CALLBACK(network_state_changed_cb), NULL);
    }

    set_nm_client(client);

    ret = init_nm_device_callback(client);
    if (ret) {
        LOG_WARN("Register device state change callback failed, ret %d", ret);
    }

    LOG_INFO("NetworkManager client initialized");
    return 0;
}

void deinit_network_manager_client(void)
{
    NMClient *client;

    /* Release the NetworkManager client instance */
    client = get_nm_client();
    if (!client)
        return;

    g_object_unref(client);
    set_nm_client(NULL);

    LOG_INFO("NetworkManager client deinitialized");
}

NMDevice *get_nm_dev_by_iface(const char *iface)
{
    NMClient *client;
    const GPtrArray *devs;
    const char *tmp_iface;
    NMDevice *net_dev;
    guint i;

    /* Get NetworkManager client and device list */
    client = get_nm_client();
    devs = nm_client_get_devices(client);

    for (i = 0; i < devs->len; ++i) {
        net_dev = g_ptr_array_index(devs, i);
        if (!net_dev)
            continue;

        tmp_iface = nm_device_get_iface(net_dev);
        if (!strcmp(tmp_iface, iface)) {
            LOG_TRACE("Expected interface detected: %s", tmp_iface);
            return net_dev;
        }

        LOG_TRACE("Other NM interface found: %s", tmp_iface);
    }

    return NULL;
}

int32_t disconnect_interface(const char *iface)
{
    NMDevice *dev;
    GCancellable *cancel;
    g_autoptr(GError) error = NULL;

    /* Lookup the NM device by interface name */
    dev = get_nm_dev_by_iface(iface);
    if (!dev)
        return -EIO;

    cancel = g_cancellable_new();

    /*
     * Asynchronously request device disconnection.
     * The callback handles result reporting.
     */
    nm_device_disconnect_async(dev, cancel, \
                               (GAsyncReadyCallback)disconnect_interface_cb, \
                               NULL);

    g_object_unref(cancel);
    return 0;
}
