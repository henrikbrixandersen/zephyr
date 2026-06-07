/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_CANOPEN_INDICATORS_COMMON_H_
#define TEST_CANOPEN_INDICATORS_COMMON_H_

#include <zephyr/sys/clock.h>

/* LED event timeout in milliseconds */
#define TIMEOUT_MS 2000U

/* Minimum LED pulse in nanoseconds */
#define PULSE_NS (50U * NSEC_PER_MSEC)

/* Acceptable time delta */
#define TIME_DELTA_NS (1U * NSEC_PER_MSEC)

/* Number of colors of the fake RGB LED */
#define RGB_NUM_COLORS 3U

/* Fake RGB LED colors */
extern const uint8_t rgb_color_red[RGB_NUM_COLORS];
extern const uint8_t rgb_color_green[RGB_NUM_COLORS];
extern const uint8_t rgb_color_off[RGB_NUM_COLORS];

/* Last set fake RGB LED color */
extern uint8_t rgb_color_last[RGB_NUM_COLORS];

/* LED event counting semaphore */
extern struct k_sem event_sem;

/* LEDs */
extern const struct led_dt_spec red_led;
extern const struct led_dt_spec green_led;
extern const struct led_dt_spec rgb_led;

int fake_led_set_brightness_delegate(const struct device *dev, uint32_t led, uint8_t value);

int fake_led_set_color_delegate(const struct device *dev, uint32_t led, uint8_t num_colors,
				const uint8_t *color);

void assert_rgb_color_set(int call_count, const uint8_t *expected);


#endif /* TEST_CANOPEN_INDICATORS_COMMON_H_ */
