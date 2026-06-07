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
#include <zephyr/sys/minmax.h>
#include <zephyr/ztest.h>

#include "common.h"

/* Global variables */
static struct canopen_indicators dut;

static void test_on_off(enum canopen_indicator_color color, const uint8_t *expected)
{
	/* Turn LED on */
	zassert_ok(canopen_indicators_set_state(&dut, color, CANOPEN_INDICATOR_STATE_ON));
	assert_rgb_color_set(1, expected);

	/* Turn LED off */
	zassert_ok(canopen_indicators_set_state(&dut, color, CANOPEN_INDICATOR_STATE_OFF));
	assert_rgb_color_set(2, rgb_color_off);
}

ZTEST(canopen_indicators_status, test_red_on_off)
{
	test_on_off(CANOPEN_INDICATOR_COLOR_RED, rgb_color_red);
}

ZTEST(canopen_indicators_status, test_green_on_off)
{
	test_on_off(CANOPEN_INDICATOR_COLOR_GREEN, rgb_color_green);
}

ZTEST(canopen_indicators_status, test_red_priority)
{
	/* Turn green LED on */
	zassert_ok(canopen_indicators_set_state_green(&dut, CANOPEN_INDICATOR_STATE_ON));
	assert_rgb_color_set(1, rgb_color_green);

	/* Turn red LED on, red will take priority */
	zassert_ok(canopen_indicators_set_state_red(&dut, CANOPEN_INDICATOR_STATE_ON));
	assert_rgb_color_set(2, rgb_color_red);

	/* Turn red LED off, green back on */
	zassert_ok(canopen_indicators_set_state_red(&dut, CANOPEN_INDICATOR_STATE_OFF));
	assert_rgb_color_set(3, rgb_color_green);

	/* Turn green LED off, both off */
	zassert_ok(canopen_indicators_set_state_green(&dut, CANOPEN_INDICATOR_STATE_OFF));
	assert_rgb_color_set(4, rgb_color_off);

	/* Turn red LED on */
	zassert_ok(canopen_indicators_set_state_red(&dut, CANOPEN_INDICATOR_STATE_ON));
	assert_rgb_color_set(5, rgb_color_red);

	/* Turn green LED on, red still has priority */
	zassert_ok(canopen_indicators_set_state_green(&dut, CANOPEN_INDICATOR_STATE_ON));
	zassert_not_ok(k_sem_take(&event_sem, K_MSEC(TIMEOUT_MS)));

	/* Turn red LED off, green now on */
	zassert_ok(canopen_indicators_set_state_red(&dut, CANOPEN_INDICATOR_STATE_OFF));
	assert_rgb_color_set(6, rgb_color_green);
}

ZTEST(canopen_indicators_status, test_biphase)
{
	/* TODO: test biphase pattern */
}

static void canopen_indicators_status_before(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Reset events counter */
	k_sem_reset(&event_sem);

	/* Clear last set color */
	memset(rgb_color_last, 0, sizeof(rgb_color_last));

	/* Reset FFF history */
	FFF_RESET_HISTORY();

	/* Re-install custom delegate */
	fake_led_set_color_fake.custom_fake = fake_led_set_color_delegate;
}

static void canopen_indicators_status_after(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Turn LEDs off */
	zassert_ok(canopen_indicators_set_state_red(&dut, CANOPEN_INDICATOR_STATE_OFF));
	zassert_ok(canopen_indicators_set_state_green(&dut, CANOPEN_INDICATOR_STATE_OFF));

	/* Allow state to propagate */
	k_sleep(K_NSEC(PULSE_NS * 2U));
}

static void *canopen_indicators_status_setup(void)
{
	zassert_true(led_is_ready_dt(&rgb_led));

	zassert_ok(canopen_indicators_init(&dut, &k_sys_work_q, &rgb_led, &rgb_led));
	zassert_ok(canopen_indicators_enable(&dut));

	return NULL;
}

ZTEST_SUITE(canopen_indicators_status, NULL, canopen_indicators_status_setup,
	    canopen_indicators_status_before, canopen_indicators_status_after, NULL);
