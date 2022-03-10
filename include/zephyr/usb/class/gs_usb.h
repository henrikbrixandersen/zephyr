/*
 * Copyright (c) 2022-2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Geschwister Schneider USB/CAN Device Class API
 *
 * +--------+         +----------+-------------+
 * |        |         |          |  Channel 0  |
 * |        |/-------\|          +-------------+
 * |  Host  |   USB   |  Device  |  Channel 1  |
 * |        |\-------/|          +-------------+
 * |        |         |          |  Channel 2  |
 * +--------+         +----------+-------------+
 *
 */

#ifndef ZEPHYR_INCLUDE_USB_CLASS_GS_USB_H_
#define ZEPHYR_INCLUDE_USB_CLASS_GS_USB_H_

#include <zephyr/device.h>

typedef int (*gs_usb_timestamp_callback_t)(const struct device *dev, uint32_t *timestamp);

typedef int (*gs_usb_identify_callback_t)(const struct device *dev, uint16_t ch, bool identify,
					  void *user_data);

typedef int (*gs_usb_set_termination_callback_t)(const struct device *dev, uint16_t ch,
						 bool terminate, void *user_data);

typedef int (*gs_usb_get_termination_callback_t)(const struct device *dev, uint16_t ch,
						 bool *terminated, void *user_data);

struct gs_usb_ops {
	gs_usb_timestamp_callback_t timestamp;
	gs_usb_set_termination_callback_t set_termination;
	gs_usb_get_termination_callback_t get_termination;
	gs_usb_identify_callback_t identify;
	/* TODO: optional channel status callback? */
	/* TODO: optional channel rx/tx activity callback? */
};

int gs_usb_register(const struct device *dev, const struct device **channels, size_t nchannels,
		    const struct gs_usb_ops *ops, void *user_data);

#endif /* ZEPHYR_INCLUDE_USB_CLASS_GS_USB_H_ */
