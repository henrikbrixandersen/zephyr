/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen.h>
#include <zephyr/device.h>
#include <zephyr/smf.h>

/* NMT events (CiA 301, figure 48) */
enum canopen_nmt_event {
	/* NMT service start (remote) node indication */
	CANOPEN_NMT_EVENT_START,
	/* NMT service enter pre-operational indication */
	CANOPEN_NMT_EVENT_ENTER_PRE_OPERATIONAL,
	/* NMT service stop (remote) node indication */
	CANOPEN_NMT_EVENT_STOP,
	/* NMT service reset node indication */
	CANOPEN_NMT_EVENT_RESET_NODE,
	/* NMT service reset communication indication */
	CANOPEN_NMT_EVENT_RESET_COMMUNICATION,
};

/* Forward declaration of the CANopen NMT states */
static const struct smf_state canopen_nmt_states[];

/* CANopen NMT finite-state automaton (FSA) context */
struct canopen_nmt_fsa {
	/* State Machine Framework context needs to be first */
	struct smf_ctx ctx;

	/* Associated CAN interface */
	const struct device *can;

	/* State change callback */
	canopen_nmt_state_callback_t callback;
	void *user_data;
};

/* Populate the CANopen NMT FSA state table */
static const struct smf_state canopen_nmt_states[] = {
	/* Initialisation parent state */
	[CANOPEN_NMT_STATE_INITIALISATION] = SMF_CREATE_STATE(
		NULL, NULL, NULL, NULL, &canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISING]),
	/* Initialising (Initialisation sub-state) */
	[CANOPEN_NMT_STATE_INITIALISING] = SMF_CREATE_STATE(
		NULL, NULL, NULL, &canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISATION], NULL),
	/* Reset Application (Initialisation sub-state) */
	[CANOPEN_NMT_STATE_RESET_APPLICATION] = SMF_CREATE_STATE(
		NULL, NULL, NULL, &canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISATION], NULL),
	/* Reset Communication (Initialisation sub-state) */
	[CANOPEN_NMT_STATE_RESET_COMMUNICATION] = SMF_CREATE_STATE(
		NULL, NULL, NULL, &canopen_nmt_states[CANOPEN_NMT_STATE_INITIALISATION], NULL),
	/* Pre-operational */
	[CANOPEN_NMT_STATE_PRE_OPERATIONAL] = SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL),
	/* Operational */
	[CANOPEN_NMT_STATE_OPERATIONAL] = SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL),
	/* Stopped */
	[CANOPEN_NMT_STATE_STOPPED] = SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL),
};

/* Initialise a CANopen NMT FSA */
#define CANOPEN_NMT_FSA_INIT(_name, _chosen)                                                       \
	struct canopen_nmt_fsa canopen_nmt_fsa_##_name = {                                         \
		/*.can = DEVICE_DT_GET(DT_CHOSEN(_chosen)),*/                                      \
	};

/* Initialise the default CANopen NMT FSA */
CANOPEN_NMT_FSA_INIT(primary, zephyr_canbus)
