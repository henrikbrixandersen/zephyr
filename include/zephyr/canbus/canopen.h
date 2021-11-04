/*
 * Copyright (c) 2021-2023 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_CANOPEN_H_
#define ZEPHYR_INCLUDE_CANBUS_CANOPEN_H_

/**
 * @brief CANopen protocol support
 * @defgroup canopen CANopen
 * @ingroup CAN
 * @{
 */

#include <zephyr/canbus/canopen/nmt.h>
#include <zephyr/canbus/canopen/od.h>
#include <zephyr/canbus/canopen/sdo.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CANopen protocol stack initialisation
 * @defgroup canopen_init Initialisation
 * @ingroup canopen
 * @{
 */

/**
 * @brief Initialise the CANopen protocol stack
 *
 * Initialise the CANopen protocol stack.
 *
 * @param bitrate CAN bitrate in bits per second
 * @param node_id CANopen node-ID
 * @param od Pointer to the CANopen object dictionary
 *
 * @return Zero on success or negative error code otherwise.
 */
/* TODO: should bitrate be an enum? */
int canopen_init(uint32_t bitrate, uint8_t node_id, struct canopen_od *od);

/**
 * @brief Start the CANopen protocol stack
 *
 * Start the CANopen protocol stack. This will start the NMT finite-state
 * automaton (FSA) and start processing CANopen SDO writes.
 *
 * @return Zero on success or negative error code otherwise.
 */
int canopen_start(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_H_ */
