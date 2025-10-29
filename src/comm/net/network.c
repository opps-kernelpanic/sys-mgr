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
    NMClient *client;

    iface = nm_device_get_iface(device);
    LOG_DEBUG("Device [%s] state changed: %s -> %s (reason: %d)",
              iface,
              nm_state_to_str(old_state),
              nm_state_to_str(new_state),
              reason);

    client = get_nm_client();
    if (!client) {
        LOG_ERROR("Failed to get NMClient");
        return;
    }

    handle_nm_device_state(client, device);
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

const char *nm_device_type_str(NMDeviceType type)
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

/*
 * Generalized handler for any NM-managed network device.
 * Logs current state, type, name, and relevant details if available.
 */
void handle_nm_device_state(NMClient *client, NMDevice *device)
{
    NMState state;
    NMDeviceType type;
    const char *iface;
    const char *type_str;
    char addr[64] = {0};
    gint speed = 0;
    gboolean carrier;
    int32_t ret;

    if (!client || !device) {
        LOG_ERROR("Invalid NMClient or device");
        return;
    }

    state = nm_client_get_state(client);

    type = nm_device_get_device_type(device);
    type_str = nm_device_type_str(type);
    iface = nm_device_get_iface(device);

    LOG_TRACE("[%s] Handling state for %s device", iface, type_str);

    switch (state) {
    case NM_STATE_CONNECTED_GLOBAL:
    case NM_STATE_CONNECTED_SITE:
    case NM_STATE_CONNECTED_LOCAL:
        switch (type) {
        case NM_DEVICE_TYPE_WIFI: {
            ap_info_t ap_info;

            ret = get_wifi_connected_ap_info(&ap_info);
            if (!ret)
                LOG_INFO("[%s] %s connected to AP [%s - %d%%]", \
                         iface, type_str, \
                         ap_info.ssid, ap_info.strength);
            else
                LOG_TRACE("[%s] %s connected (no AP info)", \
                          iface, type_str);
            break;
        }
        case NM_DEVICE_TYPE_ETHERNET:
            carrier = nm_device_ethernet_get_carrier( \
                    NM_DEVICE_ETHERNET(device));
            speed = nm_device_ethernet_get_speed( \
                    NM_DEVICE_ETHERNET(device));
            if (carrier)
                LOG_INFO("[%s] %s link up (%d Mbit/s)", \
                         iface, type_str, speed);
            else
                LOG_TRACE("[%s] %s connected (no carrier)", \
                          iface, type_str);
            break;
        case NM_DEVICE_TYPE_MODEM:
            LOG_INFO("[%s] Cellular modem connected", iface);
            break;
        default:
            LOG_INFO("[%s] %s connected (state=%s)", \
                     iface, type_str, nm_state_to_str(state));
            break;
        }

        if (nm_device_get_ip4_config(device)) {
            const NMIPConfig *cfg = nm_device_get_ip4_config(device);
            const GPtrArray *addrs = nm_ip_config_get_addresses(cfg);
            if (addrs && addrs->len > 0) {
                const NMIPAddress *ip = g_ptr_array_index(addrs, 0);
                snprintf(addr, sizeof(addr), "%s", \
                         nm_ip_address_get_address(ip));
                LOG_TRACE("[%s] IPv4 address: %s", iface, addr);
            }
        }
        break;

    case NM_STATE_CONNECTING:
        LOG_TRACE("[%s] %s connecting...", iface, type_str);
        break;

    case NM_STATE_DISCONNECTING:
        LOG_TRACE("[%s] %s disconnecting...", iface, type_str);
        break;

    case NM_STATE_DISCONNECTED:
        LOG_TRACE("[%s] %s enabled but not connected", \
                  iface, type_str);
        break;

    case NM_STATE_ASLEEP:
        LOG_DEBUG("[%s] %s in sleep mode", iface, type_str);
        break;

    case NM_STATE_UNKNOWN:
    default:
        LOG_DEBUG("[%s] %s state unknown (%d)", \
                  iface, type_str, state);
        break;
    }
}
