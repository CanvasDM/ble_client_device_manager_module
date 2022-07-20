/**
 * @file ble_client_dm_ble.c
 * @brief
 *
 * Copyright (c) 2022 Laird Connectivity LLC
 *
 * SPDX-License-Identifier: LicenseRef-LairdConnectivity-Clause
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(ble_client_dm_ble, CONFIG_LCZ_BLE_CLIENT_DM_LOG_LEVEL);

/**************************************************************************************************/
/* Includes                                                                                       */
/**************************************************************************************************/
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <init.h>
#include "lcz_sensor_adv_format.h"
#include "attr.h"
#include "lcz_lwm2m_client.h"
#include "ble_client_dm_ble.h"

/**************************************************************************************************/
/* Local Constant, Macro and Type Definitions                                                     */
/**************************************************************************************************/
/* Bit in the flags word to signal when LwM2M data is ready */
#define LWM2M_DATA_READY_FLAG (1 << 11)
#warning "bug #22088: use correct advertising flag for LwM2M"

/* Delay before updating advertising data */
#define ADV_UPDATE_DELAY K_MSEC(100)

/* Delay after advertising start fails */
#define ADV_RESTART_DELAY K_SECONDS(2)

/**************************************************************************************************/
/* Local Function Prototypes                                                                      */
/**************************************************************************************************/
#if defined(CONFIG_LCZ_BLE_CLIENT_DM_ADVERTISE)
static void bt_connected(struct bt_conn *conn, uint8_t reason);
static void bt_disconnected(struct bt_conn *conn, uint8_t reason);
static void adv_update_work_handler(struct k_work *item);
static void lwm2m_data_ready(bool data_ready);
#endif
#if defined(CONFIG_ATTR)
static int ble_client_dm_device_ble_addr_init(const struct device *device);
#endif

/**************************************************************************************************/
/* Local Data Definitions                                                                         */
/**************************************************************************************************/
#if defined(CONFIG_LCZ_BLE_CLIENT_DM_ADVERTISE)
static LczSensorAdEvent_t ad;
static LczSensorRspWithHeader_t rsp;
static struct bt_le_ext_adv *adv = NULL;
static bool connected = false;
static bool advertising = false;
static uint16_t flags = 0;

static struct bt_le_adv_param bt_param =
	BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
			     BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX, NULL);

static struct bt_data bt_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, &ad, sizeof(ad)),
};

static struct bt_data bt_rsp[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, &rsp, sizeof(rsp)),
};

static struct bt_conn_cb bt_conn_cb = {
	.connected = bt_connected,
	.disconnected = bt_disconnected,
};

static K_WORK_DELAYABLE_DEFINE(adv_update_work, adv_update_work_handler);
#endif

/**************************************************************************************************/
/* Global Function Definitions                                                                    */
/**************************************************************************************************/
#if defined(CONFIG_LCZ_BLE_CLIENT_DM_ADVERTISE)
int ble_client_dm_device_ble_advertise(void)
{
	size_t count = 1;
	bt_addr_le_t addr;
	int ret;

	/* Register connection callbacks */
	bt_conn_cb_register(&bt_conn_cb);

	/* Register callback with the LwM2M engine */
	lcz_lwm2m_client_register_data_ready_cb(lwm2m_data_ready);

	/* Fetch the BLE address */
	bt_id_get(&addr, &count);
	if (count < 1) {
		LOG_ERR("No Bluetooth address found");
	}

	/* Fill in advertisement data */
	ad.companyId = LAIRD_CONNECTIVITY_MANUFACTURER_SPECIFIC_COMPANY_ID1;
	ad.protocolId = BTXXX_1M_PHY_AD_PROTOCOL_ID;
	ad.networkId = 0;
	ad.flags = flags;
	memcpy(&ad.addr, &addr.a, sizeof(bt_addr_t));
	ad.recordType = SENSOR_EVENT_RESERVED;
	ad.resetCount = 0;

	/* Fill in scan response data */
	rsp.companyId = LAIRD_CONNECTIVITY_MANUFACTURER_SPECIFIC_COMPANY_ID1;
	rsp.protocolId = BTXXX_1M_PHY_RSP_PROTOCOL_ID;
	rsp.rsp.productId = BT6XX_PRODUCT_ID;
	rsp.rsp.firmwareVersionMajor = 0;
	rsp.rsp.firmwareVersionMinor = 0;
	rsp.rsp.firmwareVersionPatch = 0;
	rsp.rsp.firmwareType = 0;
	rsp.rsp.configVersion = 0;
	rsp.rsp.hardwareVersion = 0;

	/* Create the advertiser */
	ret = bt_le_ext_adv_create(&bt_param, NULL, &adv);
	if (ret) {
		LOG_ERR("Failed to create advertiser set: %d", ret);
	}

	/* Start advertising */
	if (ret == 0) {
		connected = false;
		advertising = false;
		k_work_reschedule(&adv_update_work, ADV_UPDATE_DELAY);
	}

	return ret;
}
#endif

/**************************************************************************************************/
/* Local Function Definitions                                                                     */
/**************************************************************************************************/
#if defined(CONFIG_LCZ_BLE_CLIENT_DM_ADVERTISE)
/** @brief BLE connected callback
 *
 * @param[in] conn Connection handle for new connection
 * @param[in] reason Connection failure reason
 */
static void bt_connected(struct bt_conn *conn, uint8_t reason)
{
	/* We are connected and advertising has stopped automatically */
	connected = true;
	advertising = false;
}

/** @brief BLE disconnected callback
 *
 * @param[in] conn Connection handle for disconnected connection
 * @param[in] reason Connection close reason
 */
static void bt_disconnected(struct bt_conn *conn, uint8_t reason)
{
	/* We are no longer connected */
	connected = false;

	/* Restart advertising */
	k_work_reschedule(&adv_update_work, ADV_UPDATE_DELAY);
}

/** @brief Delayed work handler for updating advertisement
 *
 * This work is schedule to update/start the BLE advertisement.
 *
 * @param[in] item Delayed work item
 */
static void adv_update_work_handler(struct k_work *item)
{
	int ret = 0;

	/* Update the flags in the advertisement */
	ad.flags = flags;

	/* Stop advertising if we're not connected */
	if (connected == false && advertising == true) {
		ret = bt_le_ext_adv_stop(adv);
		if (ret != 0) {
			LOG_ERR("adv_update_work_handler: could not stop advertising: %d", ret);
		}
	}

	/* Update advertising data */
	if (ret == 0) {
		ret = bt_le_ext_adv_set_data(adv, bt_ad, ARRAY_SIZE(bt_ad), bt_rsp,
					     ARRAY_SIZE(bt_rsp));
		if (ret != 0) {
			LOG_INF("adv_update_work_handler: could not update advertising data: %d",
				ret);
		}
	}

	/* Restart advertising if we're not connected */
	if (ret == 0) {
		if (connected == false) {
			ret = bt_le_ext_adv_start(adv, NULL);
			if (ret != 0) {
				LOG_ERR("adv_update_work_handler: could not restart advertising: %d",
					ret);
			} else {
				advertising = true;
			}
		}
	}

	/* If something went wrong above, reschedule this work to retry */
	if (ret != 0) {
		k_work_reschedule(&adv_update_work, ADV_RESTART_DELAY);
	}
}

/** @brief Callback for LwM2M data ready state change
 *
 * @param[in] data_ready true if LwM2M data needs to be sent, false if not
 */
static void lwm2m_data_ready(bool data_ready)
{
	/* Update the advertisement flag word */
	if (data_ready) {
		flags |= LWM2M_DATA_READY_FLAG;
	} else {
		flags &= ~LWM2M_DATA_READY_FLAG;
	}

	/* Schedule the update to the advertisement */
	k_work_reschedule(&adv_update_work, ADV_UPDATE_DELAY);
}
#endif

/**************************************************************************************************/
/* SYS INIT                                                                                       */
/**************************************************************************************************/
#if defined(CONFIG_ATTR)
SYS_INIT(ble_client_dm_device_ble_addr_init, APPLICATION, CONFIG_LCZ_BLE_CLIENT_DM_BLE_ADDR_INIT_PRIORITY);
static int ble_client_dm_device_ble_addr_init(const struct device *device)
{
	int ret = 0;
	size_t count = 1;
	bt_addr_le_t addr;
	char addr_str[BT_ADDR_LE_STR_LEN] = { 0 };
	char bd_addr[BT_ADDR_LE_STR_LEN];
	size_t size;

	size = attr_get_size(ATTR_ID_bluetooth_address);

	/* Fetch or generate the BLE address */
	bt_id_get(&addr, &count);
	if (count < 1) {
		LOG_DBG("Creating new address");
		bt_addr_le_copy(&addr, BT_ADDR_LE_ANY);
		ret = bt_id_create(&addr, NULL);
	}
	bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
	LOG_INF("Bluetooth Address: %s count: %d status: %d", addr_str, count, ret);

	/* remove ':' from default format */
	size_t i;
	size_t j;
	for (i = 0, j = 0; j < size - 1; i++) {
		if (addr_str[i] != ':') {
			bd_addr[j] = addr_str[i];
			if (bd_addr[j] >= 'A' && bd_addr[j] <= 'Z') {
				bd_addr[j] += ('a' - 'A');
			}
			j += 1;
		}
	}
	bd_addr[j] = 0;
	attr_set_string(ATTR_ID_bluetooth_address, bd_addr, size - 1);

	return ret;
}
#endif
