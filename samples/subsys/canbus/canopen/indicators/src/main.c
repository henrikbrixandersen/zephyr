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

struct indicators_shell_state_mapping {
	const char *name;
	enum canopen_indicator_state state;
};

#define INDICATORS_SHELL_STATE_MAPPING(_name, _state) {.name = _name, .state = _state}

static const struct indicators_shell_state_mapping indicators_shell_state_map[] = {
	INDICATORS_SHELL_STATE_MAPPING("on", CANOPEN_INDICATOR_STATE_ON),
	INDICATORS_SHELL_STATE_MAPPING("off", CANOPEN_INDICATOR_STATE_OFF),
	INDICATORS_SHELL_STATE_MAPPING("flicker", CANOPEN_INDICATOR_STATE_FLICKERING),
	INDICATORS_SHELL_STATE_MAPPING("blinking", CANOPEN_INDICATOR_STATE_BLINKING),
	INDICATORS_SHELL_STATE_MAPPING("single", CANOPEN_INDICATOR_STATE_SINGLE_FLASH),
	INDICATORS_SHELL_STATE_MAPPING("double", CANOPEN_INDICATOR_STATE_DOUBLE_FLASH),
	INDICATORS_SHELL_STATE_MAPPING("triple", CANOPEN_INDICATOR_STATE_TRIPLE_FLASH),
	INDICATORS_SHELL_STATE_MAPPING("quadruple", CANOPEN_INDICATOR_STATE_QUADRUPLE_FLASH),
};

static const char * const indicators_shell_names[] = {
	"red",
	"green",
	"both",
};

#if DT_HAS_ALIAS(canopen_status_led)
/* Dedicated bicolor CANopen "status" LED */
struct led_dt_spec red_led = LED_DT_SPEC_GET(DT_ALIAS(canopen_status_led));
struct led_dt_spec green_led = LED_DT_SPEC_GET(DT_ALIAS(canopen_status_led));
#elif (DT_HAS_ALIAS(canopen_error_led) && DT_HAS_ALIAS(canopen_run_led))
/* Dedicated red CANopen "error" LED + green CANopen "run" LED */
struct led_dt_spec red_led = LED_DT_SPEC_GET(DT_ALIAS(canopen_error_led));
struct led_dt_spec green_led = LED_DT_SPEC_GET(DT_ALIAS(canopen_run_led));
#elif (DT_NODE_EXISTS(DT_NODELABEL(red_led)) && DT_NODE_EXISTS(DT_NODELABEL(green_led)))
/* Red LED + green LED */
struct led_dt_spec red_led = LED_DT_SPEC_GET(DT_NODELABEL(red_led));
struct led_dt_spec green_led = LED_DT_SPEC_GET(DT_NODELABEL(green_led));
#else
#error "Required LEDs not present in devicetree"
#endif

struct canopen_indicators indicators;

static int cmd_indicators_set_state(const struct shell *sh, size_t argc, char **argv)
{
	const struct indicators_shell_state_mapping *map = NULL;
	bool green = false;
	bool red = false;
	int err;

	for (int i = 0; i < ARRAY_SIZE(indicators_shell_state_map); i++) {
		if (strcmp(argv[2], indicators_shell_state_map[i].name) == 0) {
			map = &indicators_shell_state_map[i];
			break;
		}
	}

	if (map == NULL) {
		shell_error(sh, "invalid mode: %s", argv[2]);
		return -EINVAL;
	}

	if (strcmp(argv[1], "red") == 0) {
		shell_print(sh, "setting red indicator state to %s", map->name);
		red = true;
	} else if (strcmp(argv[1], "green") == 0) {
		shell_print(sh, "setting green indicator state to %s", map->name);
		green = true;
	} else if (strcmp(argv[1], "both") == 0) {
		shell_print(sh, "setting both indicator states to %s", map->name);
		red = true;
		green = true;
	} else {
		shell_error(sh, "invalid indicator name: %s", argv[1]);
		return -EINVAL;
	}

	if (red) {
		err = canopen_indicators_set_state_red(&indicators, map->state);
		if (err != 0) {
			shell_error(sh, "failed to set red indicator state (err %d)", err);
			return err;
		}
	}

	if (green) {
		err = canopen_indicators_set_state_green(&indicators, map->state);
		if (err != 0) {
			shell_error(sh, "failed to set green indicator state (err %d)", err);
			return err;
		}
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

static void cmd_indicators_name(size_t idx, struct shell_static_entry *entry)
{
	if (idx < ARRAY_SIZE(indicators_shell_names)) {
		entry->syntax = indicators_shell_names[idx];

	} else {
		entry->syntax = NULL;
	}

	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = &dsub_indicators_state;
}

SHELL_DYNAMIC_CMD_CREATE(dsub_indicators_name, cmd_indicators_name);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_indicators_cmds,
	SHELL_CMD_ARG(set, &dsub_indicators_name,
		      SHELL_HELP("Set CANopen indicator state",
				 "<red|green|both> <state>"),
		      cmd_indicators_set_state, 3, 0),
	SHELL_SUBCMD_SET_END
);

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
