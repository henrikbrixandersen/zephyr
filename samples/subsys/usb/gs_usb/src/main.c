/*
 * Copyright (c) 2022-2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sample_usbd.h"

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/gs_usb.h>
#include <zephyr/usb/usbd.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static int identify_callback(const struct device *dev, uint16_t ch, bool identify,
			     void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("identify channel %u %s", ch, identify ? "on" : "off");

	return 0;
}

static int set_termination_callback(const struct device *dev, uint16_t ch, bool terminate,
				    void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("set termination for channel %u %s", ch, terminate ? "on" : "off");

	return 0;
}

static int get_termination_callback(const struct device *dev, uint16_t ch, bool *terminated,
				    void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("get termination for channel %u", ch);

	*terminated = true;

	return 0;
}

static const struct gs_usb_ops gs_usb_ops = {
	.identify = identify_callback,
	.set_termination = set_termination_callback,
	.get_termination = get_termination_callback,
};

int main(void)
{
	struct usbd_context *sample_usbd;
	const struct device *gs_usb = DEVICE_DT_GET(DT_NODELABEL(gs_usb0));
	const struct device *channels[] = {
		DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus)),
	};
	int err;

	if (!device_is_ready(gs_usb)) {
		LOG_ERR("gs_usb USB device not ready");
		return 0;
	}

	err = gs_usb_register(gs_usb, channels, ARRAY_SIZE(channels), &gs_usb_ops, NULL);
	if (err != 0U) {
		LOG_ERR("failed to register gs_usb (err %d)", err);
		return 0;
	}

	sample_usbd = sample_usbd_init_device(NULL);
	if (sample_usbd == NULL) {
		LOG_ERR("failed to initialize USB device");
		return -ENODEV;
	}

	err = usbd_enable(sample_usbd);
	if (err) {
		LOG_ERR("failed to enable USB device");
		return err;
	}
}
