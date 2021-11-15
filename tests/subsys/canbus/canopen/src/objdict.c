/*
 * Copyright (c) 2021-2023 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen/od.h>
#include <zephyr/types.h>

#include "objdict.h"

CANOPEN_OD_DEFINE_OBJECTS(objdict,
	/* 1000h - Device type */
	CANOPEN_OD_OBJECT_ENTRIES(0x1000U,
		CANOPEN_OD_ENTRY_UNSIGNED32(0U, &(uint32_t){ 0U }, NULL, NULL,
					CANOPEN_OD_ATTR_ACCESS_READ),
	),
	/* 1001h - Error register */
	CANOPEN_OD_OBJECT_ENTRIES(0x1001U,
		CANOPEN_OD_ENTRY_UNSIGNED32(0U, &(uint32_t){ 0U }, NULL, NULL,
					CANOPEN_OD_ATTR_ACCESS_READ),
	),
	/* 1018h - Identity object */
	CANOPEN_OD_OBJECT_ENTRIES(0x1018U,
		CANOPEN_OD_ENTRY_UNSIGNED8(0U, &(uint8_t){ 4U }, NULL, NULL,
					CANOPEN_OD_ATTR_ACCESS_READ),
		CANOPEN_OD_ENTRY_UNSIGNED32(1U, &(uint32_t){ 0U }, NULL, NULL,
					CANOPEN_OD_ATTR_ACCESS_READ),
		CANOPEN_OD_ENTRY_UNSIGNED32(2U, &(uint32_t){ 0xdeadbeefU }, NULL, NULL,
					CANOPEN_OD_ATTR_ACCESS_READ),
		CANOPEN_OD_ENTRY_UNSIGNED32(3U, &(uint32_t){ 42U }, NULL, NULL,
					CANOPEN_OD_ATTR_ACCESS_READ),
		CANOPEN_OD_ENTRY_UNSIGNED32(4U, &(uint32_t){ 1331U }, NULL, NULL,
					CANOPEN_OD_ATTR_ACCESS_READ),
	),
);
