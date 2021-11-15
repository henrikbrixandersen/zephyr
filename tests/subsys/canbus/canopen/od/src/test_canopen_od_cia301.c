/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen/od.h>
#include <zephyr/ztest.h>

#include "test-cia301.h"

static void validate_entry_not_present(uint16_t index, uint8_t subindex)
{
	const struct canopen_od *od = &test_cia301;
	canopen_od_handle_t handle;

	handle = canopen_od_find(od, 0x1004U, 0U);
	zassert_false(canopen_od_handle_is_valid(handle), "object %04xh subindex %d is present",
		      index, subindex);
}

static void validate_entry(uint16_t index, uint8_t subindex, uint16_t exp_type, uint8_t exp_bits,
			   uint8_t exp_attr)
{
	const struct canopen_od *od = &test_cia301;
	canopen_od_handle_t handle;
	uint16_t handle_index;
	uint8_t handle_subindex;
	uint16_t type;
	uint8_t bits;
	uint8_t attr;

	/* Validate object entry present */
	handle = canopen_od_find(od, index, subindex);
	zassert_true(canopen_od_handle_is_valid(handle), "object %04xh subindex %d is not present",
		     index, subindex);

	/* Validate index */
	zassert_ok(canopen_od_handle_get_index(od, handle, &handle_index));
	zassert_equal(index, handle_index, "handle points to wrong index");

	/* Validate subindex */
	zassert_ok(canopen_od_handle_get_subindex(od, handle, &handle_subindex));
	zassert_equal(subindex, handle_subindex, "handle points to wrong subindex");

	/* Validate data type */
	zassert_ok(canopen_od_get_type_by_handle(od, handle, &type),
		   "failed to get object %04xh subindex %d type", index, subindex);
	zassert_equal(type, exp_type, "object %04xh subindex %d has wrong type", index, subindex);

	/* Validate number of bits */
	zassert_ok(canopen_od_get_bits_by_handle(od, handle, &bits),
		   "failed to get object %04xh subindex %d bits", index, subindex);
	zassert_equal(bits, exp_bits, "object %04xh subindex %d has wrong number of bits", index,
		      subindex);

	/* Validate attributes */
	zassert_ok(canopen_od_get_attr_by_handle(od, handle, &attr),
		   "failed to get object %04xh subindex %d attr", index, subindex);
	zassert_equal(attr, exp_attr, "object %04xh subindex %d has wrong attributes", index,
		      subindex);

	/* TODO: validate size */

	/* TODO: validate minimum */
	/* TODO: validate maximum */
	/* TODO: validate default value */
}

static void validate_array(uint16_t index, uint8_t size, uint8_t size_attr, uint16_t array_type,
			   uint8_t array_bits, uint8_t array_attr)
{
	/* 0 - Highest sub-index supported */
	validate_entry(index, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, size_attr);

	/* 1 to N - Array subindex 1 to N */
	for (uint8_t subindex = 1U; subindex <= size; subindex++) {
		validate_entry(index, subindex, array_type, array_bits, array_attr);
	}
}

static void validate_pdo_communication(uint16_t index)
{
	/* 0 - Highest sub-index supported */
	validate_entry(index, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_CONST);

	/* 1 - COB-ID used by RPDO */
	validate_entry(index, 1U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U,
		       CANOPEN_OD_ATTR_ACCESS_RW | CANOPEN_OD_ATTR_RELATIVE);

	/* 2 - Transmission type */
	validate_entry(index, 2U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RW);

	/* 3 - Inhibit time */
	validate_entry(index, 3U, CANOPEN_OD_DEFTYPE_UNSIGNED16, 16U, CANOPEN_OD_ATTR_ACCESS_RW);

	/* 4 - Reserved (not present) */
	validate_entry_not_present(index, 4U);

	/* 5 - Event timer */
	validate_entry(index, 5U, CANOPEN_OD_DEFTYPE_UNSIGNED16, 16U, CANOPEN_OD_ATTR_ACCESS_RW);
}

static void validate_rpdo_communication(uint16_t index)
{
	validate_pdo_communication(index);

	/* 6 - SYNC start value (not present) */
	validate_entry_not_present(index, 6U);
}

static void validate_tpdo_communication(uint16_t index)
{
	validate_pdo_communication(index);

	/* 6 - SYNC start value */
	validate_entry(index, 6U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RW);
}

static void validate_pdo_mapping(uint16_t index, uint8_t size)
{
	/* 0 - Number of mapped application objects in PDO */
	validate_entry(index, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RW);

	/* 1 to N - 1st to Nth mapped object */
	for (uint8_t subindex = 1U; subindex <= size; subindex++) {
		validate_entry(index, subindex, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U,
			       CANOPEN_OD_ATTR_ACCESS_RW);
	}
}

ZTEST_USER(canopen_od_cia301, test_1000h)
{
	/* 1000h - Device type */
	validate_entry(0x1000U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1001h)
{
	/* 1001h - Error register */
	validate_entry(0x1001U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1002h)
{
	/* 1002h - Manufacturer status register */
	validate_entry(0x1002U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1003h)
{
	/* 1003h - Pre-defined error field */
	/* 0 - Number of errors */
	/* 1 to 8 - Standard error field 1 to 8 */
	validate_array(0x1003U, 8U, CANOPEN_OD_ATTR_ACCESS_RW, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U,
		       CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1005h)
{
	/* 1005h - COB-ID SYNC */
	validate_entry(0x1005U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1006h)
{
	/* 1006h - Communication cycle period */
	validate_entry(0x1006U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1007h)
{
	/* 1007h - Synchronous window length */
	validate_entry(0x1007U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1008h)
{
	/* 1008h - Manufacturer device name */
	validate_entry(0x1008U, 0U, CANOPEN_OD_DEFTYPE_VISIBLE_STRING, 8U,
		       CANOPEN_OD_ATTR_ACCESS_CONST);
}

ZTEST_USER(canopen_od_cia301, test_1009h)
{
	/* 1009h - Manufacturer hardware version */
	validate_entry(0x1009U, 0U, CANOPEN_OD_DEFTYPE_VISIBLE_STRING, 8U,
		       CANOPEN_OD_ATTR_ACCESS_CONST);
}

ZTEST_USER(canopen_od_cia301, test_100ah)
{
	/* 100ah - Manufacturer software version */
	validate_entry(0x100aU, 0U, CANOPEN_OD_DEFTYPE_VISIBLE_STRING, 8U,
		       CANOPEN_OD_ATTR_ACCESS_CONST);
}

ZTEST_USER(canopen_od_cia301, test_100ch)
{
	/* 100ch - Guard time */
	validate_entry(0x100cU, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED16, 16U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_100dh)
{
	/* 100dh - Life time factor */
	validate_entry(0x100dU, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1010h)
{
	/* 1010h - Store parameters */
	/* 0 - Highest sub-index supported */
	/* 1 - Save all parameters */
	/* 2 - Save communication parameters */
	/* 3 - Save application parameters */
	/* 4 - Save manufacturer defined parameters */
	validate_array(0x1010U, 4U, CANOPEN_OD_ATTR_ACCESS_CONST, CANOPEN_OD_DEFTYPE_UNSIGNED32,
		       32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1011h)
{
	/* 1011h - Restore default parameters */
	/* 0 - Highest sub-index supported */
	/* 1 - Restore all default parameters */
	/* 2 - Restore communication default parameters */
	/* 3 - Restore application default parameters */
	/* 4 - Restore manufacturer default parameters */
	validate_array(0x1011U, 4U, CANOPEN_OD_ATTR_ACCESS_CONST, CANOPEN_OD_DEFTYPE_UNSIGNED32,
		       32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1012h)
{
	/* 1012h - COB-ID time stamp */
	validate_entry(0x1012U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1013h)
{
	/* 1013h - High resolution time stamp */
	validate_entry(0x1013U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1014h)
{
	/* 1014h - COB-ID EMCY */
	validate_entry(0x1014U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U,
		       CANOPEN_OD_ATTR_ACCESS_RW | CANOPEN_OD_ATTR_RELATIVE);
}

ZTEST_USER(canopen_od_cia301, test_1015h)
{
	/* 1015h - Inhibit time EMCY */
	validate_entry(0x1015U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED16, 16U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1016h)
{
	/* 1016h - Consumer heartbeat time */
	/* 0 - Highest sub-index supported */
	/* 1 - Consumer heartbeat time 1 */
	validate_array(0x1016U, 1U, CANOPEN_OD_ATTR_ACCESS_CONST, CANOPEN_OD_DEFTYPE_UNSIGNED32,
		       32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1017h)
{
	/* 1017h - Producer heartbeat time */
	validate_entry(0x1017U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED16, 16U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1018h)
{
	/* 1018h - Identity object */
	/* 0 - Highest sub-index supported */
	validate_entry(0x1018U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_CONST);

	/* 1 - Vendor-ID */
	validate_entry(0x1018U, 1U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RO);

	/* 2 - Product code */
	validate_entry(0x1018U, 2U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RO);

	/* 3 - Revision number */
	validate_entry(0x1018U, 3U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RO);

	/* 4 - Serial number */
	validate_entry(0x1018U, 4U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1019h)
{
	/* 1019h - Synchronous counter overflow value */
	validate_entry(0x1019U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1020h)
{
	/* 1020h - Verify configuration */
	/* 0 - Highest sub-index supported */
	/* 1 - Configuration date */
	/* 2 - Configuration time */
	validate_array(0x1020U, 2U, CANOPEN_OD_ATTR_ACCESS_CONST, CANOPEN_OD_DEFTYPE_UNSIGNED32,
		       32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1021h)
{
	/* 1021h - Store EDS */
	validate_entry(0x1021U, 0U, CANOPEN_OD_DEFTYPE_DOMAIN, 0U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1022h)
{
	/* 1022h - Store format */
	validate_entry(0x1022U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1023h)
{
	/* 1023h - OS command */
	/* 0 - Highest sub-index supported */
	validate_entry(0x1023U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_CONST);

	/* 1 - Command */
	validate_entry(0x1023U, 1U, CANOPEN_OD_DEFTYPE_OCTET_STRING, 8U, CANOPEN_OD_ATTR_ACCESS_RW);

	/* 2 - Status */
	validate_entry(0x1023U, 2U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RO);

	/* 3 - Reply */
	validate_entry(0x1023U, 3U, CANOPEN_OD_DEFTYPE_OCTET_STRING, 8U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1024h)
{
	/* 1024h - OS command mode */
	validate_entry(0x1024U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_WO);
}

ZTEST_USER(canopen_od_cia301, test_1025h)
{
	/* 1025h - OS debugger interface */
	/* 0 - Highest sub-index supported */
	validate_entry(0x1025U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_CONST);

	/* 1 - Command */
	validate_entry(0x1025U, 1U, CANOPEN_OD_DEFTYPE_OCTET_STRING, 8U, CANOPEN_OD_ATTR_ACCESS_RW);

	/* 2 - Status */
	validate_entry(0x1025U, 2U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RO);

	/* 3 - Reply */
	validate_entry(0x1025U, 3U, CANOPEN_OD_DEFTYPE_OCTET_STRING, 8U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1026h)
{
	/* 1026h - OS prompt */
	/* 0 - Highest sub-index supported */
	validate_entry(0x1026U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_CONST);

	/* 1 - StdIn */
	validate_entry(0x1026U, 1U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_WO);

	/* 2 - StdOut */
	validate_entry(0x1026U, 2U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RO);

	/* 3 - StdErr */
	validate_entry(0x1026U, 3U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RO);
}

ZTEST_USER(canopen_od_cia301, test_1028h)
{
	/* 1028h - Emergency consumer */
	/* 0 - Highest sub-index supported */
	/* 1 - Emergency consumer 1 */
	validate_array(0x1028U, 1U, CANOPEN_OD_ATTR_ACCESS_CONST, CANOPEN_OD_DEFTYPE_UNSIGNED32,
		       32U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1029h)
{
	/* 1029h - Error behavior */
	/* 0 - Highest sub-index supported */
	/* 1 - Communication error */
	validate_array(0x1029U, 1U, CANOPEN_OD_ATTR_ACCESS_CONST, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U,
		       CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1200h)
{
	/* 1200h - SDO server parameter */
	/* 0 - Highest sub-index supported */
	validate_entry(0x1200U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_CONST);

	/* 1 - COB-ID client to server */
	validate_entry(0x1200U, 1U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U,
		       CANOPEN_OD_ATTR_ACCESS_CONST | CANOPEN_OD_ATTR_RELATIVE);

	/* 2 - COB-ID server to client */
	validate_entry(0x1200U, 2U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U,
		       CANOPEN_OD_ATTR_ACCESS_RO | CANOPEN_OD_ATTR_RELATIVE);

	/* 3 - Node-ID of SDO client */
	validate_entry_not_present(0x1200U, 3U);
}

ZTEST_USER(canopen_od_cia301, test_1280h)
{
	/* 1280h - SDO client parameter */
	/* 0 - Highest sub-index supported */
	validate_entry(0x1280U, 0U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_CONST);

	/* 1 - COB-ID client to server */
	validate_entry(0x1280U, 1U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RW);

	/* 2 - COB-ID server to client */
	validate_entry(0x1280U, 2U, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, CANOPEN_OD_ATTR_ACCESS_RW);

	/* 3 - Node-ID of the SDO server */
	validate_entry(0x1280U, 3U, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, CANOPEN_OD_ATTR_ACCESS_RW);
}

ZTEST_USER(canopen_od_cia301, test_1400h)
{
	/* 1400h - RPDO communication parameter 1 */
	/* 0 - Highest sub-index supported */
	/* 1 - COB-ID used by RPDO */
	/* 2 - Transmission type */
	/* 3 - Inhibit time */
	/* 5 - Event timer */
	validate_rpdo_communication(0x1400U);
}

ZTEST_USER(canopen_od_cia301, test_1401h)
{
	/* 1401h - RPDO communication parameter 2 */
	/* 0 - Highest sub-index supported */
	/* 1 - COB-ID used by RPDO */
	/* 2 - Transmission type */
	/* 3 - Inhibit time */
	/* 5 - Event timer */
	validate_rpdo_communication(0x1401U);
}

ZTEST_USER(canopen_od_cia301, test_1402h)
{
	/* 1402h - RPDO communication parameter 3 */
	/* 0 - Highest sub-index supported */
	/* 1 - COB-ID used by RPDO */
	/* 2 - Transmission type */
	/* 3 - Inhibit time */
	/* 5 - Event timer */
	validate_rpdo_communication(0x1402U);
}

ZTEST_USER(canopen_od_cia301, test_1403h)
{
	/* 1403h - RPDO communication parameter 4 */
	/* 0 - Highest sub-index supported */
	/* 1 - COB-ID used by RPDO */
	/* 2 - Transmission type */
	/* 3 - Inhibit time */
	/* 5 - Event timer */
	validate_rpdo_communication(0x1403U);
}

ZTEST_USER(canopen_od_cia301, test_1600h)
{
	/* 1600h - RPDO mapping parameter 1 */
	/* 0 - Number of mapped application objects in PDO */
	/* 1 to 8 - 1st to 8th mapped object */
	validate_pdo_mapping(0x1600U, 8U);
}

ZTEST_USER(canopen_od_cia301, test_1601h)
{
	/* 1601h - RPDO mapping parameter 2 */
	/* 0 - Number of mapped application objects in PDO */
	/* 1 to 8 - 1st to 8th mapped object */
	validate_pdo_mapping(0x1601U, 8U);
}

ZTEST_USER(canopen_od_cia301, test_1602h)
{
	/* 1602h - RPDO mapping parameter 3 */
	/* 0 - Number of mapped application objects in PDO */
	/* 1 to 8 - 1st to 8th mapped object */
	validate_pdo_mapping(0x1602U, 8U);
}

ZTEST_USER(canopen_od_cia301, test_1603h)
{
	/* 1603h - RPDO mapping parameter 1 */
	/* 0 - Number of mapped application objects in PDO */
	/* 1 to 8 - 1st to 8th mapped object */
	validate_pdo_mapping(0x1603U, 8U);
}

ZTEST_USER(canopen_od_cia301, test_1800h)
{
	/* 1800h - TPDO communication parameter 1 */
	/* 0 - Highest sub-index supported */
	/* 1 - COB-ID used by TPDO */
	/* 2 - Transmission type */
	/* 3 - Inhibit time */
	/* 5 - Event timer */
	/* 6 - SYNC start value */
	validate_tpdo_communication(0x1800U);
}

ZTEST_USER(canopen_od_cia301, test_1801h)
{
	/* 1801h - TPDO communication parameter 2 */
	/* 0 - Highest sub-index supported */
	/* 1 - COB-ID used by TPDO */
	/* 2 - Transmission type */
	/* 3 - Inhibit time */
	/* 5 - Event timer */
	/* 6 - SYNC start value */
	validate_tpdo_communication(0x1801U);
}

ZTEST_USER(canopen_od_cia301, test_1802h)
{
	/* 1802h - TPDO communication parameter 3 */
	/* 0 - Highest sub-index supported */
	/* 1 - COB-ID used by TPDO */
	/* 2 - Transmission type */
	/* 3 - Inhibit time */
	/* 5 - Event timer */
	/* 6 - SYNC start value */
	validate_tpdo_communication(0x1802U);
}

ZTEST_USER(canopen_od_cia301, test_1803h)
{
	/* 1803h - TPDO communication parameter 4 */
	/* 0 - Highest sub-index supported */
	/* 1 - COB-ID used by TPDO */
	/* 2 - Transmission type */
	/* 3 - Inhibit time */
	/* 5 - Event timer */
	/* 6 - SYNC start value */
	validate_tpdo_communication(0x1803U);
}

ZTEST_USER(canopen_od_cia301, test_1a00h)
{
	/* 1a00h - TPDO mapping parameter 1 */
	/* 0 - Number of mapped application objects in TPDO */
	/* 1 to 8 - 1st to 8th mapped object */
	validate_pdo_mapping(0x1a00U, 8U);
}

ZTEST_USER(canopen_od_cia301, test_1a01h)
{
	/* 1a01h - TPDO mapping parameter 2 */
	/* 0 - Number of mapped application objects in TPDO */
	/* 1 to 8 - 1st to 8th mapped object */
	validate_pdo_mapping(0x1a01U, 8U);
}

ZTEST_USER(canopen_od_cia301, test_1a02h)
{
	/* 1a02h - TPDO mapping parameter 3 */
	/* 0 - Number of mapped application objects in TPDO */
	/* 1 to 8 - 1st to 8th mapped object */
	validate_pdo_mapping(0x1a02U, 8U);
}

ZTEST_USER(canopen_od_cia301, test_1a03h)
{
	/* 1a03h - TPDO mapping parameter 4 */
	/* 0 - Number of mapped application objects in TPDO */
	/* 1 to 8 - 1st to 8th mapped object */
	validate_pdo_mapping(0x1a03U, 8U);
}

ZTEST_USER(canopen_od_cia301, test_not_present)
{
	/* 1004h - Not present in standard */
	validate_entry_not_present(0x1004U, 0U);

	/* 1027h - Modules (redundant) */
	validate_entry_not_present(0x1027U, 0U);
}

void *canopen_od_cia301_setup(void)
{
#ifdef CONFIG_USERSPACE
	zassert_ok(k_mem_domain_add_partition(&k_mem_domain_default, &test_cia301_partition));
#endif /* CONFIG_USERSPACE */

	return NULL;
}

ZTEST_SUITE(canopen_od_cia301, NULL, canopen_od_cia301_setup, NULL, NULL, NULL);
