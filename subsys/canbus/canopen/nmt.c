/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/slist.h>

LOG_MODULE_REGISTER(canopen_nmt, CONFIG_CANOPEN_LOG_LEVEL);

/* NMT events (CiA 301, figure 48) */
enum canopen_nmt_event {
	/* Power on or hardware reset, transition (1) */
	CANOPEN_NMT_EVENT_POWER_ON,
	/* NMT service start node indication, transition (3),(6) */
	CANOPEN_NMT_EVENT_START,
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

static void canopen_nmt_state_initialisation_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Initialisation");

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_INITIALISATION);
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

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_RESET_APPLICATION);

	/* TODO: set manufacturer-specific and standardized device areas their power-on values */

	/* CiA 301, figure 49, transition (16) */
	smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_RESET_COMMUNICATION]);
}

static void canopen_nmt_state_reset_communication_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Reset communication");

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_RESET_COMMUNICATION);

	/* TODO: set communication profile area to its power-on values */

	/* CiA 301, figure 49, transition (2) */
	smf_set_state(SMF_CTX(nmt), &canopen_nmt_states[CANOPEN_NMT_STATE_PRE_OPERATIONAL]);
}

static void canopen_nmt_state_pre_operational_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Pre-operational");

	/* TODO: boot-up write */

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_PRE_OPERATIONAL);
}

static void canopen_nmt_state_pre_operational_run(void *obj)
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
		/* Event ignored */
	}
}

static void canopen_nmt_state_operational_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Operational");

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_OPERATIONAL);
}

static void canopen_nmt_state_operational_run(void *obj)
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
		/* Event ignored */
	}
}

static void canopen_nmt_state_stopped_entry(void *obj)
{
	struct canopen_nmt *nmt = obj;

	LOG_DBG("Stopped");

	canopen_nmt_fire_state_callbacks(nmt, CANOPEN_NMT_STATE_STOPPED);
}

static void canopen_nmt_state_stopped_run(void *obj)
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
		/* Event ignored */
	}
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

static void canopen_nmt_event_thread(void *p1, void *p2, void *p3)
{
	struct canopen_nmt *nmt = p1;
	canopen_nmt_event_t event;
	int err;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		err = k_msgq_get(&nmt->eventq, &event, K_FOREVER);
		if (err != 0) {
			LOG_ERR("failed to get event from queue (err %d)", err);
			break;
		}

		if (event == CANOPEN_NMT_EVENT_POWER_ON) {
			/* CiA 301, figure 48, transition (1) */
			smf_set_initial(SMF_CTX(nmt),
					&canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISATION]);
		} else {
			nmt->event = event;

			err = smf_run_state(SMF_CTX(nmt));
			if (err != 0) {
				break;
			}
		}
	}

	LOG_ERR("NMT finite-state machine terminated (err %d)", err);
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

int canopen_nmt_init(struct canopen_nmt *nmt, uint8_t node_id)
{
	k_tid_t tid;

	__ASSERT_NO_MSG(nmt != NULL);

	if (node_id < CANOPEN_NODE_ID_MIN || node_id > CANOPEN_NODE_ID_MAX) {
		LOG_ERR("invalid node-ID %d", node_id);
		return -EINVAL;
	}

	nmt->node_id = node_id;

	k_msgq_init(&nmt->eventq, nmt->eventq_buf, sizeof(canopen_nmt_event_t),
		    CONFIG_CANOPEN_NMT_EVENT_MSGQ_SIZE);

	tid = k_thread_create(&nmt->event_thread, nmt->event_stack,
			      K_KERNEL_STACK_SIZEOF(nmt->event_stack), canopen_nmt_event_thread,
			      (void *)nmt, NULL, NULL, CONFIG_CANOPEN_NMT_THREAD_PRIO, 0,
			      K_NO_WAIT);
	k_thread_name_set(tid, "canopen_nmt");

	/* Run initial state entry functions in the event thread context */
	return canopen_nmt_event_enqueue(nmt, CANOPEN_NMT_EVENT_POWER_ON);
}
