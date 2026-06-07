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
#include <zephyr/timing/timing.h>
#include <zephyr/ztest.h>

#include "common.h"

/* Global variables */
static struct canopen_indicators dut;

static void test_on_off(enum canopen_indicator_color color, const struct led_dt_spec *led)
{
	/* Turn LED on */
	zassert_ok(canopen_indicators_set_state(&dut, color, CANOPEN_INDICATOR_STATE_ON));

	zassert_ok(k_sem_take(&event_sem, K_MSEC(TIMEOUT_MS)));
	zassert_equal(fake_led_set_brightness_fake.call_count, 1);
	zassert_equal(fake_led_set_brightness_fake.arg0_val, led->dev);
	zassert_equal(fake_led_set_brightness_fake.arg1_val, led->index);
	zassert_equal(fake_led_set_brightness_fake.arg2_val, LED_BRIGHTNESS_MAX);

	/* Turn LED off */
	zassert_ok(canopen_indicators_set_state(&dut, color, CANOPEN_INDICATOR_STATE_OFF));

	zassert_ok(k_sem_take(&event_sem, K_MSEC(TIMEOUT_MS)));
	zassert_equal(fake_led_set_brightness_fake.call_count, 2);
	zassert_equal(fake_led_set_brightness_fake.arg0_val, led->dev);
	zassert_equal(fake_led_set_brightness_fake.arg1_val, led->index);
	zassert_equal(fake_led_set_brightness_fake.arg2_val, 0U);
}

ZTEST(canopen_indicators_error_run, test_red_on_off)
{
	test_on_off(CANOPEN_INDICATOR_COLOR_RED, &red_led);
}

ZTEST(canopen_indicators_error_run, test_green_on_off)
{
	test_on_off(CANOPEN_INDICATOR_COLOR_GREEN, &green_led);
}

static void test_isophase(enum canopen_indicator_color color, const struct led_dt_spec *led,
			  enum canopen_indicator_state state, uint64_t pulse_ns)
{
	const int events = 10;
	timing_t start_time;
	timing_t end_time;
	uint64_t total_cycles;
	uint64_t total_ns;
	int i;

	start_time = timing_counter_get();
	zassert_ok(canopen_indicators_set_state(&dut, color, state));

	for (i = 0; i < events; i++) {
		zassert_ok(k_sem_take(&event_sem, K_MSEC(TIMEOUT_MS)));
		end_time = timing_counter_get();

		total_cycles = timing_cycles_get(&start_time, &end_time);
		total_ns = timing_cycles_to_ns(total_cycles);

		start_time = timing_counter_get();

		TC_PRINT("total_ns = %llu\n", total_ns);

		/* Ignore first event as it may happen sooner/later than a full pulse width */
		if (i != 0) {
			zassert_within(total_ns, pulse_ns, TIME_DELTA_NS);
		}
	}

	for (i = 0; i < events; i++) {
		zassert_equal_ptr(fff.call_history[i], fake_led_set_brightness);
		zassert_equal(fake_led_set_brightness_fake.arg0_history[i], led->dev);
		zassert_equal(fake_led_set_brightness_fake.arg1_history[i], led->index);

		if (i % 2 == 0) {
			zassert_equal(fake_led_set_brightness_fake.arg2_history[i],
				      LED_BRIGHTNESS_MAX);
		} else {
			zassert_equal(fake_led_set_brightness_fake.arg2_history[i], 0U);
		}
	}
}

ZTEST(canopen_indicators_error_run, test_red_flickering)
{
	test_isophase(CANOPEN_INDICATOR_COLOR_RED, &red_led, CANOPEN_INDICATOR_STATE_FLICKERING,
		      PULSE_NS);
}

ZTEST(canopen_indicators_error_run, test_green_flickering)
{
	test_isophase(CANOPEN_INDICATOR_COLOR_GREEN, &green_led, CANOPEN_INDICATOR_STATE_FLICKERING,
		      PULSE_NS);
}

ZTEST(canopen_indicators_error_run, test_red_blinking)
{
	test_isophase(CANOPEN_INDICATOR_COLOR_RED, &red_led, CANOPEN_INDICATOR_STATE_BLINKING,
		      PULSE_NS * 4U);
}

ZTEST(canopen_indicators_error_run, test_green_blinking)
{
	test_isophase(CANOPEN_INDICATOR_COLOR_GREEN, &green_led, CANOPEN_INDICATOR_STATE_BLINKING,
		      PULSE_NS * 4U);
}

static void test_flash(enum canopen_indicator_color color, const struct led_dt_spec *led,
		       enum canopen_indicator_state state, uint8_t flashes)
{
	const int events = flashes * 4 + 1;
	timing_t start_time;
	timing_t end_time;
	uint64_t total_cycles;
	uint64_t total_ns;
	int i;

	start_time = timing_counter_get();
	zassert_ok(canopen_indicators_set_state(&dut, color, state));

	/* TODO: synchronize to shift registers */

	for (i = 0; i < events; i++) {
		zassert_ok(k_sem_take(&event_sem, K_MSEC(TIMEOUT_MS)));
		end_time = timing_counter_get();

		total_cycles = timing_cycles_get(&start_time, &end_time);
		total_ns = timing_cycles_to_ns(total_cycles);

		start_time = timing_counter_get();

		TC_PRINT("total_ns = %llu\n", total_ns);

		/* Ignore first event as it may happen sooner/later than a full pulse width */
		if (i != 0) {
			/* TODO: validate timing */
			/* zassert_within(total_ns, pulse_ns, TIME_DELTA_NS); */
		}
	}

	for (i = 0; i < events; i++) {
		zassert_equal_ptr(fff.call_history[i], fake_led_set_brightness);
		zassert_equal(fake_led_set_brightness_fake.arg0_history[i], led->dev);
		zassert_equal(fake_led_set_brightness_fake.arg1_history[i], led->index);

		if (i % 2 == 0) {
			zassert_equal(fake_led_set_brightness_fake.arg2_history[i],
				      LED_BRIGHTNESS_MAX);
		} else {
			zassert_equal(fake_led_set_brightness_fake.arg2_history[i], 0U);
		}
	}
}

ZTEST(canopen_indicators_error_run, test_red_single_flash)
{
	test_flash(CANOPEN_INDICATOR_COLOR_RED, &red_led, CANOPEN_INDICATOR_STATE_SINGLE_FLASH, 1U);
}

ZTEST(canopen_indicators_error_run, test_green_single_flash)
{
	test_flash(CANOPEN_INDICATOR_COLOR_GREEN, &green_led, CANOPEN_INDICATOR_STATE_SINGLE_FLASH,
		   1U);
}

ZTEST(canopen_indicators_error_run, test_red_double_flash)
{
	test_flash(CANOPEN_INDICATOR_COLOR_RED, &red_led, CANOPEN_INDICATOR_STATE_DOUBLE_FLASH, 2U);
}

ZTEST(canopen_indicators_error_run, test_green_double_flash)
{
	test_flash(CANOPEN_INDICATOR_COLOR_GREEN, &green_led, CANOPEN_INDICATOR_STATE_DOUBLE_FLASH,
		   2U);
}

ZTEST(canopen_indicators_error_run, test_red_triple_flash)
{
	test_flash(CANOPEN_INDICATOR_COLOR_RED, &red_led, CANOPEN_INDICATOR_STATE_TRIPLE_FLASH, 3U);
}

ZTEST(canopen_indicators_error_run, test_green_triple_flash)
{
	test_flash(CANOPEN_INDICATOR_COLOR_GREEN, &green_led, CANOPEN_INDICATOR_STATE_TRIPLE_FLASH,
		   3U);
}

ZTEST(canopen_indicators_error_run, test_red_quadruple_flash)
{
	test_flash(CANOPEN_INDICATOR_COLOR_RED, &red_led, CANOPEN_INDICATOR_STATE_QUADRUPLE_FLASH,
		   4U);
}

ZTEST(canopen_indicators_error_run, test_green_quadruple_flash)
{
	test_flash(CANOPEN_INDICATOR_COLOR_GREEN, &green_led,
		   CANOPEN_INDICATOR_STATE_QUADRUPLE_FLASH, 4U);
}

static void canopen_indicators_error_run_before(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Reset events counter */
	k_sem_reset(&event_sem);

	/* Reset FFF history */
	FFF_RESET_HISTORY();

	/* Re-install custom delegate */
	fake_led_set_brightness_fake.custom_fake = fake_led_set_brightness_delegate;

	/* Initialise and start timing */
	timing_init();
	timing_start();
}

static void canopen_indicators_error_run_after(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Turn LEDs off */
	zassert_ok(canopen_indicators_set_state_red(&dut, CANOPEN_INDICATOR_STATE_OFF));
	zassert_ok(canopen_indicators_set_state_green(&dut, CANOPEN_INDICATOR_STATE_OFF));

	/* Allow state to propagate */
	k_sleep(K_NSEC(PULSE_NS * 2U));

	/* Start timing */
	timing_stop();
}

static void *canopen_indicators_error_run_setup(void)
{
	zassert_true(led_is_ready_dt(&red_led));
	zassert_true(led_is_ready_dt(&green_led));

	zassert_ok(canopen_indicators_init(&dut, &k_sys_work_q, &red_led, &green_led));
	zassert_ok(canopen_indicators_enable(&dut));

	return NULL;
}

ZTEST_SUITE(canopen_indicators_error_run, NULL, canopen_indicators_error_run_setup,
	    canopen_indicators_error_run_before, canopen_indicators_error_run_after, NULL);
