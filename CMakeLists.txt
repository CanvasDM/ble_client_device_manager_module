#
# Copyright (c) 2022 Laird Connectivity LLC
#
# SPDX-License-Identifier: LicenseRef-LairdConnectivity-Clause
#

if(CONFIG_LCZ_BLE_CLIENT_DM)

zephyr_include_directories(include)
zephyr_sources(
    src/lcz_ble_client_dm.c
    src/ble_client_dm_ble.c)
zephyr_sources_ifdef(CONFIG_FSU_ENCRYPTED_FILES src/lcz_ble_client_dm_file_rules.c)
zephyr_sources_ifdef(CONFIG_MCUMGR src/lcz_ble_client_dm_smp_rules.c)
zephyr_sources_ifdef(CONFIG_ATTR src/ble_client_dm_device_id_init.c)

endif()
