/*
 * Copyright (c) 2022-2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sample_usbd.h"

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/gs_usb.h>
#include <zephyr/usb/usbd.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

#if (defined(CONFIG_USBD_GS_USB_TIMESTAMP) && DT_NODE_HAS_PROP(ZEPHYR_USER_NODE, timestamp))
#define TIMESTAMP_SUPPORTED 1
#endif

#ifdef TIMESTAMP_SUPPORTED
const struct device *counter = DEVICE_DT_GET(DT_PHANDLE(DT_PATH(zephyr_user), timestamp));

static int timestamp_callback(const struct device *dev, uint32_t *timestamp, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	return counter_get_value(counter, timestamp);
}

static int enable_timestamp_counter(void)
{
	int err;

	if (!device_is_ready(counter)) {
		LOG_ERR("timestamp device not ready");
		return -ENODEV;
	}

	if (counter_get_frequency(counter) != MHZ(1)) {
		LOG_ERR("wrong timestamp counter frequency (%u)", counter_get_frequency(counter));
		return -EINVAL;
	}

	if (counter_get_max_top_value(counter) != UINT32_MAX) {
		LOG_ERR("timestamp counter is not 32 bit wide");
		return -EINVAL;
	}

	err = counter_start(counter);
	if (err != 0) {
		LOG_ERR("failed to start timestamp counter (err %d)", err);
		return err;
	};

	return 0;
}
#endif /* TIMESTAMP_SUPPORTED */

static int identify_callback(const struct device *dev, uint16_t ch, bool identify,
			     void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	LOG_INF("identify channel %u %s", ch, identify ? "on" : "off");

	return 0;
}

static int set_termination_callback(const struct device *dev, uint16_t ch, bool terminate,
				    void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	LOG_INF("set termination for channel %u %s", ch, terminate ? "on" : "off");

	return 0;
}

static int get_termination_callback(const struct device *dev, uint16_t ch, bool *terminated,
				    void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	LOG_INF("get termination for channel %u", ch);

	*terminated = true;

	return 0;
}

static const struct gs_usb_ops gs_usb_ops = {
#ifdef TIMESTAMP_SUPPORTED
	.timestamp = timestamp_callback,
#endif /* TIMESTAMP_SUPPORTED */
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

#ifdef TIMESTAMP_SUPPORTED
	err = enable_timestamp_counter();
	if (err != 0) {
		return 0;
	}
#endif /* TIMESTAMP_SUPPORTED */

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
