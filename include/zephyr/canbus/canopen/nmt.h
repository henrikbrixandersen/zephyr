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
 * @defgroup canopen_nmt Network Management (NMT)
 * @ingroup canopen
 * @{
 */

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/slist.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CANopen NMT states
 *
 * @see CiA 301, figure 48
 */
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

/** @cond INTERNAL_HIDDEN */
/**
 * @brief Internal representation of a CANopen NMT state machine event
 */
typedef uint8_t canopen_nmt_event_t;
/** @endcond */

struct canopen_nmt;
struct canopen_nmt_state_callback;

/**
 * @typedef canopen_nmt_state_callback_handler_t
 * @brief Callback for notifying about CANopen NMT state changes

 * @note The @a cb pointer can be used to retrieve private data through CONTAINER_OF() if the
 * original canopen_nmt_state_callback_handler_t struct is stored within another private structure.
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @param cb Pointer to the CANopen NMT state callback structure.
 * @param state the new CANopen NMT state.
 * @param node_id the current CANopen node-ID.
 */
typedef void (*canopen_nmt_state_callback_handler_t)(struct canopen_nmt *nmt,
						     struct canopen_nmt_state_callback *cb,
						     enum canopen_nmt_state state, uint8_t node_id);

/** @brief CANopen Network Management (NMT) state change callback.
 *
 * This struct is used to register a state callback in a CANopen NMT object callback list. As
 * many callbacks as needed can be added as long as each of them are unique pointers of struct
 * canopen_nmt_state_callback.
 *
 * @note This structure should not be allocated on a stack.
 *
 * This type is opaque. Member data should not be accessed directly by the application.
 *
 * @see canopen_nmt_init_state_callback()
 * @see canopen_nmt_add_state_callback()
 * @see canopen_nmt_remove_state_callback()
 */
struct canopen_nmt_state_callback {
	/** Single-linked list node */
	sys_snode_t node;
	/** Callback function */
	canopen_nmt_state_callback_handler_t handler;
};

/** @brief CANopen Network Management (NMT) object
 *
 * This type is opaque. Member data should not be accessed directly by the application.
 */
struct canopen_nmt {
	/** State machine framework context (needs to be first). */
	struct smf_ctx ctx;
	/** Associated CAN interface. */
	const struct device *can;
	/** Current node-ID. */
	uint8_t node_id;
	/** Lock for changing the list of callbacks. */
	struct k_spinlock callback_lock;
	/** List of state change callbacks. */
	sys_slist_t state_callbacks;
	/** Event queue buffer. */
	char eventq_buf[sizeof(canopen_nmt_event_t) * CONFIG_CANOPEN_NMT_EVENT_MSGQ_SIZE];
	/** Current event. */
	canopen_nmt_event_t event;
	/** Event queue. */
	struct k_msgq eventq;
	/** State machine processing work queue. */
	struct k_work_q *work_q;
	/** Event queue processing work queue item. */
	struct k_work_poll eventq_work;
	/** Event queue polling events. */
	struct k_poll_event eventq_poll_events[1];
};

/**
 * @brief Initialize CANopen NMT object
 *
 * The CANopen NMT object must be initialized prior to calling any other CANopen NMT API functions.
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @param work_q Pointer to the work queue to be used by the CANopen NMT FSA.
 * @param can Pointer to the CAN controller device instance.
 * @param node_id CANopen node-ID (1 to 127).
 * @retval 0 on success.
 * @retval -EINVAL if the provided node-ID is invalid.
 * @retval -EIO if configuration of the CAN device failed.
 */
int canopen_nmt_init(struct canopen_nmt *nmt, struct k_work_q *work_q, const struct device *can,
		     uint8_t node_id);

/**
 * @brief Enable CANopen NMT object
 *
 * Enable the CANopen NMT object.
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @retval 0 on success.
 * @return 0 on success, negative errno otherwise
 */
int canopen_nmt_enable(struct canopen_nmt *nmt);

/**
 * @brief Initialize a CANopen NMT state callback
 *
 * @param callback Pointer to the CANopen NMT state callback
 * @param handler CANopen NMT state callback handler function
 */
static inline void canopen_nmt_init_state_callback(struct canopen_nmt_state_callback *callback,
						   canopen_nmt_state_callback_handler_t handler)
{
	__ASSERT_NO_MSG(callback != NULL);
	__ASSERT_NO_MSG(handler != NULL);

	callback->handler = handler;
}

/**
 * @brief Add CANopen NMT state change callback
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @param callback Pointer to the CANopen NMT state change callback.
 * @retval 0 on success.
 */
int canopen_nmt_add_state_callback(struct canopen_nmt *nmt,
				   struct canopen_nmt_state_callback *callback);

/**
 * @brief Remove CANopen NMT state change callback
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @param callback Pointer to the CANopen NMT state change callback.
 * @retval 0 on success.
 * @retval -EINVAL if callback was not found.
 */
int canopen_nmt_remove_state_callback(struct canopen_nmt *nmt,
				      struct canopen_nmt_state_callback *callback);

/**
 * @brief Reset local node
 *
 * Enqueue an event for the local CANopen NMT object to asynchronously enter the @a
 * CANOPEN_NMT_STATE_RESET_APPLICATION state.
 *
 * @note The event will be ignored if it would result in an invalid state transition.
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @return 0 on success, negative errno otherwise
 */
int canopen_nmt_reset_node(struct canopen_nmt *nmt);

/**
 * @brief Reset local communication
 *
 * Enqueue an event for the local CANopen NMT object to asynchronously enter the @a
 * CANOPEN_NMT_STATE_RESET_COMMUNICATION state.
 *
 * @note The event will be ignored if it would result in an invalid state transition.
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @return 0 on success, negative errno otherwise
 */
int canopen_nmt_reset_communication(struct canopen_nmt *nmt);

/**
 * @brief Enter local pre-operational state
 *
 * Enqueue an event for the local CANopen NMT object to asynchronously enter the @a
 * CANOPEN_NMT_STATE_PRE_OPERATIONAL state.
 *
 * @note The event will be ignored if it would result in an invalid state transition.
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @return 0 on success, negative errno otherwise
 */
int canopen_nmt_enter_pre_operational(struct canopen_nmt *nmt);

/**
 * @brief Enter local operational state
 *
 * Enqueue an event for the local CANopen NMT object to asynchronously enter the @a
 * CANOPEN_NMT_STATE_OPERATIONAL state.
 *
 * @note The event will be ignored if it would result in an invalid state transition.
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @return 0 on success, negative errno otherwise
 */
int canopen_nmt_start(struct canopen_nmt *nmt);

/**
 * @brief Enter local stopped state
 *
 * Enqueue an event for the local CANopen NMT object to asynchronously enter the @a
 * CANOPEN_NMT_STATE_STOPPED state.
 *
 * @note The event will be ignored if it would result in an invalid state transition.
 *
 * @param nmt Pointer to the CANopen NMT object.
 * @return 0 on success, negative errno otherwise
 */
int canopen_nmt_stop(struct canopen_nmt *nmt);

/**
 * @brief Get the description of a CANopen NMT state
 *
 * @param state CANopen NMT state which description should be returned.
 * @return String describing the provided state.
 */
const char *canopen_nmt_state_str(enum canopen_nmt_state state);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_NMT_H_ */
