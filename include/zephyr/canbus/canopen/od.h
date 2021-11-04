/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_CANOPEN_OD_H_
#define ZEPHYR_INCLUDE_CANBUS_CANOPEN_OD_H_

/**
 * @file canbus/canopen/od.h
 * @brief Object Dictionary
 * @defgroup canopen_od Object Dictionary
 * @ingroup canopen
 * @{
 */

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/sys/mutex.h>
#include <zephyr/toolchain.h>
#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct canopen_od;
struct canopen_od_object;
struct canopen_od_entry;

/**
 * @name CANopen object dictionary object type codes (CiA 301, table 42).
 * @{
 */
/** Object with no data fields. */
#define CANOPEN_OD_OBJCODE_NULL      0x00U
/** Large variable amount of data. */
#define CANOPEN_OD_OBJCODE_DOMAIN    0x02U
/** Type definition. */
#define CANOPEN_OD_OBJCODE_DEFTYPE   0x05U
/** Record type definition. */
#define CANOPEN_OD_OBJCODE_DEFSTRUCT 0x06U
/** Single value. */
#define CANOPEN_OD_OBJCODE_VAR       0x07U
/** Multiple data field, each field of the same data type. */
#define CANOPEN_OD_OBJCODE_ARRAY     0x08U
/** Multiple data field, any combination of data types. */
#define CANOPEN_OD_OBJCODE_RECORD    0x09U
/** @} */

/**
 * @name CANopen object dictionary data types (CiA 301, table 44).
 * @{
 */
/** Boolean type. */
#define CANOPEN_OD_DEFTYPE_BOOLEAN                       0x0001U
/** 8-bit integer type. */
#define CANOPEN_OD_DEFTYPE_INTEGER8                      0x0002U
/** 16-bit integet type. */
#define CANOPEN_OD_DEFTYPE_INTEGER16                     0x0003U
/** 32-bit integer type. */
#define CANOPEN_OD_DEFTYPE_INTEGER32                     0x0004U
/** 8-bit unsigned integer type. */
#define CANOPEN_OD_DEFTYPE_UNSIGNED8                     0x0005U
/** 16-bit unsigned integer type. */
#define CANOPEN_OD_DEFTYPE_UNSIGNED16                    0x0006U
/** 32-bit unsigned integer type. */
#define CANOPEN_OD_DEFTYPE_UNSIGNED32                    0x0007U
/** 32-bit floating point type. */
#define CANOPEN_OD_DEFTYPE_REAL32                        0x0008U
/** Visible string type. */
#define CANOPEN_OD_DEFTYPE_VISIBLE_STRING                0x0009U
/** Octet string type. */
#define CANOPEN_OD_DEFTYPE_OCTET_STRING                  0x000aU
/** Unicode string type. */
#define CANOPEN_OD_DEFTYPE_UNICODE_STRING                0x000bU
/** Time-of-day type. */
#define CANOPEN_OD_DEFTYPE_TIME_OF_DAY                   0x000cU
/** Time difference type. */
#define CANOPEN_OD_DEFTYPE_TIME_DIFFERENCE               0x000dU
/** Domain type. */
#define CANOPEN_OD_DEFTYPE_DOMAIN                        0x000fU
/** 24-bit integer type. */
#define CANOPEN_OD_DEFTYPE_INTEGER24                     0x0010U
/** 64-bit floating point type. */
#define CANOPEN_OD_DEFTYPE_REAL64                        0x0011U
/** 40-bit integer type. */
#define CANOPEN_OD_DEFTYPE_INTEGER40                     0x0012U
/** 48-bit integer type. */
#define CANOPEN_OD_DEFTYPE_INTEGER48                     0x0013U
/** 56-bit integer type. */
#define CANOPEN_OD_DEFTYPE_INTEGER56                     0x0014U
/** 64-bit integer type. */
#define CANOPEN_OD_DEFTYPE_INTEGER64                     0x0015U
/** 24-bit unsigned integer type. */
#define CANOPEN_OD_DEFTYPE_UNSIGNED24                    0x0016U
/** 40-bit unsigned integer type. */
#define CANOPEN_OD_DEFTYPE_UNSIGNED40                    0x0018U
/** 48-bit unsigned integer type. */
#define CANOPEN_OD_DEFTYPE_UNSIGNED48                    0x0019U
/** 56-bit unsigned integer type. */
#define CANOPEN_OD_DEFTYPE_UNSIGNED56                    0x001aU
/** 64-bit unsigned integer type. */
#define CANOPEN_OD_DEFTYPE_UNSIGNED64                    0x001bU
/** PDO communication parameter type. */
#define CANOPEN_OD_DEFSTRUCT_PDO_COMMUNICATION_PARAMETER 0x0020U
/** PDO mapping type. */
#define CANOPEN_OD_DEFSTRUCT_PDO_MAPPING                 0x0021U
/** SDO parameter type. */
#define CANOPEN_OD_DEFSTRUCT_SDO_PARAMETER               0x0022U
/** Identity type. */
#define CANOPEN_OD_DEFSTRUCT_IDENTITY                    0x0023U
/** OS debug record. */
#define CANOPEN_OD_DEFSTRUCT_OS_DEBUG_RECORD             0x0024U
/** OS command record. */
#define CANOPEN_OD_DEFSTRUCT_OS_COMMAND_RECORD           0x0025U
/** @} */

/**
 * @name CANopen object dictionary object entry access attributes (CiA 301, table 43).
 * @{
 */
/** @cond INTERNAL_HIDDEN */
#define CANOPEN_OD_ATTR_ACCESS_MASK  GENMASK(1, 0)
/** @endcond */
/** Read/write access */
#define CANOPEN_OD_ATTR_ACCESS_RW    0x0U
/** Write-only access */
#define CANOPEN_OD_ATTR_ACCESS_WO    0x1U
/** Read-only access */
#define CANOPEN_OD_ATTR_ACCESS_RO    0x2U
/** Const access */
#define CANOPEN_OD_ATTR_ACCESS_CONST 0x3U
/** @} */

/**
 * @name CANopen object dictionary object entry PDO mapping attributes (CiA 301, table 53)
 * @{
 */
/** @cond INTERNAL_HIDDEN */
#define CANOPEN_OD_ATTR_PDO_MAPPABLE_MASK GENMASK(3, 2)
/** @endcond */
/** RPDO mappable */
#define CANOPEN_OD_ATTR_PDO_MAPPABLE_RPDO BIT(2)
/** TPDO mappable */
#define CANOPEN_OD_ATTR_PDO_MAPPABLE_TPDO BIT(3)
/** RPDO and TPDO mappable */
#define CANOPEN_OD_ATTR_PDO_MAPPABLE                                                               \
	(CANOPEN_OD_ATTR_PDO_MAPPABLE_RPDO | CANOPEN_OD_ATTR_PDO_MAPPABLE_TPDO)
/** @} */

/**
 * @name CANopen object dictionary object entry miscellaneous attributes
 * @{
 */
/** COB-ID is relative to node-ID */
#define CANOPEN_OD_ATTR_RELATIVE BIT(4)
/** @} */

/**
 * @typedef canopen_od_handle_t
 * @brief Opaque handle for accessing a CANopen object dictionary object entry.
 */
typedef uint32_t canopen_od_handle_t;

/**
 * @typedef canopen_od_callback_handler_t
 * @brief Callback signature for accessing CANopen object dictionary entries.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param obj Pointer to the object
 * @param entry Pointer to the entry
 * @param reading True if reading, false if setting/writing
 * @param value Pointer to the value being set/written/read
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result
 * @param user_data User data provided when registering the callback
 *
 * @retval Zero on success or negative error code
 */
typedef int (*canopen_od_callback_handler_t)(const struct canopen_od *od,
					     const struct canopen_od_object *obj,
					     const struct canopen_od_entry *entry, bool reading,
					     void *value, size_t size, uint32_t *abort_code,
					     void *user_data);

/**
 * @brief CANopen object dictionary object entry
 */
struct canopen_od_entry {
	/** 8-bit sub-index of this CANopen object dictionary object entry. */
	const uint8_t subindex;
	/** Data type information for this entry. */
	const uint16_t type;
	/** Bit size information for this entry (0 to 64). */
	const uint8_t bits;
	/** Attributes for this entry. */
	const uint8_t attr;
	/** Pointer to the data storage for this entry. */
	const void *data;
	/** Pointer to the minimum allowed data value for this entry or NULL. */
	const void *min;
	/** Pointer to the maximum allowed data value for this entry or NULL. */
	const void *max;
	/** Size of this entry (number of bytes used by each of *data, *min, and *max) */
	const size_t size;
};

/**
 * @brief CANopen object dictionary object
 */
struct canopen_od_object {
	/** 16-bit index of this CANopen object dictionary object. */
	const uint16_t index;
	/** Optional callback function for accessing this object. */
	canopen_od_callback_handler_t callback;
	/** Optional callback user data */
	void *user_data;
	/**
	 * Pointer to array of CANopen object dictionary entries for this object, ordered by
	 * ascending sub-index.
	 */
	const struct canopen_od_entry *entries;
	/** Number of entries. */
	const uint8_t num_entries;
};

/**
 * @brief CANopen object dictionary
 */
struct canopen_od {
	/** Pointer to object dictionary lock. */
	struct sys_mutex *lock;
	/** Pointer to array of CANopen object dictionary objects, ordered by ascending index. */
	const struct canopen_od_object *objects;
	/** Number of objects. */
	const uint16_t num_objects;
};

/**
 * @brief CANopen object dictionary information
 */
struct canopen_od_info {
	/** Pointer to the Object dictionary. */
	const struct canopen_od *od;
	/** Object dictionary name. */
	const char *name;
};

/**
 * @name CANopen object dictionary object entry initialization macros
 * @{
 */

/**
 * @brief CANopen object dictionary object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _type Data type
 * @param _bits Bit size of the data (0 to 64)
 * @param _data Pointer to the data
 * @param _min Pointer to the minimum value
 * @param _max Pointer to the maximum value
 * @param _size Size of this entry in bytes
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY(_subindex, _type, _bits, _data, _min, _max, _size, _attr)                 \
	{                                                                                          \
		.subindex = _subindex,                                                             \
		.type = _type,                                                                     \
		.bits = _bits,                                                                     \
		.data = _data,                                                                     \
		.min = _min,                                                                       \
		.max = _max,                                                                       \
		.size = _size,                                                                     \
		.attr = _attr,                                                                     \
	}

/**
 * @brief CANopen object dictionary BOOLEAN object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the bool data
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_BOOLEAN(_subindex, _data, _attr)                                          \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_BOOLEAN, 1U, _data, NULL, NULL,             \
			 sizeof(bool), _attr)

/**
 * @brief CANopen object dictionary UNSIGNED8 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint8_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED8(_subindex, _data, _min, _max, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_UNSIGNED8, 8U, _data, _min, _max,           \
			 sizeof(uint8_t), _attr)

/**
 * @brief CANopen object dictionary UNSIGNED16 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint16_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED16(_subindex, _data, _min, _max, _attr)                           \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_UNSIGNED16, 16U, _data, _min, _max,         \
			 sizeof(uint16_t), _attr)

/**
 * @brief CANopen object dictionary UNSIGNED24 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint32_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED24(_subindex, _data, _min, _max, _attr)                           \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_UNSIGNED24, 24U, _data, _min, _max,         \
			 sizeof(uint32_t), _attr)

/**
 * @brief CANopen object dictionary UNSIGNED32 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint32_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED32(_subindex, _data, _min, _max, _attr)                           \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_UNSIGNED32, 32U, _data, _min, _max,         \
			 sizeof(uint32_t), _attr)

/**
 * @brief CANopen object dictionary UNSIGNED40 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED40(_subindex, _data, _min, _max, _attr)                           \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_UNSIGNED40, 40U, _data, _min, _max,         \
			 sizeof(uint64_t), _attr)

/**
 * @brief CANopen object dictionary UNSIGNED48 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED48(_subindex, _data, _min, _max, _attr)                           \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_UNSIGNED48, 48U, _data, _min, _max,         \
			 sizeof(uint64_t), _attr)

/**
 * @brief CANopen object dictionary UNSIGNED56 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED56(_subindex, _data, _min, _max, _attr)                           \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_UNSIGNED56, 56U, _data, _min, _max,         \
			 sizeof(uint64_t), _attr)

/**
 * @brief CANopen object dictionary UNSIGNED64 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED64(_subindex, _data, _min, _max, _attr)                           \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_UNSIGNED64, 64U, _data, _min, _max,         \
			 sizeof(uint64_t), _attr)

/**
 * @brief CANopen object dictionary INTEGER8 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int8_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER8(_subindex, _data, _min, _max, _attr)                             \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_INTEGER8, 8U, _data, _min, _max,            \
			 sizeof(int8_t), _attr)

/**
 * @brief CANopen object dictionary INTEGER16 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int16_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER16(_subindex, _data, _min, _max, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_INTEGER16, 16U, _data, _min, _max,          \
			 sizeof(int16_t), _attr)

/**
 * @brief CANopen object dictionary INTEGER824 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int32_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER24(_subindex, _data, _min, _max, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_INTEGER24, 24U, _data, _min, _max,          \
			 sizeof(int32_t), _attr)

/**
 * @brief CANopen object dictionary INTEGER32 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int32_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER32(_subindex, _data, _min, _max, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_INTEGER32, 32U, _data, _min, _max,          \
			 sizeof(int32_t), _attr)

/**
 * @brief CANopen object dictionary INTEGER40 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER40(_subindex, _data, _min, _max, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_INTEGER40, 40U, _data, _min, _max,          \
			 sizeof(int64_t), _attr)

/**
 * @brief CANopen object dictionary INTEGER48 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER48(_subindex, _data, _min, _max, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_INTEGER48, 48U, _data, _min, _max,          \
			 sizeof(int64_t), _attr)

/**
 * @brief CANopen object dictionary INTEGER56 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER56(_subindex, _data, _min, _max, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_INTEGER56, 56U, _data, _min, _max,          \
			 sizeof(int64_t), _attr)

/**
 * @brief CANopen object dictionary INTEGER64 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER64(_subindex, _data, _min, _max, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_INTEGER64, 64U, _data, _min, _max,          \
			 sizeof(int64_t), _attr)

/**
 * @brief CANopen object dictionary REAL32 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the float data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_REAL32(_subindex, _data, _min, _max, _attr)                               \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_REAL32, 32U, _data, _min, _max,             \
			 sizeof(float), _attr)

/**
 * @brief CANopen object dictionary REAL64 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the double data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_REAL64(_subindex, _data, _min, _max, _attr)                               \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_REAL64, 64U, _data, _min, _max,             \
			 sizeof(double), _attr)

/**
 * @brief CANopen object dictionary TIME_OF_DAY object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_TIME_OF_DAY(_subindex, _data, _attr)                                      \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_TIME_OF_DAY, 48U, _data, NULL, NULL,        \
			 sizeof(uint64_t), _attr)

/**
 * @brief CANopen object dictionary TIME_DIFFERENCE object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_TIME_DIFFERENCE(_subindex, _data, _attr)                                  \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_TIME_DIFFERENCE, 48U, _data, NULL, NULL,    \
			 sizeof(uint64_t), _attr)

/**
 * @brief CANopen object dictionary VISIBLE_STRING object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint8_t data
 * @param _size Size of the data in bytes
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_VISIBLE_STRING(_subindex, _data, _size, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_VISIBLE_STRING, 8U, _data, NULL, NULL,      \
			 _size, _attr)

/**
 * @brief CANopen object dictionary OCTET_STRING object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint8_t data
 * @param _size Size of the data in bytes
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_OCTET_STRING(_subindex, _data, _size, _attr)                              \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_OCTET_STRING, 8U, _data, NULL, NULL, _size, \
			 _attr)

/**
 * @brief CANopen object dictionary UNICODE_STRING object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint16_t data
 * @param _size Size of the data in bytes
 * @param _attr Attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNICODE_STRING(_subindex, _data, _size, _attr)                            \
	CANOPEN_OD_ENTRY(_subindex, CANOPEN_OD_DEFTYPE_UNICODE_STRING, 16U, _data, NULL, NULL,     \
			 _size, _attr)
/** @} */

/**
 * @name CANopen object dictionary object initialization macros
 * @{
 */

/**
 * @brief CANopen object initialization macro
 * @see canopen_od_object
 *
 * @param _index 16-bit index of this object
 * @param _num_entries Number of entries in the object
 * @param _entries Pointer to the array of entries
 */
#define CANOPEN_OD_OBJECT(_index, _num_entries, _entries)                                          \
	{                                                                                          \
		.index = _index,                                                                   \
		.callback = NULL,                                                                  \
		.user_data = NULL,                                                                 \
		.entries = _entries,                                                               \
		.num_entries = _num_entries,                                                       \
	}

/**
 * @brief CANopen object initialization macro with nested entries
 * @see CANOPEN_OD_OBJECT
 * @see canopen_od_object
 *
 * The nested entries will be declared as a const compound literal array.
 *
 * @param _index 16-bit index of this object
 * @param _entries Pointer to the array of entries
 */
#define CANOPEN_OD_OBJECT_ENTRIES(_index, _entries...)                                             \
	CANOPEN_OD_OBJECT(_index, ARRAY_SIZE(((const struct canopen_od_entry[]){_entries})),       \
			  ((const struct canopen_od_entry[]){_entries}))

/** @} */

/**
 * @name CANopen object dictionary initialization macros
 * @{
 */

/**
 * @brief CANopen object dictionary declaration macro
 * @see canopen_od
 *
 * @param _name Name of the CANopen object dictionary
 */
#define CANOPEN_OD_DECLARE(_name) extern const struct canopen_od _name

#if defined(CONFIG_CANOPEN_OD_INFO) || defined(__DOXYGEN__)
/**
 * @brief CANopen object dictionary information initialization macro
 * @see canopen_od_info
 *
 * @param _name Name of the CANopen object dictionary info
 * @param _od Name of the CANopen object dictionary
 */
#define CANOPEN_OD_INFO(_name, _od)                                                                \
	const STRUCT_SECTION_ITERABLE(canopen_od_info, _name) = {                                  \
		.od = &_od,                                                                        \
		.name = STRINGIFY(_od),                                                            \
	}
#else /* CONFIG_CANOPEN_OD_INFO */
#define CANOPEN_OD_INFO(_name, _od)
#endif /* !CONFIG_CANOPEN_OD_INFO */

/**
 * @brief CANopen object dictionary initialization macro
 * @see canopen_od
 *
 * @param _name Name of the CANopen object dictionary
 * @param _num_objects Number of objects in the array
 * @param _objects Pointer to the array of objects
 */
#define CANOPEN_OD_DEFINE(_name, _num_objects, _objects)                                           \
	static SYS_MUTEX_DEFINE(_name##_lock);                                                     \
	const struct canopen_od _name = {                                                          \
		.lock = &_name##_lock,                                                             \
		.objects = _objects,                                                               \
		.num_objects = _num_objects,                                                       \
	};                                                                                         \
	CANOPEN_OD_INFO(_name##_info, _name);

/**
 * @brief CANopen object dictionary initialization macro with nested objects
 * @see CANOPEN_OD_DEFINE
 * @see canopen_od
 *
 * The nested objects will be declared as a compound literal array.
 *
 * @param _name Name of the CANopen object dictionary
 * @param _objects Nested object initializers
 */
#define CANOPEN_OD_DEFINE_OBJECTS(_name, _objects...)                                              \
	CANOPEN_OD_DEFINE(_name, ARRAY_SIZE(((struct canopen_od_object[]){_objects})),             \
			  ((struct canopen_od_object[]){_objects}))

/** @} */

/**
 * @name CANopen object dictionary utility functions
 * @{
 */

/**
 * @brief Lock the CANopen object dictionary.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param timeout Waiting period to obtain the lock, or one of the special
 *                values K_NO_WAIT and K_FOREVER.
 *
 * @retval 0 CANopen object dictionary locked
 * @retval -EBUSY Returned without waiting
 * @retval -EAGAIN Waiting period timed out
 */
static inline int canopen_od_lock(const struct canopen_od *od, k_timeout_t timeout)
{
	__ASSERT_NO_MSG(od != NULL);

	return sys_mutex_lock(od->lock, timeout);
}

/**
 * @brief Unlock the CANopen object dictionary.
 *
 * @param od Pointer to the CANopen object dictionary
 *
 * @retval 0 CANopen object dictionary unlocked
 * @retval -EPERM The current thread does not own the lock
 * @retval -EINVAL The CANopen object dictionary is not locked
 */
static inline int canopen_od_unlock(const struct canopen_od *od)
{
	__ASSERT_NO_MSG(od != NULL);

	return sys_mutex_unlock(od->lock);
}

/**
 * TODO
 */
typedef int (*canopen_od_foreach_entry_callback_t)(const struct canopen_od *od,
						   canopen_od_handle_t handle, void *user_data);

/**
 * TODO
 */
int canopen_od_foreach_entry(const struct canopen_od *od, canopen_od_foreach_entry_callback_t cb,
			     void *user_data);

/**
 * TODO
 */
int canopen_od_handle_get_index(const struct canopen_od *od, canopen_od_handle_t handle,
				uint16_t *index);

/**
 * TODO
 */
int canopen_od_handle_get_subindex(const struct canopen_od *od, canopen_od_handle_t handle,
				   uint8_t *subindex);

/**
 * @brief Find an object entry in the CANopen object dictionary.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param index 16-bit index of the object to find
 * @param subindex 8-bit sub-index of the object entry to find
 *
 * @retval TODO
 */
canopen_od_handle_t canopen_od_find(const struct canopen_od *od, uint16_t index, uint8_t subindex);

/**
 * @brief Find an object entry in the CANopen object dictionary by handle.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param handle Handle for the CANopen object dictionary object
 * @param subindex 8-bit sub-index of the new object entry to find
 *
 * @retval TODO
 */
canopen_od_handle_t canopen_od_find_by_handle(const struct canopen_od *od,
					      canopen_od_handle_t handle, uint8_t subindex);

/**
 * @brief Set CANopen object dictionary callback
 *
 * Set the callback function to be called when reading/writing/setting values in
 * the CANopen object dictionary for a given object.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param index Index of the CANopen object dictionary object
 * @param cb Callback function
 * @param user_data User data to be passed to the callback
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
int canopen_od_set_callback(const struct canopen_od *od, uint16_t index,
			    canopen_od_callback_handler_t cb, void *user_data);

/**
 * TODO
 */
bool canopen_od_handle_is_valid(canopen_od_handle_t handle);

/**
 * TODO
 */
int canopen_od_get_type_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
				  uint16_t *type);

/**
 * TODO
 */
static inline int canopen_od_get_type(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				      uint16_t *type)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_get_type_by_handle(od, handle, type);
}

/**
 * TODO
 */
int canopen_od_get_bits_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
				  uint8_t *bits);

/**
 * TODO
 */
static inline int canopen_od_get_bits(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				      uint8_t *bits)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_get_bits_by_handle(od, handle, bits);
}

/**
 * TODO
 */
int canopen_od_get_size_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
				  size_t *size);

/**
 * TODO
 */
static inline int canopen_od_get_size(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				      size_t *size)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_get_size_by_handle(od, handle, size);
}

/**
 * TODO
 */
int canopen_od_get_attr_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
				  uint8_t *attr);

/**
 * TODO
 */
static inline int canopen_od_get_attr(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				      uint8_t *attr)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_get_attr_by_handle(od, handle, attr);
}

/**
 * TODO
 */
static inline int canopen_od_get_min(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				     void *min, size_t size)
{
	/* TODO */
	return 0;
}

/**
 * TODO
 */
static inline int canopen_od_get_max(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				     void *max, size_t size)
{
	/* TODO */
	return 0;
}

/** @} */

/**
 * @name CANopen object dictionary untyped accessors
 * @{
 */

/**
 * @brief Set a CANopen object dictionary object entry by handle without obtaining the lock
 *
 * Set the value of a CANopen object dictionary object entry bypassing any
 * read-only access.
 *
 * @note The caller is responsible to locking the object dictionary using @a
 * canopen_od_lock() before calling this function and unlocking it using @a
 * canopen_od_unlock() afterwards.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param handle Handle for the CANopen object dictionary object entry
 * @param value Pointer to the value to be set.
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
int canopen_od_set_by_handle_unlocked(const struct canopen_od *od, canopen_od_handle_t handle,
				      void *value, size_t size, uint32_t *abort_code);

/**
 * @brief Set a CANopen object dictionary object entry value by handle
 *
 * Lock the CANopen object dictionary, set the value of a CANopen object
 * dictionary object entry bypassing any read-only access, and unlock the
 * CANopen object dictionary again.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param handle Handle for the CANopen object dictionary object entry
 * @param value Pointer to the value to be set.
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
static inline int canopen_od_set_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
					   void *value, size_t size, uint32_t *abort_code)
{
	int err;

	canopen_od_lock(od, K_FOREVER);
	err = canopen_od_set_by_handle_unlocked(od, handle, value, size, abort_code);
	canopen_od_unlock(od);

	return err;
}

/**
 * @brief Set a CANopen object dictionary object entry by index and sub-index
 *
 * Lock the CANopen object dictionary, set the value of a CANopen object
 * dictionary object entry bypassing any read-only access, and unlock the
 * CANopen object dictionary again.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param index Index of the CANopen object dictionary object
 * @param subindex Sub-index of the CANopen object dictionary object entry. For
 *                 single object dictionary objects the sub-index is always 0.
 * @param value Pointer to the value to be set.
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
static inline int canopen_od_set(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				 void *value, size_t size, uint32_t *abort_code)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_set_by_handle(od, handle, value, size, abort_code);
}

/**
 * @brief Write to a CANopen object dictionary object entry by handle without obtaining the lock
 *
 * Write the value to the CANopen object dictionary object entry.
 *
 * @note The caller is responsible to locking the object dictionary using @a
 * canopen_od_lock() before calling this function and unlocking it using @a
 * canopen_od_unlock() afterwards.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param handle Handle for the CANopen object dictionary object entry
 * @param value Pointer to the value to be written.
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
int canopen_od_write_by_handle_unlocked(const struct canopen_od *od, canopen_od_handle_t handle,
					void *value, size_t size, uint32_t *abort_code);

/**
 * @brief Write to a CANopen object dictionary object entry by handle
 *
 * Lock the CANopen object dictionary, write the value to the CANopen object
 * dictionary object entry, and unlock the CANopen object dictionary again.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param handle Handle for the CANopen object dictionary object entry
 * @param value Pointer to the value to be written.
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
static inline int canopen_od_write_by_handle(const struct canopen_od *od,
					     canopen_od_handle_t handle, void *value, size_t size,
					     uint32_t *abort_code)
{
	int err;

	canopen_od_lock(od, K_FOREVER);
	err = canopen_od_write_by_handle_unlocked(od, handle, value, size, abort_code);
	canopen_od_unlock(od);

	return err;
}

/**
 * @brief Write to a CANopen object dictionary object entry by index and sub-index
 *
 * Lock the CANopen object dictionary, write the value to the CANopen object
 * dictionary object entry, and unlock the CANopen object dictionary again.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param index Index of the CANopen object dictionary object
 * @param subindex Sub-index of the CANopen object dictionary object entry. For
 *                 non-array/non-record dictionary objects the sub-index is always 0.
 * @param value Pointer to the value to be written.
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
static inline int canopen_od_write(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				   void *value, size_t size, uint32_t *abort_code)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_write_by_handle(od, handle, value, size, abort_code);
}

/**
 * @brief Read from a CANopen object dictionary object entry by handle without obtaining the lock
 *
 * Read the value from the CANopen object dictionary object entry.
 *
 * @note The caller is responsible to locking the object dictionary using @a
 * canopen_od_lock() before calling this function and unlocking it using @a
 * canopen_od_unlock() afterwards.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param handle Handle for the CANopen object dictionary object entry
 * @param value Pointer to where to store the value.
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 * @retval -ENXIO if the handle is invalid
 */
int canopen_od_read_by_handle_unlocked(const struct canopen_od *od, canopen_od_handle_t handle,
				       void *value, size_t size, uint32_t *abort_code);

/**
 * @brief Read from a CANopen object dictionary object entry by handle
 *
 * Lock the CANopen object dictionary, read the value from the CANopen object
 * dictionary object entry, and unlock the CANopen object dictionary again.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param handle Handle for the CANopen object dictionary object entry
 * @param value Pointer to where to store the value.
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 * @retval -ENXIO if the handle is invalid
 */
static inline int canopen_od_read_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
					    void *value, size_t size, uint32_t *abort_code)
{
	int err;

	canopen_od_lock(od, K_FOREVER);
	err = canopen_od_read_by_handle_unlocked(od, handle, value, size, abort_code);
	canopen_od_unlock(od);

	return err;
}

/**
 * @brief Read from a CANopen object dictionary object entry by index and sub-index
 *
 * Lock the CANopen object dictionary, read the value from the CANopen object
 * dictionary object entry, and unlock the CANopen object dictionary again.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param index Index of the CANopen object dictionary object
 * @param subindex Sub-index of the CANopen object dictionary object entry. For
 *                 non-array/non-record dictionary objects the sub-index is always 0.
 * @param value Pointer to where to store the value.
 * @param size Size of @a value in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 * @retval -ENXIO if subindex does not exist
 */
static inline int canopen_od_read(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				  void *value, size_t size, uint32_t *abort_code)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_read_by_handle(od, handle, value, size, abort_code);
}

/** @} */

/**
 * @name CANopen object dictionary UNSIGNED32 accessor functions
 * @{
 */

static inline int canopen_od_set_u32_by_handle_unlocked(const struct canopen_od *od,
							canopen_od_handle_t handle, uint32_t value,
							uint32_t *abort_code)
{
	/* TODO: check type and bits */

	return canopen_od_set_by_handle_unlocked(od, handle, &value, sizeof(value), abort_code);
}

static inline int canopen_od_set_u32_by_handle(const struct canopen_od *od,
					       canopen_od_handle_t handle, uint32_t value,
					       uint32_t *abort_code)
{
	/* TODO: check type and bits */

	return canopen_od_set_by_handle(od, handle, &value, sizeof(value), abort_code);
}

static inline int canopen_od_set_u32(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				     uint32_t value, uint32_t *abort_code)
{
	/* TODO: check type and bits */

	return canopen_od_set(od, index, subindex, &value, sizeof(value), abort_code);
}

static inline int canopen_od_write_u32_by_handle_unlocked(const struct canopen_od *od,
							  canopen_od_handle_t handle,
							  uint32_t value, uint32_t *abort_code)
{
	/* TODO: check type and bits */

	return canopen_od_write_by_handle_unlocked(od, handle, &value, sizeof(value), abort_code);
}

static inline int canopen_od_write_u32_by_handle(const struct canopen_od *od,
						 canopen_od_handle_t handle, uint32_t value,
						 uint32_t *abort_code)
{
	/* TODO: check type and bits */

	return canopen_od_write_by_handle(od, handle, &value, sizeof(value), abort_code);
}

static inline int canopen_od_write_u32(const struct canopen_od *od, uint16_t index,
				       uint8_t subindex, uint32_t value, uint32_t *abort_code)
{
	/* TODO: check type and bits */

	return canopen_od_write(od, index, subindex, &value, sizeof(value), abort_code);
}

static inline int canopen_od_read_u32_by_handle_unlocked(const struct canopen_od *od,
							 canopen_od_handle_t handle,
							 uint32_t *value, uint32_t *abort_code)
{
	/* TODO: check type and bits */

	return canopen_od_read_by_handle_unlocked(od, handle, value, sizeof(*value), abort_code);
}

static inline int canopen_od_read_u32_by_handle(const struct canopen_od *od,
						canopen_od_handle_t handle, uint32_t *value,
						uint32_t *abort_code)
{
	/* TODO: check type and bits */

	return canopen_od_read_by_handle(od, handle, value, sizeof(*value), abort_code);
}

static inline int canopen_od_read_u32(const struct canopen_od *od, uint16_t index, uint8_t subindex,
				      uint32_t *value, uint32_t *abort_code)
{
	/* TODO: check type and bits */

	return canopen_od_read(od, index, subindex, value, sizeof(*value), abort_code);
}

/** @} */

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_OD_H_ */
