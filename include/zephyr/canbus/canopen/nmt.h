/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_CANOPEN_NMT_H_
#define ZEPHYR_INCLUDE_CANBUS_CANOPEN_NMT_H_

/**
 * @file canbus/canopen/nmt.h
 * @brief Network Management (NMT)
 * @defgroup canopen_nmt Network Management
 * @ingroup canopen
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** NMT states (CiA 301, figure 48) */
enum canopen_nmt_state {
	/**
	 * @brief Initialisation.
	 */
	CANOPEN_NMT_STATE_INITIALISATION,
	/**
	 * @brief Initialising (Initialisation sub-state).
	 */
	CANOPEN_NMT_STATE_INITIALISING,
	/**
	 * @brief Reset Application (Initialisation sub-state).
	 */
	CANOPEN_NMT_STATE_RESET_APPLICATION,
	/**
	 * @brief Reset Communication (Initialisation sub-state).
	 */
	CANOPEN_NMT_STATE_RESET_COMMUNICATION,
	/**
	 * @brief Pre-operational.
	 */
	CANOPEN_NMT_STATE_PRE_OPERATIONAL,
	/**
	 * @brief Operational.
	 */
	CANOPEN_NMT_STATE_OPERATIONAL,
	/**
	 * @brief Stopped.
	 */
	CANOPEN_NMT_STATE_STOPPED,
};

/**
 * @typedef canopen_nmt_callback_t
 * @brief Callback for notifying the application of CANopen NMT state changes.
 *
 * @param state the new CANopen NMT state.
 */
typedef void (*canopen_nmt_state_callback_t)(enum canopen_nmt_state state, void *user_data);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_OD_H_ */
