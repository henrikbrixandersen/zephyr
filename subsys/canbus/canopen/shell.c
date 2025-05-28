/*
 * Copyright (c) 2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/canbus/canopen.h>
#include <zephyr/shell/shell.h>

static const struct canopen_od *canopen_shell_get_od(const char *name)
{
	STRUCT_SECTION_FOREACH(canopen_od_info, info) {
		if (info->name != NULL && strcmp(name, info->name) == 0U) {
			return info->od;
		}
	}

	return NULL;
}

static int canopen_shell_od_dump_entry_callback(const struct canopen_od *od,
						canopen_od_handle_t handle, void *user_data)
{
	const struct shell *sh = user_data;
	uint16_t index;
	uint8_t subindex;
	int err;

	err = canopen_od_handle_get_index(od, handle, &index);
	if (err != 0) {
		return err;
	}

	err = canopen_od_handle_get_subindex(od, handle, &subindex);
	if (err != 0) {
		return err;
	}

	/* TODO */
	if (subindex == 0U) {
		shell_print(sh, "%04xh:", index);
	}

	shell_print(sh, "\t%d:", subindex);

	return 0;
}

static int cmd_canopen_od_dump(const struct shell *sh, size_t argc, char **argv)
{
	const struct canopen_od *od = canopen_shell_get_od(argv[1]);
	int err;

	if (od == NULL) {
		shell_error(sh, "objdict %s not found", argv[1]);
		return -EINVAL;
	}

	shell_print(sh, "dumping %s", argv[1]);

	err = canopen_od_foreach_entry(od, canopen_shell_od_dump_entry_callback, (void *)sh);
	if (err != 0) {
		shell_error(sh, "failed to dump objdict (err %d)", err);
		return err;
	}

	return 0;
}

static void cmd_canopen_od_name(size_t idx, struct shell_static_entry *entry)
{
	size_t match_idx = 0U;

	entry->syntax = NULL;
	entry->handler = NULL;
	entry->help = NULL;
	entry->subcmd = NULL;

	STRUCT_SECTION_FOREACH(canopen_od_info, info) {
		if (info->name != NULL && strlen(info->name) != 0U) {
			if (match_idx == idx) {
				entry->syntax = info->name;
				break;
			}

			match_idx++;
		}
	}
}

SHELL_DYNAMIC_CMD_CREATE(dsub_canopen_od_name, cmd_canopen_od_name);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_canopen_od_cmds,
	SHELL_CMD_ARG(dump, &dsub_canopen_od_name,
		"Dump CANopen object dictionary\n"
		"Usage: canopen od dump <OBJDICT>\n",
		cmd_canopen_od_dump, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_canopen_cmds,
	SHELL_CMD(od, &sub_canopen_od_cmds,
		"CANopen object dictionary commands\n"
		"Usage: can od <dump> <OBJDICT> ...",
		NULL),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(canopen, &sub_canopen_cmds, "CANopen commands", NULL);
