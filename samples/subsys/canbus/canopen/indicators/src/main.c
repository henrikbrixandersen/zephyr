/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <zephyr/canbus/canopen.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "sample_canopen_leds.h"

struct indicators_shell_led_state_map {
	const char *name;
	enum canopen_indicators_led_state state;
};

#define INDICATORS_SHELL_LED_STATE_MAP(_name, _state) {.name = _name, .state = _state}

static const struct indicators_shell_led_state_map indicators_shell_led_state_map[] = {
	INDICATORS_SHELL_LED_STATE_MAP("on", CANOPEN_INDICATORS_LED_STATE_ON),
	INDICATORS_SHELL_LED_STATE_MAP("off", CANOPEN_INDICATORS_LED_STATE_OFF),
	INDICATORS_SHELL_LED_STATE_MAP("flickering", CANOPEN_INDICATORS_LED_STATE_FLICKERING),
	INDICATORS_SHELL_LED_STATE_MAP("blinking", CANOPEN_INDICATORS_LED_STATE_BLINKING),
	INDICATORS_SHELL_LED_STATE_MAP("single", CANOPEN_INDICATORS_LED_STATE_SINGLE_FLASH),
	INDICATORS_SHELL_LED_STATE_MAP("double", CANOPEN_INDICATORS_LED_STATE_DOUBLE_FLASH),
	INDICATORS_SHELL_LED_STATE_MAP("triple", CANOPEN_INDICATORS_LED_STATE_TRIPLE_FLASH),
	INDICATORS_SHELL_LED_STATE_MAP("quadruple", CANOPEN_INDICATORS_LED_STATE_QUADRUPLE_FLASH),
};

static const char *const indicators_shell_led_names[] = {
	"red",
	"green",
	"both",
};

struct indicators_shell_state_map {
	const char *name;
	enum canopen_indicators_state state;
};

#define INDICATORS_SHELL_STATE_MAP(_name, _state) {.name = _name, .state = _state}

static const struct indicators_shell_state_map indicators_shell_state_map[] = {
	INDICATORS_SHELL_STATE_MAP("autobitrate-or-lss",
				   CANOPEN_INDICATORS_STATE_AUTOBITRATE_OR_LSS),
	INDICATORS_SHELL_STATE_MAP("invalid-configuration",
				   CANOPEN_INDICATORS_STATE_INVALID_CONFIGURATION),
	INDICATORS_SHELL_STATE_MAP("warning-limit-reached",
				   CANOPEN_INDICATORS_STATE_WARNING_LIMIT_REACHED),
	INDICATORS_SHELL_STATE_MAP("error-control-event",
				   CANOPEN_INDICATORS_STATE_ERROR_CONTROL_EVENT),
	INDICATORS_SHELL_STATE_MAP("sync-error", CANOPEN_INDICATORS_STATE_SYNC_ERROR),
	INDICATORS_SHELL_STATE_MAP("event-timer-error", CANOPEN_INDICATORS_STATE_EVENT_TIMER_ERROR),
	INDICATORS_SHELL_STATE_MAP("bus-off", CANOPEN_INDICATORS_STATE_BUS_OFF),
	INDICATORS_SHELL_STATE_MAP("pre-operational", CANOPEN_INDICATORS_STATE_PRE_OPERATIONAL),
	INDICATORS_SHELL_STATE_MAP("stopped", CANOPEN_INDICATORS_STATE_STOPPED),
	INDICATORS_SHELL_STATE_MAP("program-download", CANOPEN_INDICATORS_STATE_PROGRAM_DOWNLOAD),
	INDICATORS_SHELL_STATE_MAP("operational", CANOPEN_INDICATORS_STATE_OPERATIONAL),
};

struct canopen_indicators indicators;

static int cmd_indicators_set_led_state(const struct shell *sh, size_t argc, char **argv)
{
	const struct indicators_shell_led_state_map *map = NULL;
	bool green = false;
	bool red = false;
	int err;

	for (int i = 0; i < ARRAY_SIZE(indicators_shell_led_state_map); i++) {
		if (strcmp(argv[2], indicators_shell_led_state_map[i].name) == 0) {
			map = &indicators_shell_led_state_map[i];
			break;
		}
	}

	if (map == NULL) {
		shell_error(sh, "invalid indicators LED mode: %s", argv[2]);
		return -EINVAL;
	}

	if (strcmp(argv[1], "red") == 0) {
		shell_print(sh, "setting red LED indicators state to %s", map->name);
		red = true;
	} else if (strcmp(argv[1], "green") == 0) {
		shell_print(sh, "setting green LED indicators state to %s", map->name);
		green = true;
	} else if (strcmp(argv[1], "both") == 0) {
		shell_print(sh, "setting both LED indicators states to %s", map->name);
		red = true;
		green = true;
	} else {
		shell_error(sh, "invalid indicators LED name: %s", argv[1]);
		return -EINVAL;
	}

	if (red) {
		err = canopen_indicators_set_led_state_red(&indicators, map->state);
		if (err != 0) {
			shell_error(sh, "failed to set red LED indicators state (err %d)", err);
			return err;
		}
	}

	if (green) {
		err = canopen_indicators_set_led_state_green(&indicators, map->state);
		if (err != 0) {
			shell_error(sh, "failed to set green LED indicators state (err %d)", err);
			return err;
		}
	}

	return 0;
}

static void cmd_indicators_led_state(size_t idx, struct shell_static_entry *entry)
{
	if (idx < ARRAY_SIZE(indicators_shell_led_state_map)) {
		entry->syntax = indicators_shell_led_state_map[idx].name;

	} else {
		entry->syntax = NULL;
	}

	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

SHELL_DYNAMIC_CMD_CREATE(dsub_indicators_led_state, cmd_indicators_led_state);

static void cmd_indicators_led_name(size_t idx, struct shell_static_entry *entry)
{
	if (idx < ARRAY_SIZE(indicators_shell_led_names)) {
		entry->syntax = indicators_shell_led_names[idx];

	} else {
		entry->syntax = NULL;
	}

	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = &dsub_indicators_led_state;
}

SHELL_DYNAMIC_CMD_CREATE(dsub_indicators_led_name, cmd_indicators_led_name);

static int cmd_indicators_set_state(const struct shell *sh, size_t argc, char **argv)
{
	const struct indicators_shell_state_map *map = NULL;
	int err;

	for (int i = 0; i < ARRAY_SIZE(indicators_shell_state_map); i++) {
		if (strcmp(argv[1], indicators_shell_state_map[i].name) == 0) {
			map = &indicators_shell_state_map[i];
			break;
		}
	}

	if (map == NULL) {
		shell_error(sh, "invalid indicators state: %s", argv[1]);
		return -EINVAL;
	}

	shell_print(sh, "setting indicators state %s", map->name);

	err = canopen_indicators_set_state(&indicators, map->state);
	if (err != 0) {
		shell_error(sh, "failed to set indicators state (err %d)", err);
		return err;
	}

	return 0;
}

static int cmd_indicators_clear_state(const struct shell *sh, size_t argc, char **argv)
{
	const struct indicators_shell_state_map *map = NULL;
	int err;

	for (int i = 0; i < ARRAY_SIZE(indicators_shell_state_map); i++) {
		if (strcmp(argv[1], indicators_shell_state_map[i].name) == 0) {
			map = &indicators_shell_state_map[i];
			break;
		}
	}

	if (map == NULL) {
		shell_error(sh, "invalid indicators state: %s", argv[1]);
		return -EINVAL;
	}

	shell_print(sh, "clearing indicators state %s", map->name);

	err = canopen_indicators_clear_state(&indicators, map->state);
	if (err != 0) {
		shell_error(sh, "failed to clear indicators state (err %d)", err);
		return err;
	}

	return 0;
}

static void cmd_indicators_state(size_t idx, struct shell_static_entry *entry)
{
	if (idx < ARRAY_SIZE(indicators_shell_state_map)) {
		entry->syntax = indicators_shell_state_map[idx].name;

	} else {
		entry->syntax = NULL;
	}

	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;
}

SHELL_DYNAMIC_CMD_CREATE(dsub_indicators_state, cmd_indicators_state);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_indicators_cmds,
	SHELL_CMD_ARG(led, &dsub_indicators_led_name,
		      SHELL_HELP("Set CANopen indicators LED state", "<red|green|both> <state>"),
		      cmd_indicators_set_led_state, 3, 0),
	SHELL_CMD_ARG(set, &dsub_indicators_state,
		      SHELL_HELP("Set CANopen indicators state", "<state>"),
		      cmd_indicators_set_state, 2, 0),
	SHELL_CMD_ARG(clear, &dsub_indicators_state,
		      SHELL_HELP("Clear CANopen indicators state", "<state>"),
		      cmd_indicators_clear_state, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(indicators, &sub_indicators_cmds, "CANopen indicators commands", NULL);

int main(void)
{
	int err;

	err = canopen_indicators_init(&indicators, &k_sys_work_q, &red_led, &green_led);
	if (err != 0) {
		printf("failed to initialise CANopen indicators (err %d)\n", err);
		return err;
	}

	err = canopen_indicators_enable(&indicators);
	if (err != 0) {
		printf("failed to enable CANopen indicators (err %d)\n", err);
		return err;
	}

	return 0;
}
