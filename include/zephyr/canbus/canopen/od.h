/*
 * Copyright (c) 2021-2023 Henrik Brix Andersen <henrik@brixandersen.dk>
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
#include <zephyr/types.h>
#include <zephyr/canbus/canopen/sdo.h> /* SDO abort codes */

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct canopen_od;
struct canopen_od_object;
struct canopen_od_entry;

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
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result
 * @param user_data User data provided when registering the callback
 *
 * @retval Zero on success or negative error code
 */
typedef int (*canopen_od_callback_handler_t)(const struct canopen_od *od,
					     const struct canopen_od_object *obj,
					     const struct canopen_od_entry *entry,
					     bool reading, void *value, size_t size,
					     enum canopen_sdo_abort_code *abort_code,
					     void *user_data);

/**
 * @brief CANopen object dictionary object entry
 */
struct canopen_od_entry {
	/** 8-bit sub-index of this CANopen object dictionary object entry. */
	const uint8_t subindex;
	/** Pointer to the data storage for this entry. */
	void *data;
	/** Pointer to the minimum allowed data value for this entry or NULL. */
	void *min;
	/** Pointer to the maximum allowed data value for this entry or NULL. */
	void *max;
	/** Size of of this entry (number of bytes used by each of *data, *min, and *max) */
	size_t size;
	/** Attributes for this entry. */
	uint32_t attr;
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
	 * Array of CANopen object dictionary entries for this object, ordered by
	 * ascending sub-index.
	 */
	struct canopen_od_entry *entries;
	/** Number of entries. */
	uint8_t num_entries;
};

/**
 * @brief CANopen object dictionary
 */
struct canopen_od {
	/** CANopen object dictionary lock. */
	struct k_mutex lock;
	/** Array of CANopen object dictionary objects, ordered by ascending index. */
	struct canopen_od_object *objects;
	/** Number of objects. */
	uint16_t num_objects;
};

/**
 * @name CANopen object dictionary entry type attribute
 * @{
 */
/** @cond INTERNAL_HIDDEN */
#define CANOPEN_OD_ATTR_TYPE_MASK       GENMASK(2, 0)
/** @endcond */
/** Specify type */
#define CANOPEN_OD_ATTR_TYPE(_type)     FIELD_PREP(CANOPEN_OD_ATTR_TYPE_MASK, _type)
/** Get type */
#define CANOPEN_OD_ATTR_GET_TYPE(_attr) FIELD_GET(CANOPEN_OD_ATTR_TYPE_MASK, _attr)
/** Boolean type */
#define CANOPEN_OD_ATTR_TYPE_BOOLEAN    0x1U
/** Void type */
#define CANOPEN_OD_ATTR_TYPE_VOID       0x2U
/** Unsigned integer type */
#define CANOPEN_OD_ATTR_TYPE_UNSIGNED   0x3U
/** Signed integer type */
#define CANOPEN_OD_ATTR_TYPE_INTEGER    0x4U
/** 32 bit floating point type */
#define CANOPEN_OD_ATTR_TYPE_REAL32     0x5U
/** 64 bit floating point type */
#define CANOPEN_OD_ATTR_TYPE_REAL64     0x6U
/** NIL (empty) type */
#define CANOPEN_OD_ATTR_TYPE_NIL        0x7U
/** @} */

/**
 * @name CANopen object dictionary object entry bit size attribute
 * @{
 */
/** @cond INTERNAL_HIDDEN */
#define CANOPEN_OD_ATTR_BIT_SIZE_MASK       GENMASK(9, 3)
/** @endcond */
/** Specify bit size (0 to 64) */
#define CANOPEN_OD_ATTR_BIT_SIZE(_bits)     FIELD_PREP(CANOPEN_OD_ATTR_BIT_SIZE_MASK, _bits)
/** Get bit size (0 to 64) */
#define CANOPEN_OD_ATTR_GET_BIT_SIZE(_attr) FIELD_GET(CANOPEN_OD_ATTR_BIT_SIZE_MASK, _attr)
/** @} */

/**
 * @name CANopen object dictionary object entry access attributes
 * @{
 */
/** @cond INTERNAL_HIDDEN */
#define CANOPEN_OD_ATTR_ACCESS_MASK       GENMASK(11, 10)
/** @endcond */
/** Read access */
#define CANOPEN_OD_ATTR_ACCESS_READ       BIT(10)
/** Write access */
#define CANOPEN_OD_ATTR_ACCESS_WRITE      BIT(11)
/** Read/write access */
#define CANOPEN_OD_ATTR_ACCESS_READ_WRITE \
	(CANOPEN_OD_ATTR_ACCESS_READ | CANOPEN_OD_ATTR_ACCESS_WRITE)
/** @} */

/**
 * @name CANopen object dictionary object entry PDO mapping attributes
 * @{
 */
/** @cond INTERNAL_HIDDEN */
#define CANOPEN_OD_ATTR_PDO_MAPPABLE_MASK GENMASK(13, 12)
/** @endcond */
/** RPDO mappable */
#define CANOPEN_OD_ATTR_PDO_MAPPABLE_RPDO BIT(12)
/** TPDO mappable */
#define CANOPEN_OD_ATTR_PDO_MAPPABLE_TPDO BIT(13)
/** @} */

/**
 * @name CANopen object dictionary object entry initialization macros
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, _size, _attr)	\
{											\
	.subindex = _subindex,								\
	.data = _data,									\
	.min = _min,									\
	.max = _max,									\
	.size = _size,									\
	.attr = _attr,									\
}
/** @endcond */

/**
 * @brief CANopen object dictionary BOOLEAN object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the bool data
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_BOOLEAN(_subindex, _data, _attr)	\
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, NULL, NULL, sizeof(bool), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_BOOLEAN) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(1) | _attr)

/**
 * @brief CANopen object dictionary UNSIGNED8 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint8_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED8(_subindex, _data, _min, _max, _attr)	\
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(uint8_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(8) | _attr)

/**
 * @brief CANopen object dictionary UNSIGNED16 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint16_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED16(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(uint16_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(16) | _attr)

/**
 * @brief CANopen object dictionary UNSIGNED24 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint32_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED24(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(uint32_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(24) | _attr)

/**
 * @brief CANopen object dictionary UNSIGNED32 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint32_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED32(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(uint32_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(32) | _attr)

/**
 * @brief CANopen object dictionary UNSIGNED40 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED40(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(uint64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(40) | _attr)

/**
 * @brief CANopen object dictionary UNSIGNED48 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED48(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(uint64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(48) | _attr)

/**
 * @brief CANopen object dictionary UNSIGNED56 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED56(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(uint64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(56) | _attr)

/**
 * @brief CANopen object dictionary UNSIGNED64 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNSIGNED64(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(uint64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(64) | _attr)

/**
 * @brief CANopen object dictionary INTEGER8 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int8_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER8(_subindex, _data, _min, _max, _attr)	\
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(int8_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_INTEGER) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(8) | _attr)

/**
 * @brief CANopen object dictionary INTEGER16 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int16_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER16(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(int16_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_INTEGER) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(16) | _attr)

/**
 * @brief CANopen object dictionary INTEGER824 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int32_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER24(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(int32_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_INTEGER) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(24) | _attr)

/**
 * @brief CANopen object dictionary INTEGER32 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int32_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER32(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(int32_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_INTEGER) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(32) | _attr)

/**
 * @brief CANopen object dictionary INTEGER40 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER40(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(int64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_INTEGER) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(40) | _attr)

/**
 * @brief CANopen object dictionary INTEGER48 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER48(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(int64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_INTEGER) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(48) | _attr)

/**
 * @brief CANopen object dictionary INTEGER56 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER56(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(int64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_INTEGER) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(56) | _attr)

/**
 * @brief CANopen object dictionary INTEGER64 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the int64_t data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_INTEGER64(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(int64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_INTEGER) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(64) | _attr)

/**
 * @brief CANopen object dictionary REAL32 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the float data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_REAL32(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(float), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_REAL32) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(32) | _attr)

/**
 * @brief CANopen object dictionary REAL64 object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the double data
 * @param _min Pointer to the minimum value (or NULL)
 * @param _max Pointer to the maximum value (or NULL)
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_REAL64(_subindex, _data, _min, _max, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, _min, _max, sizeof(double), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_REAL64) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(64) | _attr)

/**
 * @brief CANopen object dictionary TIME_OF_DAY object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_TIME_OF_DAY(_subindex, _data, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, NULL, NULL, sizeof(uint64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(48) | _attr)

/**
 * @brief CANopen object dictionary TIME_DIFFERENCE object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint64_t data
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_TIME_DIFFERENCE(_subindex, _data, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, NULL, NULL, sizeof(uint64_t), \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(48) | _attr)

/**
 * @brief CANopen object dictionary VISIBLE_STRING object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint8_t data
 * @param _size Size of the data in bytes
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_VISIBLE_STRING(_subindex, _data, _size, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, NULL, NULL, _size, \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(8) | _attr)

/**
 * @brief CANopen object dictionary OCTET_STRING object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint8_t data
 * @param _size Size of the data in bytes
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_OCTET_STRING(_subindex, _data, _size, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, NULL, NULL, _size, \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(8) | _attr)

/**
 * @brief CANopen object dictionary UNICODE_STRING object entry initialization macro
 * @see canopen_od_entry
 *
 * @param _subindex 8-bit sub-index of the entry
 * @param _data Pointer to the uint16_t data
 * @param _size Size of the data in bytes
 * @param _attr Additional attributes for the entry
 */
#define CANOPEN_OD_ENTRY_UNICODE_STRING(_subindex, _data, _size, _attr) \
	CANOPEN_OD_ENTRY_INITIALIZER(_subindex, _data, NULL, NULL, _size, \
				     CANOPEN_OD_ATTR_TYPE(CANOPEN_OD_ATTR_TYPE_UNSIGNED) | \
				     CANOPEN_OD_ATTR_BIT_SIZE(16) | _attr)
/** @} */

/**
 * @name CANopen object dictionary object initialization macros
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define CANOPEN_OD_OBJECT_INITIALIZER(_index, _entries, _num_entries)	\
{									\
	.index = _index,						\
	.callback = NULL,						\
	.user_data = NULL,						\
	.entries = _entries,						\
	.num_entries = _num_entries,					\
}
/** @endcond */

/**
 * TODO
 */
#define CANOPEN_OD_OBJECT_ENTRIES(_index, _entries...)	\
	CANOPEN_OD_OBJECT_INITIALIZER(_index, ((struct canopen_od_entry []) { _entries }), \
				      ARRAY_SIZE(((struct canopen_od_entry []) { _entries })))
/** @} */

/**
 * @name CANopen object dictionary initialization macros
 * @{
 */

/**
 * TODO
 */
#define CANOPEN_OD_DECLARE(_name) extern struct canopen_od _name

/**
 * @brief CANopen object dictionary initialization macro
 * @see canopen_od
 *
 * @param _name Name of the CANopen object dictionary
 * @param _objects Pointer to the array of objects
 * @param _num_objects Number of objects in the array
 */
#define CANOPEN_OD_DEFINE(_name, _objects, _num_objects)	\
	struct canopen_od _name = {				\
		.lock =	Z_MUTEX_INITIALIZER(_name.lock),	\
		.objects = _objects,				\
		.num_objects = _num_objects,			\
	}

/**
 * TODO
 */
#define CANOPEN_OD_DEFINE_OBJECTS(_name, _objects...)			\
	CANOPEN_OD_DEFINE(_name, ((struct canopen_od_object []) { _objects }), \
			  ARRAY_SIZE(((struct canopen_od_object []) { _objects })))

/** @} */

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
int canopen_od_lock(struct canopen_od *od, k_timeout_t timeout);

/**
 * @brief Unlock the CANopen object dictionary.
 *
 * @param od Pointer to the CANopen object dictionary
 *
 * @retval 0 CANopen object dictionary unlocked
 * @retval -EPERM The current thread does not own the lock
 * @retval -EINVAL The CANopen object dictionary is not locked
 */
int canopen_od_unlock(struct canopen_od *od);

/**
 * TODO
 */
bool canopen_od_handle_is_valid(canopen_od_handle_t handle);

/**
 * TODO
 */
bool canopen_od_handle_is_index(canopen_od_handle_t handle, uint16_t index);

/**
 * TODO
 */
bool canopen_od_handle_is_subindex(canopen_od_handle_t handle, uint8_t subindex);

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
int canopen_od_set_callback(struct canopen_od *od, uint16_t index,
			    canopen_od_callback_handler_t cb, void *user_data);

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
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
int canopen_od_set_by_handle_unlocked(struct canopen_od *od, canopen_od_handle_t handle,
				      void *value, size_t len,
				      enum canopen_sdo_abort_code *abort_code);

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
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
static inline int canopen_od_set_by_handle(struct canopen_od *od, canopen_od_handle_t handle,
					   void *value, size_t len,
					   enum canopen_sdo_abort_code *abort_code)
{
	int err;

	canopen_od_lock(od, K_FOREVER);
	err = canopen_od_set_by_handle_unlocked(od, handle, value, len, abort_code);
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
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
static inline int canopen_od_set(struct canopen_od *od, uint16_t index, uint8_t subindex,
				 void *value, size_t len,
				 enum canopen_sdo_abort_code *abort_code)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_set_by_handle(od, handle, value, len, abort_code);
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
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
int canopen_od_write_by_handle_unlocked(struct canopen_od *od, canopen_od_handle_t handle,
					void *value, size_t len,
					enum canopen_sdo_abort_code *abort_code);

/**
 * @brief Write to a CANopen object dictionary object entry by handle
 *
 * Lock the CANopen object dictionary, write the value to the CANopen object
 * dictionary object entry, and unlock the CANopen object dictionary again.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param handle Handle for the CANopen object dictionary object entry
 * @param value Pointer to the value to be written.
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
static inline int canopen_od_write_by_handle(struct canopen_od *od, canopen_od_handle_t handle,
					     void *value, size_t len,
					     enum canopen_sdo_abort_code *abort_code)
{
	int err;

	canopen_od_lock(od, K_FOREVER);
	err = canopen_od_write_by_handle_unlocked(od, handle, value, len, abort_code);
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
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 */
static inline int canopen_od_write(struct canopen_od *od, uint16_t index, uint8_t subindex,
				   void *value, size_t len,
				   enum canopen_sdo_abort_code *abort_code)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_write_by_handle(od, handle, value, len, abort_code);
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
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 * @retval -ENXIO if the handle is invalid
 */
int canopen_od_read_by_handle_unlocked(struct canopen_od *od, canopen_od_handle_t handle,
				       void *value, size_t len,
				       enum canopen_sdo_abort_code *abort_code);

/**
 * @brief Read from a CANopen object dictionary object entry by handle
 *
 * Lock the CANopen object dictionary, read the value from the CANopen object
 * dictionary object entry, and unlock the CANopen object dictionary again.
 *
 * @param od Pointer to the CANopen object dictionary
 * @param handle Handle for the CANopen object dictionary object entry
 * @param value Pointer to where to store the value.
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 * @retval -ENXIO if the handle is invalid
 */
static inline int canopen_od_read_by_handle(struct canopen_od *od, canopen_od_handle_t handle,
					    void *value, size_t len,
					    enum canopen_sdo_abort_code *abort_code)
{
	int err;

	canopen_od_lock(od, K_FOREVER);
	err = canopen_od_read_by_handle_unlocked(od, handle, value, len, abort_code);
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
 * @param len Value length in bytes
 * @param abort_code Pointer to where to store the SDO abort code result or NULL.
 *
 * @retval 0 on success
 * @retval -EINVAL if invalid function parameters were given
 * @retval -ENXIO if subindex does not exist
 */
static inline int canopen_od_read(struct canopen_od *od, uint16_t index, uint8_t subindex,
				  void *value, size_t len,
				  enum canopen_sdo_abort_code *abort_code)
{
	canopen_od_handle_t handle = canopen_od_find(od, index, subindex);

	return canopen_od_read_by_handle(od, handle, value, len, abort_code);
}

static inline int canopen_od_read_u32_by_handle_unlocked(struct canopen_od *od, canopen_od_handle_t handle,
							 uint32_t *value, enum canopen_sdo_abort_code *abort_code)
{
	/* TODO: check attr type and bits */

	return canopen_od_read_by_handle_unlocked(od, handle, value, sizeof(*value), abort_code);
}

static inline int canopen_od_read_u32_by_handle(struct canopen_od *od, canopen_od_handle_t handle,
						uint32_t *value, enum canopen_sdo_abort_code *abort_code)
{
	/* TODO: check attr type and bits */

	return canopen_od_read_by_handle(od, handle, value, sizeof(*value), abort_code);
}

static inline int canopen_od_read_u32(struct canopen_od *od, uint16_t index, uint8_t subindex,
				      uint32_t *value, enum canopen_sdo_abort_code *abort_code)
{
	/* TODO: check attr type and bits */

	return canopen_od_read(od, index, subindex, value, sizeof(*value), abort_code);
}

static inline int canopen_od_get_attr(struct canopen_od *od, uint16_t index, uint8_t subindex,
				      uint32_t *attr)
{

}

static inline int canopen_od_set_attr(struct canopen_od *od, uint16_t index, uint8_t subindex,
				      uint32_t attr)
{

}

static inline int canopen_od_get_min(struct canopen_od *od, uint16_t index, uint8_t subindex,
				     void *min, size_t len)
{

}

static inline int canopen_od_set_min(struct canopen_od *od, uint16_t index, uint8_t subindex,
				     void *min, size_t len)
{

}

static inline int canopen_od_get_max(struct canopen_od *od, uint16_t index, uint8_t subindex,
				     void *max, size_t len)
{

}

static inline int canopen_od_set_max(struct canopen_od *od, uint16_t index, uint8_t subindex,
				     void *max, size_t len)
{

}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_OD_H_ */
