/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen/od.h>
#include <zephyr/ztest.h>

#include "objdict.h"

ZTEST(canopen_od, test_canopen_od_find)
{
	canopen_od_handle_t handle;

	handle = canopen_od_find(&objdict, 0x1004U, 0U);
	zassert_false(canopen_od_handle_is_valid(handle), "Object 1004h not present");

	handle = canopen_od_find(&objdict, 0x1000U, 0U);
	zassert_true(canopen_od_handle_is_valid(handle), "Object 1000h is present");

	handle = canopen_od_find(&objdict, 0x1001U, 0U);
	zassert_true(canopen_od_handle_is_valid(handle), "Object 1001h is present");
	/* zassert_equal(obj->index, 0x1001, "Object index is 1001h"); */

	handle = canopen_od_find(&objdict, 0x1018U, 0U);
	zassert_true(canopen_od_handle_is_valid(handle), "Object 1018h is present");
	/* zassert_equal(obj->index, 0x1018, "Object index is 1018h"); */

	handle = canopen_od_find(&objdict, 0x1018U, 1U);
	zassert_true(canopen_od_handle_is_valid(handle),
		     "Object index 1018h sub-index 1 is present");
	/* zassert_equal(entry->subindex, 1U, "Object index 1018h sub-index 1 matches"); */

	handle = canopen_od_find_by_handle(&objdict, handle, 2U);
	zassert_true(canopen_od_handle_is_valid(handle),
		     "Object index 1018h sub-index 2 is present");
	/* zassert_equal(entry->subindex, 1U, "Object index 1018h sub-index 1 matches"); */

	handle = canopen_od_find_by_handle(&objdict, handle, 3U);
	zassert_true(canopen_od_handle_is_valid(handle),
		     "Object index 1018h sub-index 3 is present");
	/* zassert_equal(entry->subindex, 1U, "Object index 1018h sub-index 1 matches"); */

	handle = canopen_od_find_by_handle(&objdict, handle, 4U);
	zassert_true(canopen_od_handle_is_valid(handle),
		     "Object index 1018h sub-index 4 is present");
	/* zassert_equal(entry->subindex, 1U, "Object index 1018h sub-index 1 matches"); */

	handle = canopen_od_find_by_handle(&objdict, handle, 5U);
	zassert_false(canopen_od_handle_is_valid(handle),
		      "Object index 1018h sub-index 5 is not present");
}

ZTEST_SUITE(canopen_od, NULL, NULL, NULL, NULL, NULL);
