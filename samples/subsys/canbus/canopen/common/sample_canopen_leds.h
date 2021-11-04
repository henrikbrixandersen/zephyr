/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SAMPLES_SUBSYS_CANBUS_CANOPEN_LEDS_H_
#define ZEPHYR_SAMPLES_SUBSYS_CANBUS_CANOPEN_LEDS_H_

#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>

extern struct led_dt_spec red_led;
extern struct led_dt_spec green_led;

#endif /* ZEPHYR_SAMPLES_SUBSYS_CANBUS_CANOPEN_LEDS_H_ */
