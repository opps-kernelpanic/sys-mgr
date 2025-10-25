/**
 * @file network.c
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

int32_t network_manager_comm_init()
{
    g_autoptr(GError) error = NULL;
    NMClient *client = NULL;

    client = nm_client_new(NULL, &error);
    if (!client) {
        LOG_ERROR("Failed to create NMClient: %s\n", error->message);
        return EXIT_FAILURE;
    }
    set_nm_client(client);

    return EXIT_SUCCESS;
}

void network_manager_comm_deinit()
{
    NMClient *client = get_nm_client();

    if (client) {
        g_object_unref(client);
        set_nm_client(NULL);
    }
}

NMDevice * g_nm_device_get_by_iface(const char *exp_iface)
{
    NMClient *client = get_nm_client();
    const GPtrArray *devs = nm_client_get_devices(client);
    const char *iface;

    for (guint i = 0; i < devs->len; ++i) {
        NMDevice *net_dev = g_ptr_array_index(devs, i);
        iface = nm_device_get_iface(net_dev);
        if (net_dev && !strcmp(iface, exp_iface)) {
            LOG_TRACE("Found network interface: %s", iface);
            return net_dev;
        } else {
            LOG_TRACE("Detected device interface: %s", iface);
        }
    }

    return NULL;
}

// TODO: Listen to state change signal from NM
int32_t disconnect_interface(const char *exp_iface)
{
    g_autoptr(GError) error = NULL;
    NMDevice *dev = g_nm_device_get_by_iface(exp_iface);
    if (!dev) {
        return EXIT_FAILURE;
    }

    if (!nm_device_disconnect(dev, NULL, &error)) {
        LOG_ERROR("Failed to disconnect: %s\n", error->message);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
