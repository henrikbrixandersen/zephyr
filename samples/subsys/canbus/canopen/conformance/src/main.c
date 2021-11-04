/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "objdict.h"

LOG_MODULE_REGISTER(app, CONFIG_CANOPEN_LOG_LEVEL);

struct canopen co;
struct canopen_sdo_server sdo_server;
struct canopen_nmt_state_callback cb;

static void state_callback(struct canopen_nmt *nmt, struct canopen_nmt_state_callback *cb,
			   enum canopen_nmt_state state, uint8_t node_id)
{
	LOG_INF("NMT state: %s, node-ID: %d", canopen_nmt_state_str(state), node_id);
}

int main(void)
{
	const struct device *can = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
	int err;

	if (!device_is_ready(can)) {
		LOG_ERR("CAN device not ready");
		return -ENODEV;
	}

	/* TODO: use macro */
	co.sdo_servers = &sdo_server;
	co.num_sdo_servers = 1U;

	err = canopen_init(&co, &objdict, can, 127U);
	if (err != 0) {
		LOG_ERR("failed to initialize the CANopen protocol stack (err %d)", err);
		return err;
	}

	canopen_nmt_init_state_callback(&cb, state_callback);

	err = canopen_nmt_add_state_callback(&co.nmt, &cb);
	if (err != 0) {
		LOG_ERR("failed to add NMT state callback (err %d)", err);
		return err;
	}

	/* TODO: move this to nmt.c */
	err = can_start(can);
	if (err != 0) {
		LOG_ERR("failed to start CAN device (err %d)", err);
		return err;
	}

	err = canopen_enable(&co);
	if (err != 0) {
		LOG_ERR("failed to enable the CANopen protocol stack (err %d)", err);
		return err;
	}

	return 0;
}
