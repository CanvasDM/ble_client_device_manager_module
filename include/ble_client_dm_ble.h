/**
 * @file ble_client_dm_ble.h
 * @brief
 *
 * Copyright (c) 2022 Laird Connectivity LLC
 *
 * SPDX-License-Identifier: LicenseRef-LairdConnectivity-Clause
 */

#ifndef __BLE_CLIENT_DM_BLE_H__
#define __BLE_CLIENT_DM_BLE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************/
/* Global Function Prototypes                                                                     */
/**************************************************************************************************/
#if defined(CONFIG_LCZ_BLE_CLIENT_DM_ADVERTISE)
/**
 * @brief Start BLE advertisements
 *
 * This function starts BLE advertisements in a format that will allow a DM-enabled
 * gateway to discover this client/sensor and connect to it to receive LwM2M data
 * sent by the client/sensor.
 *
 * @return 0 on success, <0 on error
 */
int ble_client_dm_device_ble_advertise(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLE_CLIENT_DM_BLE_H__ */
