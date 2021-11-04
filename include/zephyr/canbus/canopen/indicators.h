/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @ingroup canopen_indicators
 * @brief CANopen Indicators
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_CANOPEN_INDICATORS_H_
#define ZEPHYR_INCLUDE_CANBUS_CANOPEN_INDICATORS_H_

/**
 * @brief CANopen Indicators
 * @defgroup canopen_indicators CANopen Indicators
 * @ingroup canopen
 * @{
 */

#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/dt-bindings/led/led.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CANopen indicators LED states
 *
 * @see CiA 303-3, table 1
 */
enum canopen_indicators_led_state {
	/**
	 * @brief LED constantly on.
	 */
	CANOPEN_INDICATORS_LED_STATE_ON,
	/**
	 * @brief LED constantly off.
	 */
	CANOPEN_INDICATORS_LED_STATE_OFF,
	/**
	 * @brief Isophase on/off @ ~10 Hz; on for ~50 ms, off for ~50 ms.
	 */
	CANOPEN_INDICATORS_LED_STATE_FLICKERING,
	/**
	 * @brief Isophase on/off @ ~2.5 Hz; on for ~200 ms, off for ~200 ms.
	 */
	CANOPEN_INDICATORS_LED_STATE_BLINKING,
	/**
	 * @brief One flash (~200 ms) followed by an off phase (~1000 ms).
	 */
	CANOPEN_INDICATORS_LED_STATE_SINGLE_FLASH,
	/**
	 * @brief Two flashes (~200 ms), separated by an off phase (~200 ms). Finished by an off
	 * phase (~1000 ms).
	 */
	CANOPEN_INDICATORS_LED_STATE_DOUBLE_FLASH,
	/**
	 * @brief Three flashes (~200 ms), separated by an off phase (~200 ms). Finished by an off
	 * phase (~1000 ms).
	 */
	CANOPEN_INDICATORS_LED_STATE_TRIPLE_FLASH,
	/**
	 * @brief Four flashes (~200 ms), separated by an off phase (~200 ms). Finished by an off
	 * phase (~1000 ms).
	 */
	CANOPEN_INDICATORS_LED_STATE_QUADRUPLE_FLASH,
	/**
	 * @brief Number of LED states.
	 */
	CANOPEN_INDICATORS_LED_STATE_MAX,
};

/**
 * CANopen indicators LED colors
 *
 * @see CiA 303-3, section 4.1
 */
enum canopen_indicators_led_color {
	/** Red LED */
	CANOPEN_INDICATORS_LED_COLOR_RED,
	/** Green LED */
	CANOPEN_INDICATORS_LED_COLOR_GREEN,
	/** Number of LED colors. */
	CANOPEN_INDICATORS_LED_COLOR_MAX,
};

/**
 * CANopen indicators states
 *
 * @see CiA 303-3, table 2 and table 3
 */
enum canopen_indicators_state {
	/**
	 * @brief AutoBitrate/LSS.
	 */
	CANOPEN_INDICATORS_STATE_AUTOBITRATE_OR_LSS,
	/**
	 * @brief Invalid configuration.
	 */
	CANOPEN_INDICATORS_STATE_INVALID_CONFIGURATION,
	/**
	 * @brief Warning limit reached.
	 */
	CANOPEN_INDICATORS_STATE_WARNING_LIMIT_REACHED,
	/**
	 * @brief Error control event.
	 */
	CANOPEN_INDICATORS_STATE_ERROR_CONTROL_EVENT,
	/**
	 * @brief Sync error.
	 */
	CANOPEN_INDICATORS_STATE_SYNC_ERROR,
	/**
	 * @brief Event-timer error.
	 */
	CANOPEN_INDICATORS_STATE_EVENT_TIMER_ERROR,
	/**
	 * @brief Bus off.
	 */
	CANOPEN_INDICATORS_STATE_BUS_OFF,
	/**
	 * @brief Pre-operational.
	 */
	CANOPEN_INDICATORS_STATE_PRE_OPERATIONAL,
	/**
	 * @brief Stopped.
	 */
	CANOPEN_INDICATORS_STATE_STOPPED,
	/**
	 * @brief Program/firmware download.
	 */
	CANOPEN_INDICATORS_STATE_PROGRAM_DOWNLOAD,
	/**
	 * @brief Operational.
	 */
	CANOPEN_INDICATORS_STATE_OPERATIONAL,
	/**
	 * @brief Number of states.
	 */
	CANOPEN_INDICATORS_STATE_MAX,
};

/** @brief CANopen Indicators
 *
 * This type is opaque. Member data should not be accessed directly by the application.
 */
struct canopen_indicators {
	struct canopen_indicators_led {
		/** LED indicator. */
		struct led_dt_spec led;
#ifdef CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT
		/** Number of colors of this LED. */
		uint8_t num_colors;
		/** Color mapping for this LED. */
		uint8_t color[LED_COLOR_ID_MAX];
#endif /* CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT */
		/** Current LED state. */
		enum canopen_indicators_led_state current;
		/** Next LED state. */
		enum canopen_indicators_led_state next;
		/** Current state (on/off). */
		bool on;
	} leds[CANOPEN_INDICATORS_LED_COLOR_MAX];
#ifdef CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT
	/** True if the red/green LEDs are the same physical LED, false otherwise. */
	bool is_status_led;
#endif /* CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT */
	/** Indicators state bitmap. */
	ATOMIC_DEFINE(state, CANOPEN_INDICATORS_STATE_MAX);
	/** Indicator state processing timer. */
	struct k_timer timer;
	/** Indicator state processing work queue. */
	struct k_work_q *work_q;
	/** Indicator LED state processing work queue item. */
	struct k_work led_work;
	/** Indicator state processing work queue item. */
	struct k_work state_work;
	/** Indicator state pattern bit counters. */
	uint8_t counters[CANOPEN_INDICATORS_LED_STATE_MAX];
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
 * @brief Set the state of a CANopen indicator LED
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param color the color of the indicator LED to set.
 * @param state Indicator LED state.
 * @retval 0 on success, negative errno value on failure.
 */
int canopen_indicators_set_led_state(struct canopen_indicators *indicators,
				     enum canopen_indicators_led_color color,
				     enum canopen_indicators_led_state state);

/**
 * @brief Set the state of the red CANopen indicator LED
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param state Indicator LED state.
 * @retval 0 on success, negative errno value on failure.
 */
static inline int canopen_indicators_set_led_state_red(struct canopen_indicators *indicators,
						       enum canopen_indicators_led_state state)
{
	return canopen_indicators_set_led_state(indicators, CANOPEN_INDICATORS_LED_COLOR_RED,
						state);
}

/**
 * @brief Set the state of the green CANopen indicator LED
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param state Indicator LED state.
 * @retval 0 on success, negative errno value on failure.
 */
static inline int canopen_indicators_set_led_state_green(struct canopen_indicators *indicators,
							 enum canopen_indicators_led_state state)
{
	return canopen_indicators_set_led_state(indicators, CANOPEN_INDICATORS_LED_COLOR_GREEN,
						state);
}

/**
 * @brief Set a state of the CANopen indicators to a given value
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param state Indicator state to set.
 * @param val Value to set.
 * @retval 0 on success, negative errno value on failure.
 */
int canopen_indicators_set_state_to(struct canopen_indicators *indicators,
				    enum canopen_indicators_state state, bool val);


/**
 * @brief Set a state of the CANopen indicators
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param state Indicator state to set.
 * @retval 0 on success, negative errno value on failure.
 */
static inline int canopen_indicators_set_state(struct canopen_indicators *indicators,
					       enum canopen_indicators_state state)
{
	return canopen_indicators_set_state_to(indicators, state, true);
}

/**
 * @brief Clear a state of the CANopen indicators
 *
 * @param indicators Pointer to the CANopen indicators.
 * @param state Indicator state to clear.
 * @retval 0 on success, negative errno value on failure.
 */
static inline int canopen_indicators_clear_state(struct canopen_indicators *indicators,
						 enum canopen_indicators_state state)
{
	return canopen_indicators_set_state_to(indicators, state, false);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_INDICATORS_H_ */
