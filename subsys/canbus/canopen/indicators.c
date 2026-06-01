/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

LOG_MODULE_REGISTER(canopen_indicators, CONFIG_CANOPEN_LOG_LEVEL);

/* Indicator states (CiA 303-3, table 1) */
enum canopen_indicator_state {
	/* LED constantly on */
	CANOPEN_INDICATOR_STATE_LED_ON,
	/* LED constantly off. */
	CANOPEN_INDICATOR_STATE_LED_OFF,
	/* Isophase on/off @ ~10 Hz; on for ~50 ms, off for ~50 ms. */
	CANOPEN_INDICATOR_STATE_LED_FLICKERING,
	/* Isophase on/off @ ~2.5 Hz; on for ~200 ms, off for ~200 ms. */
	CANOPEN_INDICATOR_STATE_LED_BLINKING,
	/* One flash (~200 ms) followed by a long off phase (~1000 ms). */
	CANOPEN_INDICATOR_STATE_LED_SINGLE_FLASH,
	/*
	 * Two flashes (~200 ms), separated by an off phase (~200 ms). Finished by a off phase
	 * (~1000 ms).
	 */
	CANOPEN_INDICATOR_STATE_LED_DOUBLE_FLASH,
	/*
	 * Three flashes (~200 ms), separated by an off phase (~200 ms). Finished by a long
	 * off phase (~1000 ms).
	 */
	CANOPEN_INDICATOR_STATE_LED_TRIPLE_FLASH,
	/*
	 * Four flashes (~200 ms), separated by an off phase (~200 ms). Finished by a long off
	 * phase (~1000 ms).
	 */
	CANOPEN_INDICATOR_STATE_LED_QUADRUPLE_FLASH,
};
