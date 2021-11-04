/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @ingroup canopen_nmt
 * @brief CANopen Indicators
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_CANOPEN_INDICATORS_H_
#define ZEPHYR_INCLUDE_CANBUS_CANOPEN_INDICATORS_H_

/**
 * @brief CANopen Indicators
 * @defgroup canopen_nmt CANopen Indicators
 * @ingroup canopen
 * @{
 */

#include <zephyr/canbus/canopen/nmt.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/dt-bindings/led/led.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/__assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CANopen indicator states
 *
 * @see CiA 303-3, table 1
 */
enum canopen_indicator_state {
	/**
	 * @brief LED constantly on.
	 */
	CANOPEN_INDICATOR_STATE_ON,
	/**
	 * @brief LED constantly off.
	 */
	CANOPEN_INDICATOR_STATE_OFF,
	/**
	 * @brief Isophase on/off @ ~10 Hz; on for ~50 ms, off for ~50 ms.
	 */
	CANOPEN_INDICATOR_STATE_FLICKERING,
	/**
	 * @brief Isophase on/off @ ~2.5 Hz; on for ~200 ms, off for ~200 ms.
	 */
	CANOPEN_INDICATOR_STATE_BLINKING,
	/**
	 * @brief One flash (~200 ms) followed by an off phase (~1000 ms).
	 */
	CANOPEN_INDICATOR_STATE_SINGLE_FLASH,
	/**
	 * @brief Two flashes (~200 ms), separated by an off phase (~200 ms). Finished by an off
	 * phase (~1000 ms).
	 */
	CANOPEN_INDICATOR_STATE_DOUBLE_FLASH,
	/**
	 * @brief Three flashes (~200 ms), separated by an off phase (~200 ms). Finished by an off
	 * phase (~1000 ms).
	 */
	CANOPEN_INDICATOR_STATE_TRIPLE_FLASH,
	/**
	 * @brief Four flashes (~200 ms), separated by an off phase (~200 ms). Finished by an off
	 * phase (~1000 ms).
	 */
	CANOPEN_INDICATOR_STATE_QUADRUPLE_FLASH,
	/**
	 * @brief Number of states.
	 */
	CANOPEN_INDICATOR_STATE_MAX,
};

/**
 * CANopen indicator colors
 *
 * @see CiA 303-3, section 4.1
 */
enum canopen_indicator_color {
	/** Red LED */
	CANOPEN_INDICATOR_COLOR_RED,
	/** Green LED */
	CANOPEN_INDICATOR_COLOR_GREEN,
	/** Number of colors. */
	CANOPEN_INDICATOR_COLOR_MAX,
};

/** @brief CANopen Indicators
 *
 * This type is opaque. Member data should not be accessed directly by the application.
 */
struct canopen_indicators {
	struct canopen_indicator_led {
		/** LED indicator. */
		struct led_dt_spec led;
#ifdef CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT
		/** Number of colors of this LED. */
		uint8_t num_colors;
		/** Color mapping for this LED. */
		uint8_t color[LED_COLOR_ID_MAX];
#endif /* CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT */
		/** LED state. */
		enum canopen_indicator_state state;
		/** Current state (on/off). */
		bool on;
	} leds[CANOPEN_INDICATOR_COLOR_MAX];
#ifdef CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT
	/** True if the red/green LEDs are the same physical LED, false otherwise. */
	bool is_status_led;
#endif /* CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT */
	/** Indicator state processing timer. */
	struct k_timer timer;
	/** Indicator state processing work queue. */
	struct k_work_q *work_q;
	/** Indicator state processing work queue item. */
	struct k_work work;
	/** Indicator state pattern bit counters. */
	uint8_t counters[CANOPEN_INDICATOR_STATE_MAX];
};

/**
 * @brief Initialize CANopen indicators
 *
 * The CANopen indicators must be initialized prior to calling any other CANopen indicators API
 * functions.
 *
 * @note If using one bicolor red/green LED (a CANopen "status" LED as opposed to a red CANopen
 * "error" LED and a green CANopen "run" LED), the same multicolor LED specification should be
 * passed as both @a red_led and @a green_led. This functionality depends on
 * @kconfig{CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT} being enabled.
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param work_q Pointer to the work queue to be used by the CANopen indicators.
 * @param red_led Pointer to the red LED as specified in devicetree.
 * @param green_led Pointer to the green LED as specified in devicetree.
 * @retval 0 on success, negative errno value on failure.
 * @retval -EIO Configuration of the indicator devices failed.
 * @retval -ENODEV LED device not available.
 * @retval -ENOTSUP LED device with unsupported configuration.
 */
int canopen_indicators_init(struct canopen_indicators *indicators, struct k_work_q *work_q,
			    const struct led_dt_spec *red_led, const struct led_dt_spec *green_led);

/**
 * @brief Enable CANopen Indicators
 *
 * Enable the CANopen Indicators.
 *
 * @param indicators Pointer to the CANopen indicators.
 * @return 0 on success, negative errno value on failure.
 */
int canopen_indicators_enable(struct canopen_indicators *indicators);

/**
 * @brief Set the state of a CANopen indicator
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param color the color of the indicator to set.
 * @param state Indicator state.
 * @retval 0 on success, negative errno value on failure.
 */
int canopen_indicators_set_state(struct canopen_indicators *indicators,
				 enum canopen_indicator_color color,
				 enum canopen_indicator_state state);

/**
 * @brief Set the state of the red CANopen indicator
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param state Indicator state.
 * @retval 0 on success, negative errno value on failure.
 */
static inline int canopen_indicators_set_state_red(struct canopen_indicators *indicators,
						   enum canopen_indicator_state state)
{
	return canopen_indicators_set_state(indicators, CANOPEN_INDICATOR_COLOR_RED, state);
}

/**
 * @brief Set the state of the green CANopen indicator
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param state Indicator state.
 * @retval 0 on success, negative errno value on failure.
 */
static inline int canopen_indicators_set_state_green(struct canopen_indicators *indicators,
						     enum canopen_indicator_state state)
{
	return canopen_indicators_set_state(indicators, CANOPEN_INDICATOR_COLOR_GREEN, state);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_INDICATORS_H_ */
