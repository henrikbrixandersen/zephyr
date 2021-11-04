/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @ingroup canopen_iface
 * @brief CANopen Interface
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_CANOPEN_IFACE_H_
#define ZEPHYR_INCLUDE_CANBUS_CANOPEN_IFACE_H_

/**
 * @brief CANopen Interface
 * @defgroup canopen_iface CANopen Interface
 * @ingroup canopen
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*canopen_iface_add_rx_filter)(uint32_t cob_id, uint8_t priority);

typedef int (*canopen_iface_)();

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_IFACE_H_ */
