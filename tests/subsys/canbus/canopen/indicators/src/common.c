/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/led/led_fake.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/minmax.h>
#include <zephyr/types.h>
#include <zephyr/ztest.h>

#include "common.h"

const uint8_t rgb_color_red[RGB_NUM_COLORS] = {255U, 0U, 0U};
const uint8_t rgb_color_green[RGB_NUM_COLORS] = {0U, 255U, 0U};
const uint8_t rgb_color_off[RGB_NUM_COLORS] = {0U, 0U, 0U};

uint8_t rgb_color_last[RGB_NUM_COLORS];

K_SEM_DEFINE(event_sem, 0U, K_SEM_MAX_LIMIT);

const struct led_dt_spec red_led = LED_DT_SPEC_GET(DT_NODELABEL(fake_red_led));
const struct led_dt_spec green_led = LED_DT_SPEC_GET(DT_NODELABEL(fake_green_led));
const struct led_dt_spec rgb_led = LED_DT_SPEC_GET(DT_NODELABEL(fake_rgb_led));

DEFINE_FFF_GLOBALS;

int fake_led_set_brightness_delegate(const struct device *dev, uint32_t led, uint8_t value)
{
	printk("led %d brightness = %u\n", led, value);

	k_sem_give(&event_sem);

	return 0;
}

int fake_led_set_color_delegate(const struct device *dev, uint32_t led, uint8_t num_colors,
				const uint8_t *color)
{
	memcpy(rgb_color_last, color, min(sizeof(rgb_color_last), num_colors));
	k_sem_give(&event_sem);

	return 0;
}

void assert_rgb_color_set(int call_count, const uint8_t *expected)
{
	zassert_ok(k_sem_take(&event_sem, K_MSEC(TIMEOUT_MS)));
	zassert_equal(fake_led_set_color_fake.call_count, call_count);
	zassert_equal(fake_led_set_color_fake.arg0_val, rgb_led.dev);
	zassert_equal(fake_led_set_color_fake.arg1_val, rgb_led.index);
	zassert_equal(fake_led_set_color_fake.arg2_val, RGB_NUM_COLORS);
	zassert_mem_equal(rgb_color_last, expected, RGB_NUM_COLORS);
}
