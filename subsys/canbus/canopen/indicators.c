/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/arch/common/ffs.h>
#include <zephyr/canbus/canopen/indicators.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(canopen_indicators, CONFIG_CANOPEN_LOG_LEVEL);

/* Error LED truth table states mask, CiA 303-3, table 2 */
#define CANOPEN_INDICATORS_ERROR_LED_STATE_MASK                                                    \
	(BIT(CANOPEN_INDICATORS_STATE_AUTOBITRATE_OR_LSS) |                                        \
	 BIT(CANOPEN_INDICATORS_STATE_INVALID_CONFIGURATION) |                                     \
	 BIT(CANOPEN_INDICATORS_STATE_WARNING_LIMIT_REACHED) |                                     \
	 BIT(CANOPEN_INDICATORS_STATE_ERROR_CONTROL_EVENT) |                                       \
	 BIT(CANOPEN_INDICATORS_STATE_SYNC_ERROR) |                                                \
	 BIT(CANOPEN_INDICATORS_STATE_EVENT_TIMER_ERROR) | BIT(CANOPEN_INDICATORS_STATE_BUS_OFF))

/* Run LED truth table states mask, CiA 303-3, table 3 */
#define CANOPEN_INDICATORS_RUN_LED_STATE_MASK                                                      \
	(BIT(CANOPEN_INDICATORS_STATE_AUTOBITRATE_OR_LSS) |                                        \
	 BIT(CANOPEN_INDICATORS_STATE_PRE_OPERATIONAL) | BIT(CANOPEN_INDICATORS_STATE_STOPPED) |   \
	 BIT(CANOPEN_INDICATORS_STATE_PROGRAM_DOWNLOAD) |                                          \
	 BIT(CANOPEN_INDICATORS_STATE_OPERATIONAL))

/* Indicator period in milliseconds */
#define CANOPEN_INDICATORS_PERIOD_MS 50U

/* Red LED indicator patterns as bitmasks, each bit representing 50 ms */
static const uint32_t canopen_indicators_patterns_red[] = {
	[CANOPEN_INDICATORS_LED_STATE_ON] = 0x1U,
	[CANOPEN_INDICATORS_LED_STATE_OFF] = 0x0U,
	[CANOPEN_INDICATORS_LED_STATE_FLICKERING] = 0x01U,
	[CANOPEN_INDICATORS_LED_STATE_BLINKING] = 0x0FU,
	[CANOPEN_INDICATORS_LED_STATE_SINGLE_FLASH] = 0x0FU,
	[CANOPEN_INDICATORS_LED_STATE_DOUBLE_FLASH] = 0x0F0FU,
	[CANOPEN_INDICATORS_LED_STATE_TRIPLE_FLASH] = 0x0F0F0FU,
	[CANOPEN_INDICATORS_LED_STATE_QUADRUPLE_FLASH] = 0x0F0F0F0FU,
};

/* Green LED indicator patterns as bitmasks, each bit representing 50 ms */
static const uint32_t canopen_indicators_patterns_green[] = {
	[CANOPEN_INDICATORS_LED_STATE_ON] = 0x1U,
	[CANOPEN_INDICATORS_LED_STATE_OFF] = 0x0U,
	[CANOPEN_INDICATORS_LED_STATE_FLICKERING] = 0x02U,
	[CANOPEN_INDICATORS_LED_STATE_BLINKING] = 0xF0U,
	[CANOPEN_INDICATORS_LED_STATE_SINGLE_FLASH] = 0xF0U,
	/* Green double-flash is reserved for future use in CiA 303-3 */
	[CANOPEN_INDICATORS_LED_STATE_DOUBLE_FLASH] = 0xF0F0U,
	[CANOPEN_INDICATORS_LED_STATE_TRIPLE_FLASH] = 0xF0F0F0U,
	/* Green quadruple-flash is not specified in CiA 303-3, but included here for simplicity */
	[CANOPEN_INDICATORS_LED_STATE_QUADRUPLE_FLASH] = 0xF0F0F0F0U,
};

/* Indicator patterns */
static const uint32_t *canopen_indicators_patterns[] = {
	[CANOPEN_INDICATORS_LED_COLOR_RED] = canopen_indicators_patterns_red,
	[CANOPEN_INDICATORS_LED_COLOR_GREEN] = canopen_indicators_patterns_green,
};

/* Indicator state pattern period lengths in units of 50 ms (bits) */
static const uint8_t canopen_indicators_pattern_lengths[] = {
	[CANOPEN_INDICATORS_LED_STATE_ON] = 1U,
	[CANOPEN_INDICATORS_LED_STATE_OFF] = 1U,
	[CANOPEN_INDICATORS_LED_STATE_FLICKERING] = 2U,
	[CANOPEN_INDICATORS_LED_STATE_BLINKING] = 8U,
	[CANOPEN_INDICATORS_LED_STATE_SINGLE_FLASH] = 24U,
	[CANOPEN_INDICATORS_LED_STATE_DOUBLE_FLASH] = 32U,
	[CANOPEN_INDICATORS_LED_STATE_TRIPLE_FLASH] = 40U,
	[CANOPEN_INDICATORS_LED_STATE_QUADRUPLE_FLASH] = 48U,
};

/* Color map for all colors off */
__maybe_unused static const uint8_t canopen_indicators_color_off[LED_COLOR_ID_MAX] = {0U};

#ifdef CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT
static void canopen_indicators_update_status_led(struct canopen_indicators *indicators, bool *on)
{
	struct canopen_indicators_led *common = &indicators->leds[0];
	const uint8_t *color = canopen_indicators_color_off;
	bool changed = false;
	int err;

	if (on[CANOPEN_INDICATORS_LED_COLOR_RED]) {
		/* Give priority to the red LED to avoid orange/amber light */
		on[CANOPEN_INDICATORS_LED_COLOR_GREEN] = false;
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

	if (on[CANOPEN_INDICATORS_LED_COLOR_RED]) {
		color = indicators->leds[CANOPEN_INDICATORS_LED_COLOR_RED].color;
	} else if (on[CANOPEN_INDICATORS_LED_COLOR_GREEN]) {
		color = indicators->leds[CANOPEN_INDICATORS_LED_COLOR_GREEN].color;
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
		struct canopen_indicators_led *led = &indicators->leds[i];

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

static void canopen_indicators_led_work_handler(struct k_work *work)
{
	struct canopen_indicators *indicators =
		CONTAINER_OF(work, struct canopen_indicators, led_work);
	bool on[ARRAY_SIZE(indicators->leds)];

	for (int i = 0; i < ARRAY_SIZE(indicators->leds); i++) {
		enum canopen_indicators_led_state state = indicators->leds[i].current;
		uint8_t count = indicators->counters[state];

		if (count >= NUM_BITS(uint32_t)) {
			on[i] = false;
		} else {
			on[i] = BIT(count) & canopen_indicators_patterns[i][state];
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

		if (indicators->counters[i] >= canopen_indicators_pattern_lengths[i]) {
			indicators->counters[i] = 0U;
		}
	}

	for (int i = 0; i < ARRAY_SIZE(indicators->leds); i++) {
		enum canopen_indicators_led_state state = indicators->leds[i].current;
		uint8_t count = indicators->counters[state];

		if (count == 0U) {
			/* Advance to next state */
			indicators->leds[i].current = indicators->leds[i].next;
		}
	}
}

static void canopen_indicators_timer_expired(struct k_timer *timer)
{
	struct canopen_indicators *indicators =
		CONTAINER_OF(timer, struct canopen_indicators, timer);
	int err;

	err = k_work_submit_to_queue(indicators->work_q, &indicators->led_work);
	if (err < 0) {
		LOG_ERR("failed to submit LED work item (err %d)", err);
	}
}

int canopen_indicators_set_led_state(struct canopen_indicators *indicators,
				     enum canopen_indicators_led_color color,
				     enum canopen_indicators_led_state state)
{
	__ASSERT_NO_MSG(indicators != NULL);

	if (color < 0 || color >= CANOPEN_INDICATORS_LED_COLOR_MAX) {
		LOG_ERR("invalid indicator LED color %d", color);
		return -EINVAL;
	}

	if (state < 0 || state >= CANOPEN_INDICATORS_LED_STATE_MAX) {
		LOG_ERR("invalid indicator LED state %d", state);
		return -EINVAL;
	}

	indicators->leds[color].next = state;

	return 0;
}

static void canopen_indicators_state_work_handler(struct k_work *work)
{
	struct canopen_indicators *indicators =
		CONTAINER_OF(work, struct canopen_indicators, state_work);
	enum canopen_indicators_led_state led_state;
	atomic_val_t state = atomic_get(indicators->state);

	switch (find_msb_set(state & CANOPEN_INDICATORS_ERROR_LED_STATE_MASK) - 1U) {
	case CANOPEN_INDICATORS_STATE_AUTOBITRATE_OR_LSS:
		led_state = CANOPEN_INDICATORS_LED_STATE_FLICKERING;
		break;
	case CANOPEN_INDICATORS_STATE_INVALID_CONFIGURATION:
		led_state = CANOPEN_INDICATORS_LED_STATE_BLINKING;
		break;
	case CANOPEN_INDICATORS_STATE_WARNING_LIMIT_REACHED:
		led_state = CANOPEN_INDICATORS_LED_STATE_SINGLE_FLASH;
		break;
	case CANOPEN_INDICATORS_STATE_ERROR_CONTROL_EVENT:
		led_state = CANOPEN_INDICATORS_LED_STATE_DOUBLE_FLASH;
		break;
	case CANOPEN_INDICATORS_STATE_SYNC_ERROR:
		led_state = CANOPEN_INDICATORS_LED_STATE_TRIPLE_FLASH;
		break;
	case CANOPEN_INDICATORS_STATE_EVENT_TIMER_ERROR:
		led_state = CANOPEN_INDICATORS_LED_STATE_QUADRUPLE_FLASH;
		break;
	case CANOPEN_INDICATORS_STATE_BUS_OFF:
		led_state = CANOPEN_INDICATORS_LED_STATE_ON;
		break;
	default:
		led_state = CANOPEN_INDICATORS_LED_STATE_OFF;
		break;
	}

	indicators->leds[CANOPEN_INDICATORS_LED_COLOR_RED].next = led_state;

	switch (find_msb_set(state & CANOPEN_INDICATORS_RUN_LED_STATE_MASK) - 1U) {
	case CANOPEN_INDICATORS_STATE_AUTOBITRATE_OR_LSS:
		led_state = CANOPEN_INDICATORS_LED_STATE_FLICKERING;
		break;
	case CANOPEN_INDICATORS_STATE_PRE_OPERATIONAL:
		led_state = CANOPEN_INDICATORS_LED_STATE_BLINKING;
		break;
	case CANOPEN_INDICATORS_STATE_STOPPED:
		led_state = CANOPEN_INDICATORS_LED_STATE_SINGLE_FLASH;
		break;
	case CANOPEN_INDICATORS_STATE_PROGRAM_DOWNLOAD:
		led_state = CANOPEN_INDICATORS_LED_STATE_TRIPLE_FLASH;
		break;
	case CANOPEN_INDICATORS_STATE_OPERATIONAL:
		led_state = CANOPEN_INDICATORS_LED_STATE_ON;
		break;
	default:
		led_state = CANOPEN_INDICATORS_LED_STATE_OFF;
		break;
	}

	indicators->leds[CANOPEN_INDICATORS_LED_COLOR_GREEN].next = led_state;
}

int canopen_indicators_set_state_to(struct canopen_indicators *indicators,
				    enum canopen_indicators_state state, bool val)
{
	int err;

	__ASSERT_NO_MSG(indicators != NULL);

	if (state < 0 || state >= CANOPEN_INDICATORS_STATE_MAX) {
		LOG_ERR("invalid indicator state %d", state);
		return -EINVAL;
	}

	if (atomic_test_and_set_bit_to(indicators->state, state, val)) {
		err = k_work_submit_to_queue(indicators->work_q, &indicators->state_work);
		if (err < 0) {
			LOG_ERR("failed to submit state work item (err %d)", err);
			return err;
		}
	}

	return 0;
}

int canopen_indicators_enable(struct canopen_indicators *indicators)
{
	__ASSERT_NO_MSG(indicators != NULL);

	k_timer_start(&indicators->timer, K_NO_WAIT, K_MSEC(CANOPEN_INDICATORS_PERIOD_MS));

	return 0;
}

#ifdef CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT
static int canopen_indicators_init_color_map(struct canopen_indicators_led *led, uint8_t color)
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
	const uint8_t colors[CANOPEN_INDICATORS_LED_COLOR_MAX] = {
		[CANOPEN_INDICATORS_LED_COLOR_RED] = LED_COLOR_ID_RED,
		[CANOPEN_INDICATORS_LED_COLOR_GREEN] = LED_COLOR_ID_GREEN,
	};
	int err;
#endif /* CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT */

	__ASSERT_NO_MSG(indicators != NULL);
	__ASSERT_NO_MSG(work_q != NULL);

	(void)atomic_clear(indicators->state);

	indicators->work_q = work_q;
	indicators->leds[CANOPEN_INDICATORS_LED_COLOR_RED].led = *red_led;
	indicators->leds[CANOPEN_INDICATORS_LED_COLOR_GREEN].led = *green_led;

	for (int i = 0; i < ARRAY_SIZE(indicators->leds); i++) {
		struct canopen_indicators_led *led = &indicators->leds[i];

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

		led->current = CANOPEN_INDICATORS_LED_STATE_OFF;
		led->next = CANOPEN_INDICATORS_LED_STATE_OFF;
		led->on = false;
	}

#ifdef CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT
	if (red_led->dev == green_led->dev && red_led->index == green_led->index) {
		if (indicators->leds[CANOPEN_INDICATORS_LED_COLOR_RED].num_colors < 2U) {
			LOG_ERR("CANopen status LED with too few colors");
			return -EINVAL;
		}

		indicators->is_status_led = true;
	} else {
		indicators->is_status_led = false;
	}
#endif /* CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT */

	k_timer_init(&indicators->timer, canopen_indicators_timer_expired, NULL);
	k_work_init(&indicators->led_work, canopen_indicators_led_work_handler);
	k_work_init(&indicators->state_work, canopen_indicators_state_work_handler);

	memset(indicators->counters, 0, sizeof(indicators->counters));

	return 0;
}
