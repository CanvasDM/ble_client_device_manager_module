/**
 * @file memfault_task.c
 * @brief
 *
 * Copyright (c) 2022 Laird Connectivity LLC
 *
 * SPDX-License-Identifier: LicenseRef-LairdConnectivity-Clause
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(memfault_task, CONFIG_LCZ_BLE_CLIENT_DM_LOG_LEVEL);

/**************************************************************************************************/
/* Includes                                                                                       */
/**************************************************************************************************/
#include <zephyr/zephyr.h>
#include <memfault_ncs.h>
#if defined(CONFIG_ATTR)
#include <attr.h>
#endif
#include <lcz_memfault.h>
#include <file_system_utilities.h>

#include "memfault_task.h"
#include "ble_client_dm_ble.h"

/**************************************************************************************************/
/* Local Constant, Macro and Type Definitions                                                     */
/**************************************************************************************************/
#define MEMFAULT_DATA_FILE_PATH CONFIG_FSU_MOUNT_POINT "/" CONFIG_LCZ_BLE_CLIENT_DM_MEMFAULT_FILE_NAME

/**************************************************************************************************/
/* Local Data Definitions                                                                         */
/**************************************************************************************************/
static struct k_timer report_data_timer;
static uint8_t chunk_buf[CONFIG_LCZ_BLE_CLIENT_DM_MEMFAULT_CHUNK_BUF_SIZE];
static ble_client_dm_ble_memfault_data_ready_cb_t memfault_cb;

/**************************************************************************************************/
/* Global Data Definitions                                                                        */
/**************************************************************************************************/
extern const k_tid_t memfault;

/**************************************************************************************************/
/* Local Function Prototypes                                                                      */
/**************************************************************************************************/
static void report_data_timer_expired(struct k_timer *timer_id);

/**************************************************************************************************/
/* Local Function Definitions                                                                     */
/**************************************************************************************************/
static void report_data_timer_expired(struct k_timer *timer_id)
{
	(void)lcz_ble_client_dm_memfault_post_data();
}

static void memfault_thread(void *arg1, void *arg2, void *arg3)
{
	char *dev_id;
	size_t file_size;
	bool has_coredump;
	bool delete_file;
	int r;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	dev_id = (char *)attr_get_quasi_static(ATTR_ID_device_id);

	memfault_ncs_device_id_set(dev_id, strlen(dev_id));

	k_timer_init(&report_data_timer, report_data_timer_expired, NULL);

	k_timer_start(&report_data_timer,
		      K_SECONDS(CONFIG_LCZ_BLE_CLIENT_DM_MEMFAULT_REPORT_PERIOD_SECONDS),
		      K_SECONDS(CONFIG_LCZ_BLE_CLIENT_DM_MEMFAULT_REPORT_PERIOD_SECONDS));

	while (true) {
		k_thread_suspend(memfault);

		LOG_INF("Saving Memfault data...");
		if (fsu_get_file_size_abs(MEMFAULT_DATA_FILE_PATH) >=
			CONFIG_LCZ_BLE_CLIENT_DM_MEMFAULT_FILE_MAX_SIZE_BYTES) {
			delete_file = true;
		} else {
			delete_file = false;
		}
		r = lcz_memfault_save_data_to_file(MEMFAULT_DATA_FILE_PATH, chunk_buf,
							sizeof(chunk_buf), delete_file, true,
							&file_size, &has_coredump);

		if (!r) {
			LOG_INF("Memfault data saved!");
		} else {
			LOG_ERR("Memfault data save failed [%d]!", r);
		}

		if (memfault_cb){
			memfault_cb((bool)(file_size > 0));
		}

		/* Reset timer each time data is sent */
		k_timer_start(&report_data_timer,
			      K_SECONDS(CONFIG_LCZ_BLE_CLIENT_DM_MEMFAULT_REPORT_PERIOD_SECONDS),
			      K_SECONDS(CONFIG_LCZ_BLE_CLIENT_DM_MEMFAULT_REPORT_PERIOD_SECONDS));
	}
}

/**************************************************************************************************/
/* Global Function Definitions                                                                    */
/**************************************************************************************************/
int lcz_ble_client_dm_memfault_post_data(void)
{
	k_thread_resume(memfault);
	return 0;
}

void ble_client_dm_register_memfault_data_ready_cb(ble_client_dm_ble_memfault_data_ready_cb_t cb)
{
	memfault_cb = cb;
}

K_THREAD_DEFINE(memfault, CONFIG_LCZ_BLE_CLIENT_DM_MEMFAULT_THREAD_STACK_SIZE, memfault_thread, NULL,
		NULL, NULL, K_PRIO_PREEMPT(CONFIG_LCZ_BLE_CLIENT_DM_MEMFAULT_THREAD_PRIORITY), 0, 0);
