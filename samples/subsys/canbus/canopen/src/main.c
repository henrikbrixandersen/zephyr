/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, CONFIG_CANOPEN_LOG_LEVEL);

struct canopen_nmt nmt;
struct canopen_nmt_state_callback cb;

static void state_callback(struct canopen_nmt *nmt, struct canopen_nmt_state_callback *cb,
			   enum canopen_nmt_state state, uint8_t node_id)
{
	LOG_INF("NMT state: %s, node-ID: %d", canopen_nmt_state_str(state), node_id);
}

int main(void)
{
	int err;

	canopen_nmt_init_state_callback(&cb, state_callback);

	err = canopen_nmt_add_state_callback(&nmt, &cb);
	if (err != 0) {
		LOG_ERR("failed to add NMT state callback (err %d)", err);
		return err;
	}

	err = canopen_nmt_init(&nmt, 127U);
	if (err != 0) {
		LOG_ERR("failed to initialize NMT object");
		return err;
	}

	k_msleep(100);

	LOG_INF("starting");
	canopen_nmt_start(&nmt);

	k_msleep(100);

	LOG_INF("entering pre-operational");
	canopen_nmt_enter_pre_operational(&nmt);

	k_msleep(100);

	LOG_INF("stopping");
	canopen_nmt_stop(&nmt);

	k_msleep(100);

	LOG_INF("resetting communication");
	canopen_nmt_reset_communication(&nmt);

	k_msleep(100);

	LOG_INF("resetting node");
	canopen_nmt_reset_node(&nmt);

	return 0;
}
