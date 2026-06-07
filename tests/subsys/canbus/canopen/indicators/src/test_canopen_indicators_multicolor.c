/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen/indicators.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/led/led_fake.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "common.h"

/* Global variables */
static struct canopen_indicators dut;

ZTEST(canopen_indicators_multicolor, test_red_on_off)
{
	/* Turn LED on */
	zassert_ok(canopen_indicators_set_state_red(&dut, CANOPEN_INDICATOR_STATE_ON));

	zassert_ok(k_sem_take(&event_sem, K_MSEC(TIMEOUT_MS)));
	zassert_equal(fake_led_set_brightness_fake.call_count, 1);
	zassert_equal(fake_led_set_brightness_fake.arg0_val, red_led.dev);
	zassert_equal(fake_led_set_brightness_fake.arg1_val, red_led.index);
	zassert_equal(fake_led_set_brightness_fake.arg2_val, LED_BRIGHTNESS_MAX);

	/* Turn LED off */
	zassert_ok(canopen_indicators_set_state_red(&dut, CANOPEN_INDICATOR_STATE_OFF));

	zassert_ok(k_sem_take(&event_sem, K_MSEC(TIMEOUT_MS)));
	zassert_equal(fake_led_set_brightness_fake.call_count, 2);
	zassert_equal(fake_led_set_brightness_fake.arg0_val, red_led.dev);
	zassert_equal(fake_led_set_brightness_fake.arg1_val, red_led.index);
	zassert_equal(fake_led_set_brightness_fake.arg2_val, 0U);
}

ZTEST(canopen_indicators_multicolor, test_green_on_off)
{
	/* Turn LED on */
	zassert_ok(canopen_indicators_set_state_green(&dut, CANOPEN_INDICATOR_STATE_ON));
	assert_rgb_color_set(1, rgb_color_green);

	/* Turn LED off */
	zassert_ok(canopen_indicators_set_state_green(&dut, CANOPEN_INDICATOR_STATE_OFF));
	assert_rgb_color_set(2, rgb_color_off);
}

static void canopen_indicators_multicolor_before(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Reset events counter */
	k_sem_reset(&event_sem);

	/* Clear last set color */
	memset(rgb_color_last, 0, sizeof(rgb_color_last));

	/* Reset FFF history */
	FFF_RESET_HISTORY();

	/* Re-install custom delegates */
	fake_led_set_brightness_fake.custom_fake = fake_led_set_brightness_delegate;
	fake_led_set_color_fake.custom_fake = fake_led_set_color_delegate;
}

static void canopen_indicators_multicolor_after(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Turn LEDs off */
	zassert_ok(canopen_indicators_set_state_red(&dut, CANOPEN_INDICATOR_STATE_OFF));
	zassert_ok(canopen_indicators_set_state_green(&dut, CANOPEN_INDICATOR_STATE_OFF));

	/* Allow state to propagate */
	k_sleep(K_NSEC(PULSE_NS * 2U));
}

static void *canopen_indicators_multicolor_setup(void)
{
	zassert_true(led_is_ready_dt(&red_led));
	zassert_true(led_is_ready_dt(&rgb_led));

	zassert_ok(canopen_indicators_init(&dut, &k_sys_work_q, &red_led, &rgb_led));
	zassert_ok(canopen_indicators_enable(&dut));

	return NULL;
}

ZTEST_SUITE(canopen_indicators_multicolor, NULL, canopen_indicators_multicolor_setup,
	    canopen_indicators_multicolor_before, canopen_indicators_multicolor_after, NULL);
