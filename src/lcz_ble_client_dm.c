/**
 * @file lcz_ble_client_dm.c
 *
 * Copyright (c) 2022 Laird Connectivity
 *
 * SPDX-License-Identifier: LicenseRef-LairdConnectivity-Clause
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(lcz_ble_client_dm, CONFIG_LCZ_BLE_CLIENT_DM_LOG_LEVEL);

/**************************************************************************************************/
/* Includes                                                                                       */
/**************************************************************************************************/
#include <zephyr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <init.h>
#include "lcz_lwm2m_client.h"
#if defined(CONFIG_ATTR)
#include "attr.h"
#endif
#include "ble_client_dm_ble.h"

/**************************************************************************************************/
/* Local Constant, Macro and Type Definitions                                                     */
/**************************************************************************************************/
#define DM_CONNECTION_DELAY_FALLBACK 5

/**************************************************************************************************/
/* Local Function Prototypes                                                                      */
/**************************************************************************************************/
static void lwm2m_client_connected_event(struct lwm2m_ctx *client, int lwm2m_client_index,
					 bool connected, enum lwm2m_rd_client_event client_event);
static int lcz_ble_client_dm_init(const struct device *device);
static void dm_start_work_handler(struct k_work *work);
static void dm_start(void);
static void dm_stop(void);

/**************************************************************************************************/
/* Local Data Definitions                                                                         */
/**************************************************************************************************/
static struct lcz_lwm2m_client_event_callback_agent lwm2m_event_agent;
static K_WORK_DELAYABLE_DEFINE(dm_start_work, dm_start_work_handler);

/**************************************************************************************************/
/* Local Function Definitions                                                                     */
/**************************************************************************************************/
/** @brief Handle connection events from the LwM2M client
 *
 * When errors occur, this function will attempt to restart the LwM2M client.
 *
 * @param[in] client LwM2M engine client context
 * @param[in] lwm2m_client_index RD client index for this connection
 * @param[in] connected true if connected to LwM2M server, false if not
 * @param[in] client_event RD client event that triggered this callback
 */
static void lwm2m_client_connected_event(struct lwm2m_ctx *client, int lwm2m_client_index,
					 bool connected, enum lwm2m_rd_client_event client_event)
{
	if (lwm2m_client_index == CONFIG_LCZ_BLE_CLIENT_DM_CLIENT_INDEX) {
		if (connected == false) {
			if (client_event == LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_FAILURE ||
			    client_event == LWM2M_RD_CLIENT_EVENT_REGISTRATION_FAILURE ||
			    client_event == LWM2M_RD_CLIENT_EVENT_NETWORK_ERROR) {
				/* Restart the client */
				dm_stop();
				dm_start();
			}
		}
	}
}

/** @brief Device manager start delayed work handler
 *
 * This delayed work is used to delay the start of the device manager based on
 * the configured start delay.
 *
 * @param[in] work Delayed work item
 */
static void dm_start_work_handler(struct k_work *work)
{
	char *ep_name;
	int ret;

#if defined(CONFIG_LCZ_LWM2M_CLIENT_ENABLE_ATTRIBUTES)
	ep_name = (char *)attr_get_quasi_static(ATTR_ID_lwm2m_endpoint);
#else
	ep_name = CONFIG_LCZ_LWM2M_CLIENT_ENDPOINT_NAME;
#endif
	ret = lcz_lwm2m_client_connect(CONFIG_LCZ_BLE_CLIENT_DM_CLIENT_INDEX, -1, -1, ep_name,
				       LCZ_LWM2M_CLIENT_TRANSPORT_BLE);
	if (ret < 0) {
		LOG_ERR("Failed to start LwM2M client: %d", ret);
	}
}

/** @brief Start the client device manager
 */
static void dm_start(void)
{
	uint16_t delay;

	/* Start the client after a short delay */
#if defined(CONFIG_LCZ_BLE_CLIENT_DM_INIT_KCONFIG)
	delay = CONFIG_LCZ_BLE_CLIENT_DM_CONNECTION_DELAY;
#else
	delay = attr_get_uint32(ATTR_ID_dm_cnx_delay, DM_CONNECTION_DELAY_FALLBACK);
#endif
	k_work_reschedule(&dm_start_work, K_SECONDS(delay));
}

/** @brief Stop the client device manager
 */
static void dm_stop(void)
{
	lcz_lwm2m_client_disconnect(CONFIG_LCZ_BLE_CLIENT_DM_CLIENT_INDEX, false);
}

SYS_INIT(lcz_ble_client_dm_init, APPLICATION, CONFIG_LCZ_BLE_CLIENT_DM_INIT_PRIORITY);
/**************************************************************************************************/
/* SYS INIT                                                                                       */
/**************************************************************************************************/
static int lcz_ble_client_dm_init(const struct device *device)
{
	int ret;

	/* Register handler for client events */
	lwm2m_event_agent.connected_callback = lwm2m_client_connected_event;
	(void)lcz_lwm2m_client_register_event_callback(&lwm2m_event_agent);

	/* Enable bluetooth */
	ret = bt_enable(NULL);
	if (ret != 0) {
		LOG_ERR("bt_enable failed: %d", ret);
	} else {
#if defined(CONFIG_LCZ_BLE_CLIENT_DM_ADVERTISE)
		/* Start advertising */
		ble_client_dm_device_ble_advertise();
#endif

		/* Start device management */
		dm_start();
	}

	return ret;
}
