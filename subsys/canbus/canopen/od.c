/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/canbus/canopen/od.h>
#include <zephyr/canbus/canopen/sdo.h> /* for SDO abort codes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/check.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(canopen_od, CONFIG_CANOPEN_LOG_LEVEL);

/*
 * Internal canopen_od_handle_t fields:
 *
 *  31 30         23                        8 7           0
 * +-------------+-------------+-------------+-------------+
 * | O  E        |        object array       | entry array |
 * | V  V        |           index           |    index    |
 * +-------------+-------------+-------------+-------------+
 */
#define HANDLE_ENTRY_IDX        GENMASK(7, 0)
#define HANDLE_OBJECT_IDX       GENMASK(23, 8)
#define HANDLE_ENTRY_IDX_VALID  BIT(30)
#define HANDLE_OBJECT_IDX_VALID BIT(31)

#define HANDLE_VALID (HANDLE_OBJECT_IDX_VALID | HANDLE_ENTRY_IDX_VALID)

#define ARRAY_INDEX_PTR(_array_ptr, _item_ptr)                                                     \
	((POINTER_TO_UINT(_item_ptr) - POINTER_TO_UINT(_array_ptr)) / sizeof(_array_ptr[0]))

bool canopen_od_handle_is_valid(canopen_od_handle_t handle)
{
	/* TODO: check against sub-index 0 for sub-indexes larger than zero */
	return (handle & HANDLE_VALID) == HANDLE_VALID;
}

static int canopen_od_compare_object(const void *key, const void *element)
{
	const struct canopen_od_object *obj = (struct canopen_od_object *)element;
	const uint16_t *index = (uint16_t *)key;

	return *index - obj->index;
}

static inline struct canopen_od_object *canopen_od_find_object(const struct canopen_od *od,
							       uint16_t index)
{
	return bsearch(&index, od->objects, od->num_objects, sizeof(od->objects[0]),
		       canopen_od_compare_object);
}

static inline const struct canopen_od_object *canopen_od_get_object(const struct canopen_od *od,
								    canopen_od_handle_t handle)
{
	uint16_t obj_idx;

	if ((handle & HANDLE_OBJECT_IDX_VALID) != HANDLE_OBJECT_IDX_VALID) {
		return NULL;
	}

	obj_idx = FIELD_GET(HANDLE_OBJECT_IDX, handle);
	if (obj_idx >= od->num_objects) {
		return NULL;
	}

	return &od->objects[obj_idx];
}

static int canopen_od_compare_entry(const void *key, const void *element)
{
	const struct canopen_od_entry *entry = (struct canopen_od_entry *)element;
	const uint8_t *subindex = (uint8_t *)key;

	return *subindex - entry->subindex;
}

static inline struct canopen_od_entry *canopen_od_find_entry(const struct canopen_od_object *obj,
							     uint8_t subindex)
{
	return bsearch(&subindex, obj->entries, obj->num_entries, sizeof(obj->entries[0]),
		       canopen_od_compare_entry);
}

static inline const struct canopen_od_entry *canopen_od_get_entry(const struct canopen_od *od,
								  canopen_od_handle_t handle)
{
	const struct canopen_od_object *obj;
	uint8_t entry_idx;

	if ((handle & HANDLE_VALID) != HANDLE_VALID) {
		return NULL;
	}

	obj = canopen_od_get_object(od, handle);
	if (obj != NULL) {
		entry_idx = FIELD_GET(HANDLE_ENTRY_IDX, handle);
		if (entry_idx >= obj->num_entries) {
			return NULL;
		}

		return &obj->entries[entry_idx];
	}

	return NULL;
}

int canopen_od_foreach_entry(const struct canopen_od *od, canopen_od_foreach_entry_callback_t cb,
			     void *user_data)
{
	const struct canopen_od_object *obj;
	canopen_od_handle_t handle;
	uint16_t obj_idx;
	uint8_t entry_idx;
	int err;

	CHECKIF(od == NULL) {
		LOG_DBG("NULL od");
		return -EINVAL;
	}

	CHECKIF(cb == NULL) {
		LOG_DBG("NULL cb");
		return -EINVAL;
	}

	for (obj_idx = 0U; obj_idx < od->num_objects; obj_idx++) {
		obj = &od->objects[obj_idx];

		for (entry_idx = 0U; entry_idx < obj->num_entries; entry_idx++) {
			handle = HANDLE_VALID | FIELD_PREP(HANDLE_OBJECT_IDX, obj_idx) |
				 FIELD_PREP(HANDLE_ENTRY_IDX, entry_idx);

			err = cb(od, handle, user_data);
			if (err != 0U) {
				return err;
			}
		}
	}

	return 0;
}

int canopen_od_handle_get_index(const struct canopen_od *od, canopen_od_handle_t handle,
				uint16_t *index)
{
	const struct canopen_od_object *obj;

	obj = canopen_od_get_object(od, handle);
	if (obj == NULL) {
		return -EINVAL;
	}

	*index = obj->index;

	return 0;
}

int canopen_od_handle_get_subindex(const struct canopen_od *od, canopen_od_handle_t handle,
				   uint8_t *subindex)
{
	const struct canopen_od_entry *entry;

	entry = canopen_od_get_entry(od, handle);
	if (entry == NULL) {
		return -EINVAL;
	}

	*subindex = entry->subindex;

	return 0;
}

canopen_od_handle_t canopen_od_find(const struct canopen_od *od, uint16_t index, uint8_t subindex)
{
	struct canopen_od_object *obj;
	canopen_od_handle_t handle = 0;
	uint16_t obj_idx;

	CHECKIF(od == NULL) {
		LOG_DBG("NULL od");
		return -EINVAL;
	}

	obj = canopen_od_find_object(od, index);
	if (obj != NULL) {
		obj_idx = ARRAY_INDEX_PTR(od->objects, obj);
		handle |= HANDLE_OBJECT_IDX_VALID | FIELD_PREP(HANDLE_OBJECT_IDX, obj_idx);

		return canopen_od_find_by_handle(od, handle, subindex);
	}

	return handle;
}

canopen_od_handle_t canopen_od_find_by_handle(const struct canopen_od *od,
					      canopen_od_handle_t handle, uint8_t subindex)
{
	const struct canopen_od_object *obj;
	struct canopen_od_entry *entry;
	uint8_t entry_idx;

	CHECKIF(od == NULL) {
		LOG_DBG("NULL od");
		return -EINVAL;
	}

	/* Clear existing entry idx */
	handle &= ~(HANDLE_ENTRY_IDX_VALID | HANDLE_ENTRY_IDX);

	obj = canopen_od_get_object(od, handle);
	if (obj != NULL) {
		entry = canopen_od_find_entry(obj, subindex);
		if (entry != NULL) {
			entry_idx = ARRAY_INDEX_PTR(obj->entries, entry);
			handle |= HANDLE_ENTRY_IDX_VALID | FIELD_PREP(HANDLE_ENTRY_IDX, entry_idx);
		}
	}

	return handle;
}

int canopen_od_get_type_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
				  uint16_t *type)
{
	const struct canopen_od_entry *entry;

	entry = canopen_od_get_entry(od, handle);
	if (entry == NULL) {
		return -EINVAL;
	}

	*type = entry->type;

	return 0;
}

int canopen_od_get_bits_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
				  uint8_t *bits)
{
	const struct canopen_od_entry *entry;

	entry = canopen_od_get_entry(od, handle);
	if (entry == NULL) {
		return -EINVAL;
	}

	*bits = entry->bits;

	return 0;
}

int canopen_od_get_size_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
				  size_t *size)
{
	const struct canopen_od_entry *entry;

	entry = canopen_od_get_entry(od, handle);
	if (entry == NULL) {
		return -EINVAL;
	}

	*size = entry->size;

	return 0;
}

int canopen_od_get_attr_by_handle(const struct canopen_od *od, canopen_od_handle_t handle,
				  uint8_t *attr)
{
	const struct canopen_od_entry *entry;

	entry = canopen_od_get_entry(od, handle);
	if (entry == NULL) {
		return -EINVAL;
	}

	*attr = entry->attr;

	return 0;
}

int canopen_od_set_callback(const struct canopen_od *od, uint16_t index,
			    canopen_od_callback_handler_t cb, void *user_data)
{
	struct canopen_od_object *obj;

	CHECKIF(od == NULL) {
		LOG_DBG("NULL od");
		return -EINVAL;
	}

	obj = canopen_od_find_object(od, index);
	if (obj == NULL) {
		LOG_ERR("failed to set callback, index %04xh does not exist", index);
		return -EINVAL;
	}

	canopen_od_lock(od, K_FOREVER);
	obj->callback = cb;
	obj->user_data = user_data;
	canopen_od_unlock(od);

	return 0;
}

int canopen_od_set_by_handle_unlocked(const struct canopen_od *od, canopen_od_handle_t handle,
				      void *value, size_t size, uint32_t *abort_code)
{
	canopen_od_callback_handler_t callback;
	const struct canopen_od_object *obj;
	const struct canopen_od_entry *entry;
	int err = 0;

	CHECKIF(od == NULL) {
		LOG_DBG("NULL od");
		return -EINVAL;
	}

	CHECKIF(value == NULL) {
		LOG_DBG("NULL value");
		return -EINVAL;
	}

	obj = canopen_od_get_object(od, handle);
	if (obj == NULL) {
		if (abort_code != NULL) {
			*abort_code = CANOPEN_SDO_ABORT_OBJECT_DOES_NOT_EXIST;
		}

		return -EINVAL;
	}

	entry = canopen_od_get_entry(od, handle);
	/* TODO: check sub-index 0 */
	if (entry == NULL) {
		if (abort_code != NULL) {
			*abort_code = CANOPEN_SDO_ABORT_SUBINDEX_DOES_NOT_EXIST;
		}

		return -EINVAL;
	}

	/* TODO: check entry type (delegated to type-safe wrapper) */
	/* TODO: check within limits (delegated to type-safe wrapper) */

	callback = obj->callback;
	if (callback != NULL) {
		err = callback(od, obj, entry, false, value, size, abort_code, obj->user_data);
	}

	if (err == 0) {
		/* TODO: store value in entry */
	}

	return err;
}

int canopen_od_write_by_handle_unlocked(const struct canopen_od *od, canopen_od_handle_t handle,
					void *value, size_t size, uint32_t *abort_code)
{
	/* TODO: check if writeable */

	return canopen_od_set_by_handle_unlocked(od, handle, value, size, abort_code);
}

int canopen_od_read_by_handle_unlocked(const struct canopen_od *od, canopen_od_handle_t handle,
				       void *value, size_t size, uint32_t *abort_code)
{
	canopen_od_callback_handler_t callback;
	const struct canopen_od_object *obj;
	const struct canopen_od_entry *entry;
	int err = 0;

	CHECKIF(od == NULL) {
		LOG_DBG("NULL od");
		return -EINVAL;
	}

	CHECKIF(value == NULL) {
		LOG_DBG("NULL value");
		return -EINVAL;
	}

	obj = canopen_od_get_object(od, handle);
	if (obj == NULL) {
		if (abort_code != NULL) {
			*abort_code = CANOPEN_SDO_ABORT_OBJECT_DOES_NOT_EXIST;
		}

		return -EINVAL;
	}

	entry = canopen_od_get_entry(od, handle);
	/* TODO: check sub-index 0 */
	if (entry == NULL) {
		if (abort_code != NULL) {
			*abort_code = CANOPEN_SDO_ABORT_SUBINDEX_DOES_NOT_EXIST;
		}

		return -EINVAL;
	}

	/* TODO: check entry type */
	/* TODO: check if readable */

	callback = obj->callback;
	if (callback != NULL) {
		err = callback(od, obj, entry, true, value, size, abort_code, obj->user_data);
	}

	if (err == 0) {
		/* TODO: if callback returns 0, ... */
	}

	return err;
}
