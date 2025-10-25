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

	LOG_INFO("Interface %s disconnected successfully",
		 nm_device_get_iface(dev));
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

	/* Initialize a new NetworkManager client instance */
	client = nm_client_new(NULL, &error);
	if (!client) {
		LOG_ERROR("Failed to create NMClient: %s", error->message);
		return -EIO;
	}

	set_nm_client(client);
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
	nm_device_disconnect_async(dev,
				   cancel,
				   (GAsyncReadyCallback)disconnect_interface_cb,
				   NULL);

	g_object_unref(cancel);
	return 0;
}
