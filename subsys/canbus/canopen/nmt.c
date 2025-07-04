/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/slist.h>

LOG_MODULE_REGISTER(canopen_nmt, CONFIG_CANOPEN_LOG_LEVEL);

/* NMT node control protocol */
#define CANOPEN_NMT_NODE_CONTROL_COB_ID                   0x0U
#define CANOPEN_NMT_NODE_CONTROL_DLC                      2U
#define CANOPEN_NMT_NODE_CONTROL_CS_START                 1U
#define CANOPEN_NMT_NODE_CONTROL_CS_STOP                  2U
#define CANOPEN_NMT_NODE_CONTROL_CS_ENTER_PRE_OPERATIONAL 128U
#define CANOPEN_NMT_NODE_CONTROL_CS_RESET_NODE            129U
#define CANOPEN_NMT_NODE_CONTROL_CS_RESET_COMMUNICATION   130U
#define CANOPEN_NMT_NODE_CONTROL_NODE_ID_ALL              0U

/* NMT error control protocol */
#define CANOPEN_NMT_ERROR_CONTROL_COB_ID_BASE 0x700U

/* NMT boot-up protocol */
#define CANOPEN_NMT_BOOT_UP_COB_ID_BASE CANOPEN_NMT_ERROR_CONTROL_COB_ID_BASE
#define CANOPEN_NMT_BOOT_UP_DLC         1U

enum canopen_nmt_state_internal {
	/* Internal state for performing the boot-up write */
	CANOPEN_NMT_STATE_INTERNAL_BOOT_UP_WRITE = CANOPEN_NMT_STATE_STOPPED + 1U,
};

/* NMT events (CiA 301, figure 48) */
enum canopen_nmt_event {
	/* Power on or hardware reset, transition (1) */
	CANOPEN_NMT_EVENT_POWER_ON,
	/* NMT service start node indication, transition (3),(6) */
	CANOPEN_NMT_EVENT_START,
	/* NMT boot-up write ACK received */
	CANOPEN_NMT_EVENT_BOOT_UP_WRITE_ACK,
	/* NMT boot-up write error */
	CANOPEN_NMT_EVENT_BOOT_UP_WRITE_ERROR,
	/* NMT service enter pre-operational indication, transitions (4),(7) */
	CANOPEN_NMT_EVENT_ENTER_PRE_OPERATIONAL,
	/* NMT service stop node indication, transitions (5),(8) */
	CANOPEN_NMT_EVENT_STOP,
	/* NMT service reset node indication, transitions (9),(10),(11) */
	CANOPEN_NMT_EVENT_RESET_NODE,
	/* NMT service reset communication indication, transitions (12),(13),(14) */
	CANOPEN_NMT_EVENT_RESET_COMMUNICATION,
};

static const struct smf_state canopen_nmt_states[];

const char *canopen_nmt_state_str(enum canopen_nmt_state state)
{
	switch (state) {
	case CANOPEN_NMT_STATE_INITIALISATION:
		return "Initialisation";
	case CANOPEN_NMT_STATE_INITIALISING:
		return "Initialising";
	case CANOPEN_NMT_STATE_RESET_APPLICATION:
		return "Reset application";
	case CANOPEN_NMT_STATE_RESET_COMMUNICATION:
		return "Reset communication";
	case CANOPEN_NMT_STATE_PRE_OPERATIONAL:
		return "Pre-operational";
	case CANOPEN_NMT_STATE_OPERATIONAL:
		return "Operational";
	case CANOPEN_NMT_STATE_STOPPED:
		return "Stopped";
	default:
		return "(Unknown)";
	}
}

static void canopen_nmt_fire_state_callbacks(struct canopen_nmt *nmt, enum canopen_nmt_state state)
{
	struct canopen_nmt_state_callback *tmp;
	struct canopen_nmt_state_callback *cb;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&nmt->state_callbacks, cb, tmp, node) {
		__ASSERT_NO_MSG(cb->handler != NULL);

		cb->handler(nmt, cb, state, nmt->node_id);
	}
}

static int canopen_nmt_event_enqueue(struct canopen_nmt *nmt, canopen_nmt_event_t event)
{
	int err;

	err = k_msgq_put(&nmt->eventq, &event, K_NO_WAIT);
	if (err != 0) {
		LOG_ERR("failed to enqueue event %u (err %d)", event, err);
		return err;
	}

	return 0;
}

static void canopen_nmt_boot_up_write_tx_callback(const struct device *dev, int error,
						  void *user_data)
{
	struct canopen_nmt *nmt = user_data;

	if (error == 0) {
		(void)canopen_nmt_event_enqueue(nmt, CANOPEN_NMT_EVENT_BOOT_UP_WRITE_ACK);
	} else {
		LOG_WRN("failed to perform boot-up write (err %d)", error);
		(void)canopen_nmt_event_enqueue(nmt, CANOPEN_NMT_EVENT_BOOT_UP_WRITE_ERROR);
	}
}

static void canopen_nmt_state_initialisation_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Initialisation");

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_INITIALISATION);

	/* TODO: call can_stop() */
}

static void canopen_nmt_state_initialising_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Initialising");

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_INITIALISING);

	/* CiA 301, figure 49, transition (15) */
	smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_APPLICATION]);
}

static void canopen_nmt_state_reset_application_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Reset application");

	/* TODO: set manufacturer-specific (2000h to 5FFFh) and standardized device profile */
	/* (6000h to 9FFFh) areas to their power-on values */

	/* Iterate all OD entries and fixup all relative entries */

	/* TODO: set node-ID and bitrate settings to their power-on values. */

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_RESET_APPLICATION);

	/* CiA 301, figure 49, transition (16) */
	smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_COMMUNICATION]);
}

static void canopen_nmt_state_reset_communication_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Reset communication");

	/* TODO: set communication profile area (1000h to 1FFFh) to its power-on values */

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_RESET_COMMUNICATION);

	/* TODO: re-configure CAN */

	/* CiA 301, figure 49, transition (2), part 1 of 2 */
	smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_INTERNAL_BOOT_UP_WRITE]);
}

static void canopen_nmt_state_internal_boot_up_write_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;
	struct can_frame frame = {0};
	int err;

	frame.id = CANOPEN_NMT_BOOT_UP_COB_ID_BASE + nmt->node_id;
	frame.dlc = 1;

	/* TODO: forever? */
	err = can_send(nmt->can, &frame, K_FOREVER, canopen_nmt_boot_up_write_tx_callback, nmt);
	if (err != 0) {
		LOG_ERR("failed to enqueue boot-up CAN frame (err %d)", err);
		/* TODO: set new state or complete this state change? */
	}
}

static enum smf_state_result canopen_nmt_state_internal_boot_up_write_run(void *obj)
{
	struct canopen_nmt *nmt = obj;

	switch (nmt->event) {
	case CANOPEN_NMT_EVENT_BOOT_UP_WRITE_ACK:
		/* CiA 301, figure 49, transition (2), part 2 of 2 */
		smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_PRE_OPERATIONAL]);
		break;
	case CANOPEN_NMT_EVENT_BOOT_UP_WRITE_ERROR:
		/* TODO: retry by transitioning to self? with delay? transition back to init? */
		break;
	case CANOPEN_NMT_EVENT_RESET_NODE:
		/* Allow aborting pending boot-up write ACK by local node control */
		smf_set_state(SMF_CTX(nmt),
			      &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_APPLICATION]);
		break;
	case CANOPEN_NMT_EVENT_RESET_COMMUNICATION:
		/* Allow aborting pending boot-up write ACK by local node control */
		smf_set_state(SMF_CTX(nmt),
			      &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_COMMUNICATION]);
		break;
	default:
		return SMF_EVENT_PROPAGATE;
	}

	return SMF_EVENT_HANDLED;
}

static void canopen_nmt_state_pre_operational_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Pre-operational");

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_PRE_OPERATIONAL);
}

static enum smf_state_result canopen_nmt_state_pre_operational_run(void *obj)
{
	struct canopen_nmt *nmt = obj;

	switch (nmt->event) {
	case CANOPEN_NMT_EVENT_START:
		/* CiA 301, figure 48, transition (3) */
		smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_OPERATIONAL]);
		break;
	case CANOPEN_NMT_EVENT_STOP:
		/* CiA 301, figure 48, transition (5) */
		smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_STOPPED]);
		break;
	case CANOPEN_NMT_EVENT_RESET_NODE:
		/* CiA 301, figure 48, transition (11) */
		smf_set_state(SMF_CTX(nmt),
			      &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_APPLICATION]);
		break;
	case CANOPEN_NMT_EVENT_RESET_COMMUNICATION:
		/* CiA 301, figure 48, transition (14) */
		smf_set_state(SMF_CTX(nmt),
			      &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_COMMUNICATION]);
		break;
	default:
		return SMF_EVENT_PROPAGATE;
	}

	return SMF_EVENT_HANDLED;
}

static void canopen_nmt_state_operational_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Operational");

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_OPERATIONAL);
}

static enum smf_state_result canopen_nmt_state_operational_run(void *obj)
{
	struct canopen_nmt *nmt = obj;

	switch (nmt->event) {
	case CANOPEN_NMT_EVENT_ENTER_PRE_OPERATIONAL:
		/* CiA 301, figure 48, transition (4) */
		smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_PRE_OPERATIONAL]);
		break;
	case CANOPEN_NMT_EVENT_STOP:
		/* CiA 301, figure 48, transition (8) */
		smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_STOPPED]);
		break;
	case CANOPEN_NMT_EVENT_RESET_NODE:
		/* CiA 301, figure 48, transition (9) */
		smf_set_state(SMF_CTX(nmt),
			      &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_APPLICATION]);
		break;
	case CANOPEN_NMT_EVENT_RESET_COMMUNICATION:
		/* CiA 301, figure 48, transition (12) */
		smf_set_state(SMF_CTX(nmt),
			      &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_COMMUNICATION]);
		break;
	default:
		return SMF_EVENT_PROPAGATE;
	}

	return SMF_EVENT_HANDLED;
}

static void canopen_nmt_state_stopped_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Stopped");

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_STOPPED);
}

static enum smf_state_result canopen_nmt_state_stopped_run(void *obj)
{
	struct canopen_nmt *nmt = obj;

	switch (nmt->event) {
	case CANOPEN_NMT_EVENT_START:
		/* CiA 301, figure 48, transition (6) */
		smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_OPERATIONAL]);
		break;
	case CANOPEN_NMT_EVENT_ENTER_PRE_OPERATIONAL:
		/* CiA 301, figure 48, transition (7) */
		smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_PRE_OPERATIONAL]);
		break;
	case CANOPEN_NMT_EVENT_RESET_NODE:
		/* CiA 301, figure 48, transition (10) */
		smf_set_state(SMF_CTX(nmt),
			      &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_APPLICATION]);
		break;
	case CANOPEN_NMT_EVENT_RESET_COMMUNICATION:
		/* CiA 301, figure 48, transition (13) */
		smf_set_state(SMF_CTX(nmt),
			      &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_COMMUNICATION]);
		break;
	default:
		return SMF_EVENT_PROPAGATE;
	}

	return SMF_EVENT_HANDLED;
}

/* Populate the CANopen NMT FSA state table */
static const struct smf_state canopen_nmt_states[] = {
	/* Initialisation parent state */
	[CANOPEN_NMT_STATE_INITIALISATION] =
		SMF_CREATE_STATE(canopen_nmt_state_initialisation_entry, NULL, NULL, NULL,
				 &canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISING]),
	/* Initialising (Initialisation sub-state) */
	[CANOPEN_NMT_STATE_INITIALISING] =
		SMF_CREATE_STATE(canopen_nmt_state_initialising_entry, NULL, NULL,
				 &canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISATION], NULL),
	/* Reset Application (Initialisation sub-state) */
	[CANOPEN_NMT_STATE_RESET_APPLICATION] =
		SMF_CREATE_STATE(canopen_nmt_state_reset_application_entry, NULL, NULL,
				 &canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISATION], NULL),
	/* Reset Communication (Initialisation sub-state) */
	[CANOPEN_NMT_STATE_RESET_COMMUNICATION] =
		SMF_CREATE_STATE(canopen_nmt_state_reset_communication_entry, NULL, NULL,
				 &canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISATION], NULL),
	/* Boot-up write (internal sub-state) */
	[CANOPEN_NMT_STATE_INTERNAL_BOOT_UP_WRITE] =
		SMF_CREATE_STATE(canopen_nmt_state_internal_boot_up_write_entry,
				 canopen_nmt_state_internal_boot_up_write_run, NULL, NULL, NULL),
	/* Pre-operational */
	[CANOPEN_NMT_STATE_PRE_OPERATIONAL] =
		SMF_CREATE_STATE(canopen_nmt_state_pre_operational_entry,
				 canopen_nmt_state_pre_operational_run, NULL, NULL, NULL),
	/* Operational */
	[CANOPEN_NMT_STATE_OPERATIONAL] =
		SMF_CREATE_STATE(canopen_nmt_state_operational_entry,
				 canopen_nmt_state_operational_run, NULL, NULL, NULL),
	/* Stopped */
	[CANOPEN_NMT_STATE_STOPPED] = SMF_CREATE_STATE(
		canopen_nmt_state_stopped_entry, canopen_nmt_state_stopped_run, NULL, NULL, NULL),
};

int canopen_nmt_enable(struct canopen_nmt *nmt)
{
	__ASSERT_NO_MSG(nmt != NULL);

	return canopen_nmt_event_enqueue(nmt, CANOPEN_NMT_EVENT_POWER_ON);
}

int canopen_nmt_reset_node(struct canopen_nmt *nmt)
{
	__ASSERT_NO_MSG(nmt != NULL);

	return canopen_nmt_event_enqueue(nmt, CANOPEN_NMT_EVENT_RESET_NODE);
}

int canopen_nmt_reset_communication(struct canopen_nmt *nmt)
{
	__ASSERT_NO_MSG(nmt != NULL);

	return canopen_nmt_event_enqueue(nmt, CANOPEN_NMT_EVENT_RESET_COMMUNICATION);
}

int canopen_nmt_enter_pre_operational(struct canopen_nmt *nmt)
{
	__ASSERT_NO_MSG(nmt != NULL);

	return canopen_nmt_event_enqueue(nmt, CANOPEN_NMT_EVENT_ENTER_PRE_OPERATIONAL);
}

int canopen_nmt_start(struct canopen_nmt *nmt)
{
	__ASSERT_NO_MSG(nmt != NULL);

	return canopen_nmt_event_enqueue(nmt, CANOPEN_NMT_EVENT_START);
}

int canopen_nmt_stop(struct canopen_nmt *nmt)
{
	__ASSERT_NO_MSG(nmt != NULL);

	return canopen_nmt_event_enqueue(nmt, CANOPEN_NMT_EVENT_STOP);
}

static void canopen_nmt_node_control_callback(const struct device *can, struct can_frame *frame,
					      void *user_data)
{
	struct canopen_nmt *nmt = user_data;
	uint8_t node_id;
	uint8_t cs;
	int err = 0;

	ARG_UNUSED(can);

	if (frame->dlc != CANOPEN_NMT_NODE_CONTROL_DLC) {
		/* Non-compliant frame length, ignore */
		return;
	}

	cs = frame->data[0];
	node_id = frame->data[1];

	if ((node_id != CANOPEN_NMT_NODE_CONTROL_NODE_ID_ALL) && (node_id != nmt->node_id)) {
		/* Non-matching node-ID, ignore */
		return;
	}

	switch (frame->data[0]) {
	case CANOPEN_NMT_NODE_CONTROL_CS_START:
		err = canopen_nmt_start(nmt);
		break;
	case CANOPEN_NMT_NODE_CONTROL_CS_STOP:
		err = canopen_nmt_stop(nmt);
		break;
	case CANOPEN_NMT_NODE_CONTROL_CS_ENTER_PRE_OPERATIONAL:
		err = canopen_nmt_enter_pre_operational(nmt);
		break;
	case CANOPEN_NMT_NODE_CONTROL_CS_RESET_NODE:
		err = canopen_nmt_reset_node(nmt);
		break;
	case CANOPEN_NMT_NODE_CONTROL_CS_RESET_COMMUNICATION:
		err = canopen_nmt_reset_communication(nmt);
		break;
	default:
		/* Unknown command specifier, ignore */
	}

	if (err != 0) {
		LOG_ERR("failed to enqueue remote node control command specifier %d (err %d)", cs,
			err);
	}
}

static void canopen_nmt_eventq_triggered_work_handler(struct k_work *work)
{
	struct k_work_poll *pwork = CONTAINER_OF(work, struct k_work_poll, work);
	struct canopen_nmt *nmt = CONTAINER_OF(pwork, struct canopen_nmt, eventq_work);
	canopen_nmt_event_t event;
	int err;

	err = k_msgq_get(&nmt->eventq, &event, K_FOREVER);
	if (err != 0) {
		LOG_ERR("failed to get event from queue (err %d)", err);
		goto resubmit;
	}

	if (event == CANOPEN_NMT_EVENT_POWER_ON) {
		/* CiA 301, figure 48, transition (1) */
		smf_set_initial(SMF_CTX(nmt),
				&canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISATION]);
	} else {
		nmt->event = event;

		err = smf_run_state(SMF_CTX(nmt));
		if (err != 0) {
			LOG_ERR("NMT finite-state machine terminated (err %d)", err);
			goto resubmit;
		}
	}

resubmit:
	err = k_work_poll_submit_to_queue(nmt->work_q, &nmt->eventq_work, nmt->eventq_poll_events,
					  ARRAY_SIZE(nmt->eventq_poll_events), K_FOREVER);
	if (err != 0) {
		LOG_ERR("failed to re-submit event queue polling (err %d)", err);
	}
}

int canopen_nmt_add_state_callback(struct canopen_nmt *nmt,
				   struct canopen_nmt_state_callback *callback)
{
	k_spinlock_key_t key;

	__ASSERT_NO_MSG(nmt != NULL);
	__ASSERT_NO_MSG(callback != NULL);
	__ASSERT_NO_MSG(callback->handler != NULL);

	key = k_spin_lock(&nmt->callback_lock);

	(void)sys_slist_find_and_remove(&nmt->state_callbacks, &callback->node);
	sys_slist_append(&nmt->state_callbacks, &callback->node);

	k_spin_unlock(&nmt->callback_lock, key);

	return 0;
}

int canopen_nmt_remove_state_callback(struct canopen_nmt *nmt,
				      struct canopen_nmt_state_callback *callback)
{
	k_spinlock_key_t key;
	int err = 0;

	__ASSERT_NO_MSG(nmt != NULL);
	__ASSERT_NO_MSG(callback != NULL);

	key = k_spin_lock(&nmt->callback_lock);

	if (sys_slist_is_empty(&nmt->state_callbacks)) {
		err = -EINVAL;
		goto unlock;
	}

	if (!sys_slist_find_and_remove(&nmt->state_callbacks, &callback->node)) {
		err = -EINVAL;
		goto unlock;
	}

unlock:
	k_spin_unlock(&nmt->callback_lock, key);

	return err;
}

int canopen_nmt_init(struct canopen_nmt *nmt, struct k_work_q *work_q, const struct device *can,
		     uint8_t node_id)
{
	struct can_filter filter;
	int err;

	__ASSERT_NO_MSG(nmt != NULL);
	__ASSERT_NO_MSG(work_q != NULL);

	if (node_id < CANOPEN_NODE_ID_MIN || node_id > CANOPEN_NODE_ID_MAX) {
		LOG_ERR("invalid node-ID %d", node_id);
		return -EINVAL;
	}

	nmt->work_q = work_q;
	nmt->can = can;
	nmt->node_id = node_id;

	k_msgq_init(&nmt->eventq, nmt->eventq_buf, sizeof(canopen_nmt_event_t),
		    CONFIG_CANOPEN_NMT_EVENT_MSGQ_SIZE);

	k_poll_event_init(&nmt->eventq_poll_events[0], K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
			  K_POLL_MODE_NOTIFY_ONLY, &nmt->eventq);

	k_work_poll_init(&nmt->eventq_work, canopen_nmt_eventq_triggered_work_handler);

	err = k_work_poll_submit_to_queue(nmt->work_q, &nmt->eventq_work, nmt->eventq_poll_events,
					  ARRAY_SIZE(nmt->eventq_poll_events), K_FOREVER);
	if (err != 0) {
		LOG_ERR("failed to submit event queue polling (err %d)", err);
		return -EIO;
	}

	canopen_cob_id_to_can_filter(CANOPEN_NMT_NODE_CONTROL_COB_ID, &filter);
	err = can_add_rx_filter(nmt->can, canopen_nmt_node_control_callback, nmt, &filter);
	if (err < 0) {
		LOG_ERR("failed to add CANopen NMT CAN filter (err %d)", err);
		return -EIO;
	}

	return 0;
}
