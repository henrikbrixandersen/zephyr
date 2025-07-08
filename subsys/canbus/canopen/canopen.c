/*
 * Copyright (c) 2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen.h>
#include <zephyr/canbus/canopen/nmt.h>
#include <zephyr/canbus/canopen/sdo.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(canopen, CONFIG_CANOPEN_LOG_LEVEL);

#ifdef CONFIG_CANOPEN_USE_DEDICATED_WORKQUEUE
static K_KERNEL_STACK_DEFINE(canopen_work_q_stack, CONFIG_CANOPEN_WORKQUEUE_STACK_SIZE);
static struct k_work_q canopen_work_q;
#endif /* CONFIG_CANOPEN_USE_DEDICATED_WORKQUEUE */

int canopen_init(struct canopen *co, const struct canopen_od *od, const struct device *can,
		 uint8_t node_id)
{
	struct k_work_q *work_q;
	int err;

	__ASSERT_NO_MSG(co != NULL);
	__ASSERT_NO_MSG(od != NULL);
	__ASSERT_NO_MSG(can != NULL);

#ifdef CONFIG_CANOPEN_USE_DEDICATED_WORKQUEUE
	static const struct k_work_queue_config cfg = {
		.name = "canopen",
		.no_yield = IS_ENABLED(CONFIG_CANOPEN_WORKQUEUE_NO_YIELD),
		.essential = true,
		.work_timeout_ms = CONFIG_CANOPEN_WORKQUEUE_WORK_TIMEOUT_MS,
	};

	k_work_queue_start(&canopen_work_q, canopen_work_q_stack,
			   K_KERNEL_STACK_SIZEOF(canopen_work_q_stack),
			   CONFIG_CANOPEN_WORKQUEUE_PRIORITY, &cfg);

	work_q = &canopen_work_q;
#else  /* CONFIG_CANOPEN_USE_DEDICATED_WORKQUEUE */
	/* Use system workqueue */
	work_q = &k_sys_work_q;
#endif /* !CONFIG_CANOPEN_USE_DEDICATED_WORKQUEUE */

	co->od = od;

	if (!device_is_ready(can)) {
		LOG_ERR("CAN controller %s not ready", can->name);
		return -ENODEV;
	}

	err = canopen_nmt_init(&co->nmt, work_q, can, node_id);
	if (err != 0) {
		LOG_ERR("failed to initialize CANopen NMT FSA (err %d)", err);
		return err;
	}

	for (int i = 0; i < co->num_sdo_servers; i++) {
		uint8_t sdo_number = i + 1U;

		err = canopen_sdo_server_init(&co->sdo_servers[i], work_q, can, sdo_number);
		if (err != 0) {
			LOG_ERR("failed to initialize CANopen SDO server %d (err %d)", sdo_number,
				err);
			return err;
		}
	}

	return 0;
}

int canopen_enable(struct canopen *co)
{
	return canopen_nmt_enable(&co->nmt);
}
