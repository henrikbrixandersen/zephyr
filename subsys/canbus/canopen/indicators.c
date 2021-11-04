/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen/indicators.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(canopen_indicators, CONFIG_CANOPEN_LOG_LEVEL);

/* Indicator period in milliseconds */
#define CANOPEN_INDICATOR_PERIOD_MS 50U

/* Red LED indicator patterns as bitmasks, each bit representing 50 ms */
static const uint32_t canopen_indicator_patterns_red[] = {
	[CANOPEN_INDICATOR_STATE_ON] = UINT32_MAX,
	[CANOPEN_INDICATOR_STATE_OFF] = 0x0U,
	[CANOPEN_INDICATOR_STATE_FLICKERING] = 0x01U,
	[CANOPEN_INDICATOR_STATE_BLINKING] = 0x0FU,
	[CANOPEN_INDICATOR_STATE_SINGLE_FLASH] = 0x0FU,
	[CANOPEN_INDICATOR_STATE_DOUBLE_FLASH] = 0x0F0FU,
	[CANOPEN_INDICATOR_STATE_TRIPLE_FLASH] = 0x0F0F0FU,
	[CANOPEN_INDICATOR_STATE_QUADRUPLE_FLASH] = 0x0F0F0F0FU,
};

/* Green LED indicator patterns as bitmasks, each bit representing 50 ms */
static const uint32_t canopen_indicator_patterns_green[] = {
	[CANOPEN_INDICATOR_STATE_ON] = UINT32_MAX,
	[CANOPEN_INDICATOR_STATE_OFF] = 0x0U,
	[CANOPEN_INDICATOR_STATE_FLICKERING] = 0x02U,
	[CANOPEN_INDICATOR_STATE_BLINKING] = 0xF0U,
	[CANOPEN_INDICATOR_STATE_SINGLE_FLASH] = 0xF0U,
	/* Green double-flash is reserved for future use in CiA 303-3 */
	[CANOPEN_INDICATOR_STATE_DOUBLE_FLASH] = 0xF0F0U,
	[CANOPEN_INDICATOR_STATE_TRIPLE_FLASH] = 0xF0F0F0U,
	/* Green quadruple-flash is not specified in CiA 303-3, but included here for simplicity */
	[CANOPEN_INDICATOR_STATE_QUADRUPLE_FLASH] = 0xF0F0F0F0U,
};

/* Indicator patterns */
static const uint32_t *canopen_indicator_patterns[] = {
	[CANOPEN_INDICATOR_COLOR_RED] = canopen_indicator_patterns_red,
	[CANOPEN_INDICATOR_COLOR_GREEN] = canopen_indicator_patterns_green,
};

/* Indicator state pattern period lengths in units of 50 ms (bits) */
static const uint8_t canopen_indicator_pattern_lengths[] = {
	[CANOPEN_INDICATOR_STATE_ON] = NUM_BITS(uint32_t),
	[CANOPEN_INDICATOR_STATE_OFF] = NUM_BITS(uint32_t),
	[CANOPEN_INDICATOR_STATE_FLICKERING] = 2U,
	[CANOPEN_INDICATOR_STATE_BLINKING] = 8U,
	[CANOPEN_INDICATOR_STATE_SINGLE_FLASH] = 24U,
	[CANOPEN_INDICATOR_STATE_DOUBLE_FLASH] = 32U,
	[CANOPEN_INDICATOR_STATE_TRIPLE_FLASH] = 40U,
	[CANOPEN_INDICATOR_STATE_QUADRUPLE_FLASH] = 48U,
};

/* Color map for all colors off */
__maybe_unused static const uint8_t canopen_indicators_color_off[LED_COLOR_ID_MAX] = {0U};

#ifdef CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT
static void canopen_indicators_update_status_led(struct canopen_indicators *indicators, bool *on)
{
	struct canopen_indicator_led *common = &indicators->leds[0];
	const uint8_t *color = canopen_indicators_color_off;
	bool changed = false;
	int err;

	if (on[CANOPEN_INDICATOR_COLOR_RED]) {
		/* Give priority to the red LED to avoid orange/amber light */
		on[CANOPEN_INDICATOR_COLOR_GREEN] = false;
	}

	for (int i = 0; i < ARRAY_SIZE(indicators->leds); i++) {
		if (on[i] != indicators->leds[i].on) {
			changed = true;
			break;
		}
	}

	if (!changed) {
		return;
	}

	if (on[CANOPEN_INDICATOR_COLOR_RED]) {
		color = indicators->leds[CANOPEN_INDICATOR_COLOR_RED].color;
	} else if (on[CANOPEN_INDICATOR_COLOR_GREEN]) {
		color = indicators->leds[CANOPEN_INDICATOR_COLOR_GREEN].color;
	}

	err = led_set_color_dt(&common->led, common->num_colors, color);
	if (err != 0) {
		LOG_ERR("failed to set LED color (err %d)", err);
	} else {
		for (int i = 0; i < ARRAY_SIZE(indicators->leds); i++) {
			indicators->leds[i].on = on[i];
		}
	}
}
#endif /* CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT */

static void canopen_indicators_update_error_run_leds(struct canopen_indicators *indicators,
						     bool *on)
{
	int err;

	for (int i = 0; i < ARRAY_SIZE(indicators->leds); i++) {
		struct canopen_indicator_led *led = &indicators->leds[i];

		if (on[i] == led->on) {
			continue;
		}

#ifdef CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT
		if (led->num_colors > 1U) {
			err = led_set_color_dt(&led->led, led->num_colors,
					       on[i] ? led->color : canopen_indicators_color_off);
		} else {
#endif /* CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT */
			err = led_set_brightness_dt(&led->led, on[i] ? LED_BRIGHTNESS_MAX : 0U);
#ifdef CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT
		}
#endif /* CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT */
		if (err != 0) {
			LOG_ERR("failed to turn LED %s (err %d)", on[i] ? "on" : "off", err);
		} else {
			led->on = on[i];
		}
	}
}

static void canopen_indicators_work_handler(struct k_work *work)
{
	struct canopen_indicators *indicators = CONTAINER_OF(work, struct canopen_indicators, work);
	bool on[ARRAY_SIZE(indicators->leds)];

	for (int i = 0; i < ARRAY_SIZE(indicators->leds); i++) {
		enum canopen_indicator_state state = indicators->leds[i].state;
		uint8_t count = indicators->counters[state];

		if (count >= NUM_BITS(uint32_t)) {
			on[i] = false;
		} else {
			on[i] = BIT(count) & canopen_indicator_patterns[i][state];
		}
	}

#ifdef CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT
	if (indicators->is_status_led) {
		canopen_indicators_update_status_led(indicators, on);
	} else {
#endif /* CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT */
		canopen_indicators_update_error_run_leds(indicators, on);
#ifdef CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT
	}
#endif /* CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT */

	for (int i = 0; i < ARRAY_SIZE(indicators->counters); i++) {
		indicators->counters[i] += 1U;

		if (indicators->counters[i] >= canopen_indicator_pattern_lengths[i]) {
			indicators->counters[i] = 0U;
		}
	}
}

static void canopen_indicators_timer_expired(struct k_timer *timer)
{
	struct canopen_indicators *indicators =
		CONTAINER_OF(timer, struct canopen_indicators, timer);
	int err;

	err = k_work_submit_to_queue(indicators->work_q, &indicators->work);
	if (err < 0) {
		LOG_ERR("failed to submit work item (err %d)", err);
	}
}

int canopen_indicators_set_state(struct canopen_indicators *indicators,
				 enum canopen_indicator_color color,
				 enum canopen_indicator_state state)
{
	__ASSERT_NO_MSG(indicators != NULL);

	if (color < 0 || color >= CANOPEN_INDICATOR_COLOR_MAX) {
		LOG_ERR("invalid indicator color %d", color);
		return -EINVAL;
	}

	if (state < 0 || state >= CANOPEN_INDICATOR_STATE_MAX) {
		LOG_ERR("invalid indicator state %d", state);
		return -EINVAL;
	}

	indicators->leds[color].state = state;

	return 0;
}

int canopen_indicators_enable(struct canopen_indicators *indicators)
{
	__ASSERT_NO_MSG(indicators != NULL);

	k_timer_start(&indicators->timer, K_NO_WAIT, K_MSEC(CANOPEN_INDICATOR_PERIOD_MS));

	return 0;
}

#ifdef CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT
static int canopen_indicators_init_color_map(struct canopen_indicator_led *led, uint8_t color)
{
	const struct led_info *info;
	int err;

	memset(led->color, 0, sizeof(led->color));

	err = led_get_info_dt(&led->led, &info);
	if (err == -ENOSYS) {
		/* Color map not available, mono-color LED */
		led->num_colors = 0U;
		goto mapped;
	}

	if (err < 0) {
		LOG_ERR("failed to get LED color map (err %d)", err);
		return -ENODEV;
	}

	if (info->num_colors > ARRAY_SIZE(led->color)) {
		LOG_ERR("LED color map too large (%u entries)", info->num_colors);
		return -ENOTSUP;
	}

	led->num_colors = info->num_colors;
	if (led->num_colors < 2U) {
		/* less than two colors in map, mono-color LED */
		goto mapped;
	}

	for (int i = 0; i < info->num_colors; i++) {
		if (info->color_mapping[i] == color) {
			led->color[i] = UINT8_MAX;
			goto mapped;
		}
	}

	/* Requested color not supported by multi-color LED */
	return -ENOTSUP;

mapped:
	return 0;
}
#endif /* CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT */

int canopen_indicators_init(struct canopen_indicators *indicators, struct k_work_q *work_q,
			    const struct led_dt_spec *red_led, const struct led_dt_spec *green_led)
{
#ifdef CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT
	const uint8_t colors[CANOPEN_INDICATOR_COLOR_MAX] = {
		[CANOPEN_INDICATOR_COLOR_RED] = LED_COLOR_ID_RED,
		[CANOPEN_INDICATOR_COLOR_GREEN] = LED_COLOR_ID_GREEN,
	};
	int err;
#endif /* CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT */

	__ASSERT_NO_MSG(indicators != NULL);
	__ASSERT_NO_MSG(work_q != NULL);

	indicators->work_q = work_q;
	indicators->leds[CANOPEN_INDICATOR_COLOR_RED].led = *red_led;
	indicators->leds[CANOPEN_INDICATOR_COLOR_GREEN].led = *green_led;

	for (int i = 0; i < ARRAY_SIZE(indicators->leds); i++) {
		struct canopen_indicator_led *led = &indicators->leds[i];

		if (!led_is_ready_dt(&led->led)) {
			LOG_ERR_DEVICE_NOT_READY(led->led.dev);
			return -ENODEV;
		}

#ifdef CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT
		err = canopen_indicators_init_color_map(led, colors[i]);
		if (err != 0) {
			LOG_ERR("failed to initialize LED %d color map (err %d)", i, err);
			return err;
		}
#endif /* CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT */

		led->state = CANOPEN_INDICATOR_STATE_OFF;
		led->on = false;
	}

#ifdef CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT
	if (red_led->dev == green_led->dev && red_led->index == green_led->index) {
		if (indicators->leds[CANOPEN_INDICATOR_COLOR_RED].num_colors < 2U) {
			LOG_ERR("CANopen status LED with too few colors");
			return -EINVAL;
		}

		indicators->is_status_led = true;
	} else {
		indicators->is_status_led = false;
	}
#endif /* CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT */

	k_timer_init(&indicators->timer, canopen_indicators_timer_expired, NULL);
	k_work_init(&indicators->work, canopen_indicators_work_handler);

	memset(indicators->counters, 0, sizeof(indicators->counters));

	return 0;
}
