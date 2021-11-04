/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen.h>
#include <zephyr/device.h>
#include <zephyr/smf.h>

/* Forward declaration of the CANopen NMT states */
static const struct smf_state canopen_nmt_states[];

/* CANopen NMT finite-state automaton (FSA) context */
struct canopen_nmt_fsa {
	/* State Machine Framework context needs to be first */
	struct smf_ctx ctx;

	/* Associated CAN interface */
	const struct device *can;
};

/* Initialise a CANopen NMT FSA state */
#define CANOPEN_NMT_STATE_INIT(_state, _parent)                                                    \
	[CANOPEN_NMT_STATE_INITIALISATION] = SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL)

/* Populate the CANopen NMT FSA state table */
static const struct smf_state canopen_nmt_states[] = {
	CANOPEN_NMT_STATE_INIT(initialisation, NULL),
	CANOPEN_NMT_STATE_INIT(initialising, initialisation),
	CANOPEN_NMT_STATE_INIT(reset_application, initialisation),
	CANOPEN_NMT_STATE_INIT(reset_communication, initialisation),
	CANOPEN_NMT_STATE_INIT(pre_operational, NULL),
	CANOPEN_NMT_STATE_INIT(operational, NULL),
	CANOPEN_NMT_STATE_INIT(stopped, NULL),
};

/* Initialise a CANopen NMT FSA */
#define CANOPEN_NMT_FSA_INIT(_name, _chosen)                                                       \
	struct canopen_nmt_fsa canopen_nmt_fsa_##_name = {                                         \
		/*.can = DEVICE_DT_GET(DT_CHOSEN(_chosen)),*/                                      \
	};

/* Initialise the default CANopen NMT FSA */
CANOPEN_NMT_FSA_INIT(primary, zephyr_canbus)
