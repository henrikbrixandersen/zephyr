/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen/od.h>
#include <zephyr/ztest.h>

static uint32_t value;
CANOPEN_OD_DEFINE_OBJECTS(od,
	CANOPEN_OD_OBJECT_ENTRIES(0x0U,
		CANOPEN_OD_ENTRY_UNSIGNED32(0U, &value, NULL, NULL, 0U),
	),
);

ZTEST(canopen_od_deftypes, test_unsigned32)
{
	canopen_od_handle_t handle;
	uint16_t type;

	handle = canopen_od_find(&od, 0U, 0U);
	zassert_true(canopen_od_handle_is_valid(handle), "object is not present");

	zassert_ok(canopen_od_get_type_by_handle(&od, handle, &type), "failed to get type");
	zassert_equal(type, CANOPEN_OD_DEFTYPE_UNSIGNED32, "entry has wrong type");
}

ZTEST_SUITE(canopen_od_deftypes, NULL, NULL, NULL, NULL, NULL);
