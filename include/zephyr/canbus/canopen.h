/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_CANOPEN_H_
#define ZEPHYR_INCLUDE_CANBUS_CANOPEN_H_

/**
 * @brief CANopen protocol support
 * @defgroup canopen CANopen
 * @ingroup connectivity
 * @{
 */

#include <string.h>

#include <zephyr/canbus/canopen/nmt.h>
#include <zephyr/canbus/canopen/od.h>
#include <zephyr/canbus/canopen/sdo.h>
#include <zephyr/drivers/can.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name CANopen initialization
 * @{
 */

struct canopen {
	struct canopen_od *od;
	struct canopen_nmt nmt;
};

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
int canopen_init(struct canopen *co, uint32_t bitrate, uint8_t node_id, struct canopen_od *od);

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

/**
 * @name CANopen utilities
 * @{
 */

/**
 * @brief Minimum allowed value for a CANopen node-ID
 */
#define CANOPEN_NODE_ID_MIN 1

/**
 * @brief Maximum allowed value for a CANopen node-ID
 */
#define CANOPEN_NODE_ID_MAX 127

/**
 * @brief CANopen COB-ID frame bit
 */
#define CANOPEN_COB_ID_FRAME BIT(29)

/**
 * @brief Fill in a @a struct can_filter to match a given CANopen COB-ID.
 *
 * @param cob_id the CANopen COB-ID.
 * @param filter the CAN RX filter.
 */
static inline void canopen_cob_id_to_can_filter(uint32_t cob_id, struct can_filter *filter)
{
	uint32_t mask = CAN_STD_ID_MASK;

	memset(filter, 0, sizeof(*filter));

	if ((cob_id & CANOPEN_COB_ID_FRAME) != 0U) {
		mask = CAN_EXT_ID_MASK;
		filter->flags = CAN_FILTER_IDE;
	}

	filter->id = cob_id & mask;
	filter->mask = mask;
}

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
