/**
 * @file wifi.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
// #define LOG_LEVEL LOG_LEVEL_TRACE
#if defined(LOG_LEVEL)
#warning "LOG_LEVEL defined locally will override the global setting in this file"
#endif
#include "log.h"

#include <stdio.h>
#include <glib.h>
#include <glib-object.h>
#include <NetworkManager.h>

#include "comm/net/network.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    GMainLoop *loop;
    NMClient *client;
    NMDevice *device;
    gboolean done;
} WifiConnectContext;

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

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
/**
 * Disconnect the given Wi-Fi device from any connected AP.
 */
int32_t wifi_disconnect_device(const char *iface_name)
{
    NMDevice *dev;
    GError *error = NULL;

    dev = get_nm_dev_by_iface(iface_name);
    if (!dev) {
        LOG_ERROR("Device %s not found", iface_name);
        return EXIT_FAILURE;
    }

    if (!NM_IS_DEVICE_WIFI(dev)) {
        LOG_ERROR("Device %s is not a Wi-Fi device", iface_name);
        return EXIT_FAILURE;
    }

    LOG_INFO("Disconnecting device %s...", iface_name);
    nm_device_disconnect(dev, NULL, &error);
    if (error) {
        LOG_ERROR("Disconnect failed: %s", error->message);
        g_error_free(error);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
 * Check if given Wi-Fi device is connected to target SSID.
 * Return 1 if connected, 0 if not connected, -1 on error.
 */
int32_t wifi_is_connected_to_ssid(const char *iface_name, const char *ssid)
{
    NMDevice *dev;
    NMDeviceWifi *wifi_dev;
    NMAccessPoint *active_ap;
    GBytes *active_ssid_bytes;
    char *active_ssid_str;
    int32_t connected;

    dev = get_nm_dev_by_iface(iface_name);
    if (!dev) {
        LOG_ERROR("Device %s not found", iface_name);
        return -1;
    }

    if (!NM_IS_DEVICE_WIFI(dev)) {
        LOG_ERROR("Device %s is not a Wi-Fi device", iface_name);
        return -1;
    }

    wifi_dev = NM_DEVICE_WIFI(dev);
    active_ap = nm_device_wifi_get_active_access_point(wifi_dev);
    if (!active_ap) {
        LOG_TRACE("Device %s is not connected to any AP", iface_name);
        return 0;
    }

    active_ssid_bytes = nm_access_point_get_ssid(active_ap);
    if (!active_ssid_bytes) {
        LOG_TRACE("Device %s active AP has no SSID", iface_name);
        return 0;
    }

    active_ssid_str = nm_utils_ssid_to_utf8(g_bytes_get_data(active_ssid_bytes, NULL),
                                           g_bytes_get_size(active_ssid_bytes));

    connected = (active_ssid_str && ssid &&
                 strcmp(active_ssid_str, ssid) == 0) ? 1 : 0;

    LOG_TRACE("Device %s connected SSID: %s (target: %s) → %s",
              iface_name,
              active_ssid_str ? active_ssid_str : "<hidden>",
              ssid ? ssid : "<null>",
              connected ? "MATCH" : "NO MATCH");

    g_free(active_ssid_str);

    return connected;
}

int32_t wifi_list_access_points(const char *iface_name)
{
    NMDevice *dev;
    NMDeviceWifi *wifi_device;
    const GPtrArray *aps;
    NMAccessPoint *ap;
    GBytes *ssid_bytes;
    char *ssid_str;
    guint i;
    int32_t strength;
    NM80211ApSecurityFlags sec_flags;

    dev = get_nm_dev_by_iface(iface_name);
    if (!dev) {
        LOG_ERROR("Device %s not found", iface_name);
        return EXIT_FAILURE;
    }

    if (!NM_IS_DEVICE_WIFI(dev)) {
        LOG_ERROR("Device %s is not a Wi-Fi device", iface_name);
        return EXIT_FAILURE;
    }

    wifi_device = NM_DEVICE_WIFI(dev);
    aps = nm_device_wifi_get_access_points(wifi_device);

    LOG_INFO("Device %s: found %u access points", iface_name, aps->len);

    for (i = 0; i < aps->len; i++) {
        ap = g_ptr_array_index(aps, i);
        GBytes *ssid_bytes = nm_access_point_get_ssid(ap);
        if (!ssid_bytes)
            continue;

        ssid_str = nm_utils_ssid_to_utf8(g_bytes_get_data(ssid_bytes, NULL),
                                         g_bytes_get_size(ssid_bytes));

        strength = nm_access_point_get_strength(ap);
        sec_flags = nm_access_point_get_flags(ap);

        LOG_INFO("  SSID: %-30s Strength: %3d%% SecurityFlags: 0x%x",
                 ssid_str ? ssid_str : "<hidden>",
                 strength,
                 sec_flags);

        g_free(ssid_str);
    }

    return EXIT_SUCCESS;
}

NMAccessPoint *find_ap_on_wifi_device(NMDevice *device, \
                                      const char *bssid, \
                                      const char *ssid, \
                                      gboolean complete)
{
    const GPtrArray *aps;
    NMAccessPoint *ap;
    int32_t i;

    g_return_val_if_fail(NM_IS_DEVICE_WIFI(device), NULL);

    aps = nm_device_wifi_get_access_points(NM_DEVICE_WIFI(device));
    ap = NULL;

    LOG_TRACE("Found %u access points:\n", aps->len);

    for (i = 0; i < aps->len; i++) {
        NMAccessPoint *candidate_ap;
        candidate_ap = g_ptr_array_index(aps, i);

        /* Match BSSID if requested */
        if (bssid) {
            const char *candidate_bssid;

            candidate_bssid = nm_access_point_get_bssid(candidate_ap);
            if (!candidate_bssid)
                continue;

            if (complete) {
                if (g_str_has_prefix(candidate_bssid, bssid))
                    LOG_TRACE("%s\n", candidate_bssid);
            } else if (strcmp(bssid, candidate_bssid) != 0)
                continue;
        }

        /* Match SSID if requested */
        if (ssid) {
            GBytes *candidate_ssid;
            char *ssid_tmp;

            candidate_ssid = nm_access_point_get_ssid(candidate_ap);
            if (!candidate_ssid)
                continue;

            ssid_tmp = nm_utils_ssid_to_utf8( \
                g_bytes_get_data(candidate_ssid, NULL), \
                g_bytes_get_size(candidate_ssid));

            if (complete) {
                if (g_str_has_prefix(ssid_tmp, ssid))
                    LOG_TRACE("%s\n", ssid_tmp);
            } else if (strcmp(ssid, ssid_tmp) != 0) {
                g_free(ssid_tmp);
                continue;
            }

            LOG_TRACE("Wi-Fi interface [%s] found AP: [%s]\n", \
                      nm_device_get_iface(device), \
                      ssid_tmp ? ssid_tmp : "<hidden>");

            g_free(ssid_tmp);
        }

        /* If not in "complete" mode, return first match */
        if (!complete) {
            ap = candidate_ap;
            break;
        }
    }

    return ap;
}

/* Caller owns returned NMConnection* → must g_object_unref() when done */
NMConnection *find_connection_on_wifi_device(NMDevice *dev, \
                                             NMAccessPoint *ap, \
                                             const char *con_name)
{
    const GPtrArray *avail_cons;
    gboolean name_match;
    NMConnection *connection;
    int32_t i;

    g_return_val_if_fail(NM_IS_DEVICE_WIFI(dev), NULL);
    g_return_val_if_fail(NM_IS_ACCESS_POINT(ap), NULL);

    avail_cons = nm_device_get_available_connections(dev);
    name_match = FALSE;
    connection = NULL;

    LOG_TRACE("Found %u available connections\n", avail_cons->len);

    for (i = 0; i < avail_cons->len; i++) {
        NMConnection *avail_con;
        const char *id;

        avail_con = g_ptr_array_index(avail_cons, i);
        id = nm_connection_get_id(avail_con);

        LOG_TRACE("Wi-Fi connection ID found: [%s]", id);

        /* Match connection name (optional) */
        if (con_name) {
            if (!id || strcmp(id, con_name) != 0)
                continue;
            name_match = TRUE;
        }

        /* Check if connection is valid for AP */
        if (nm_access_point_connection_valid(ap, avail_con)) {
            connection = g_object_ref(avail_con);
            LOG_TRACE("Wi-Fi connection ID [%s] is valid for AP", id);
            break;
        }
    }

    if (name_match && !connection) {
        LOG_TRACE("Error: Connection '%s' exists but properties don't match.", \
                  con_name);
        return NULL;
    }

    return connection;
}

/**
 * Callback for NMDevice::notify::active-connection.
 * Useful fallback when ActiveConnection is not available in NM API.
 */
static void on_device_active_connection_changed(GObject *object, \
                                                GParamSpec *pspec, \
                                                gpointer user_data)
{
    WifiConnectContext *ctx;
    NMDevice *device;
    NMActiveConnection *ac;
    NMRemoteConnection *remote;
    const char *id;

    ctx = (WifiConnectContext *)user_data;
    device = NM_DEVICE(object);
    ac = nm_device_get_active_connection(device);
    if (ac) {
        remote = nm_active_connection_get_connection(ac);
        id = remote ? nm_connection_get_id(NM_CONNECTION(remote)) : NULL;

        LOG_TRACE("[DEVICE] Active connection changed → %s", \
                  id ? id : "(null)");
    } else {
        LOG_TRACE("[DEVICE] No active connection");
    }
}

/**
 * Callback for NMActiveConnection::notify::state.
 * Used to track connection state changes.
 */
static void on_active_connection_state_changed(GObject *object, \
                                               GParamSpec *pspec, \
                                               gpointer user_data)
{
    WifiConnectContext *ctx;
    NMActiveConnection *ac;
    NMActiveConnectionState state;

    ctx = (WifiConnectContext *)user_data;
    ac = NM_ACTIVE_CONNECTION(object);
    state = nm_active_connection_get_state(ac);

    LOG_TRACE("[ACTIVE-CONNECTION] State changed → %d", state);

    if (state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED) {
        LOG_INFO("[ACTIVE-CONNECTION] Connected successfully!");
        ctx->done = TRUE;
        g_main_loop_quit(ctx->loop);
    } else if (state == NM_ACTIVE_CONNECTION_STATE_DEACTIVATED) {
        LOG_TRACE("[ACTIVE-CONNECTION] Deactivated");
    }
}

/**
 * Callback for nm_client_add_and_activate_connection2_async() completion.
 */
static void add_and_activate_cb(GObject *client_obj, GAsyncResult *res, \
                                gpointer user_data)
{
    WifiConnectContext *ctx;
    GError *error;
    GVariant *out_result;
    NMActiveConnection *ac;

    ctx = (WifiConnectContext *)user_data;
    error = NULL;
    out_result = NULL;

    ac = nm_client_add_and_activate_connection2_finish(
        NM_CLIENT(client_obj), res, &out_result, &error);

    if (error) {
        LOG_ERROR("[CALLBACK] Failed to activate connection: %s",
                  error->message);
        g_error_free(error);
        ctx->done = TRUE;
        g_main_loop_quit(ctx->loop);
        return;
    }

    LOG_TRACE("[CALLBACK] Connection initiated: %s",
              ac ? nm_object_get_path(NM_OBJECT(ac)) : "NULL");

    if (ac) {
        g_signal_connect(ac, "notify::state",
                         G_CALLBACK(on_active_connection_state_changed),
                         ctx);
    } else {
        /* Fallback to device signal */
        LOG_TRACE("[CALLBACK] No ActiveConnection object returned → "
                  "fallback to device signal.");
    }
}

/**
 * WiFi connection flow using NMClient API.
 * Will block until connection completes or fails.
 */
void wifi_connect_flow(NMClient *client, NMDevice *dev, NMAccessPoint *ap,
                       const char *iface_name, const char *ssid,
                       const char *password)
{
    WifiConnectContext ctx;
    NMConnection *connection;
    NMSettingWireless *s_wifi;
    NMSettingWirelessSecurity *s_sec;
    NMSettingIPConfig *s_ip4;
    NMSettingConnection *s_con;

    ctx.loop = NULL;
    ctx.client = client;
    ctx.device = dev;
    ctx.done = FALSE;

    ctx.loop = g_main_loop_new(NULL, FALSE);

    /* Listen for device active-connection change */
    g_signal_connect(dev, "notify::active-connection",
                     G_CALLBACK(on_device_active_connection_changed), &ctx);

    /* Build in-memory NMConnection object */
    connection = nm_simple_connection_new();

    s_wifi = (NMSettingWireless *)nm_setting_wireless_new();
    g_object_set(G_OBJECT(s_wifi),
                 NM_SETTING_WIRELESS_SSID, nm_access_point_get_ssid(ap),
                 NM_SETTING_WIRELESS_MODE, "infrastructure",
                 NULL);
    nm_connection_add_setting(connection, NM_SETTING(s_wifi));

    if (password && strlen(password) > 0) {
        s_sec = (NMSettingWirelessSecurity *)
                nm_setting_wireless_security_new();
        g_object_set(G_OBJECT(s_sec),
                     NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk",
                     NM_SETTING_WIRELESS_SECURITY_PSK, password,
                     NULL);
        nm_connection_add_setting(connection, NM_SETTING(s_sec));
    }

    s_ip4 = (NMSettingIPConfig *)nm_setting_ip4_config_new();
    g_object_set(G_OBJECT(s_ip4),
                 NM_SETTING_IP_CONFIG_METHOD, "auto",
                 NULL);
    nm_connection_add_setting(connection, NM_SETTING(s_ip4));

    s_con = (NMSettingConnection *)nm_setting_connection_new();
    g_object_set(G_OBJECT(s_con),
                 NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
                 NM_SETTING_CONNECTION_INTERFACE_NAME, iface_name,
                 NM_SETTING_CONNECTION_ID, ssid,
                 NULL);
    nm_connection_add_setting(connection, NM_SETTING(s_con));

    /* Trigger add_and_activate_connection2 async */
    nm_client_add_and_activate_connection2(client,
                                           connection,
                                           dev,
                                           NULL,
                                           NULL,
                                           NULL,
                                           add_and_activate_cb,
                                           &ctx);

    /* Run main loop until connection succeeds/fails */
    g_main_loop_run(ctx.loop);
    g_main_loop_unref(ctx.loop);

    if (ctx.done) {
        LOG_INFO("[MAIN] Wi-Fi connect flow complete.");
    } else {
        LOG_ERROR("[MAIN] Wi-Fi connect flow aborted.");
    }
}

/**
 * Public API entry point: connect to Wi-Fi SSID on interface.
 */
int32_t wifi_connect_to_ssid(const char *iface_name, const char *ssid,
                         const char *password)
{
    NMClient *client;
    NMDevice *dev;
    NMAccessPoint *ap;

    client = get_nm_client();
    if (!client) {
        LOG_ERROR("Failed to get NMClient");
        return EXIT_FAILURE;
    }

    dev = get_nm_dev_by_iface(iface_name);
    if (!dev) {
        LOG_ERROR("Device %s not found", iface_name);
        return EXIT_FAILURE;
    }

    ap = find_ap_on_wifi_device(dev, NULL, ssid, FALSE);
    if (!ap) {
        LOG_ERROR("SSID '%s' not found on device '%s'", ssid, iface_name);
        return EXIT_FAILURE;
    }

    LOG_INFO("Found target AP for SSID '%s' on interface '%s'",
             ssid, iface_name);

    wifi_connect_flow(client, dev, ap, iface_name, ssid, password);

    return EXIT_SUCCESS;
}
