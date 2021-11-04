/*
 * Copyright (c) 2021-2023 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/canbus/canopen/od.h>
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

#define INDEX_OF_ARRAY(_array, _item)			\
	((POINTER_TO_UINT(_item) - POINTER_TO_UINT(_array)) / sizeof(_array[0]))

int canopen_od_lock(struct canopen_od *od, k_timeout_t timeout)
{
	__ASSERT_NO_MSG(od != NULL);

	return k_mutex_lock(&od->lock, timeout);
}

int canopen_od_unlock(struct canopen_od *od)
{
	__ASSERT_NO_MSG(od != NULL);

	return k_mutex_unlock(&od->lock);
}

bool canopen_od_handle_is_valid(canopen_od_handle_t handle)
{
	/* TODO: check against sub-index 0 for sub-indexes larger than zero */
	return (handle & HANDLE_VALID) == HANDLE_VALID;
}

bool canopen_od_handle_is_index(canopen_od_handle_t handle, uint16_t index)
{
	if ((handle & HANDLE_OBJECT_IDX_VALID) == HANDLE_OBJECT_IDX_VALID) {
		/* TODO */
	}

	return false;
}

bool canopen_od_handle_is_subindex(canopen_od_handle_t handle, uint8_t subindex)
{
	if ((handle & HANDLE_ENTRY_IDX_VALID) == HANDLE_ENTRY_IDX_VALID) {
		/* TODO: check against sub-index 0 for sub-indexes larger than zero */
		/* TODO */
	}

	return false;
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

static inline struct canopen_od_object *canopen_od_get_object(const struct canopen_od *od,
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

static inline struct canopen_od_entry *canopen_od_get_entry(const struct canopen_od *od,
							    canopen_od_handle_t handle)
{
	struct canopen_od_entry *entry = NULL;
	struct canopen_od_object *obj;
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

		entry = &obj->entries[entry_idx];
	}

	return entry;
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
		obj_idx = INDEX_OF_ARRAY(od->objects, obj);
		handle |= HANDLE_OBJECT_IDX_VALID | FIELD_PREP(HANDLE_OBJECT_IDX, obj_idx);

		return canopen_od_find_by_handle(od, handle, subindex);
	}

	return handle;
}

canopen_od_handle_t canopen_od_find_by_handle(const struct canopen_od *od,
					      canopen_od_handle_t handle, uint8_t subindex)
{
	struct canopen_od_object *obj;
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
			entry_idx = INDEX_OF_ARRAY(obj->entries, entry);
			handle |= HANDLE_ENTRY_IDX_VALID | FIELD_PREP(HANDLE_ENTRY_IDX, entry_idx);
		}
	}

	return handle;
}

int canopen_od_set_callback(struct canopen_od *od, uint16_t index,
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

int canopen_od_set_by_handle_unlocked(struct canopen_od *od, canopen_od_handle_t handle,
				      void *value, size_t len,
				      enum canopen_sdo_abort_code *abort_code)
{
	canopen_od_callback_handler_t callback;
	struct canopen_od_object *obj;
	struct canopen_od_entry *entry;
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

	callback = obj->callback;
	if (callback != NULL) {
		err = callback(od, obj, entry, false, value, len, abort_code, obj->user_data);
	}

	if (err == 0) {
		/* TODO: store value in entry */
	}

	return err;
}

int canopen_od_write_by_handle_unlocked(struct canopen_od *od, canopen_od_handle_t handle,
					void *value, size_t len,
					enum canopen_sdo_abort_code *abort_code)
{
	/* TODO: check if writeable */

	return canopen_od_set_by_handle_unlocked(od, handle, value, len, abort_code);
}

int canopen_od_read_by_handle_unlocked(struct canopen_od *od, canopen_od_handle_t handle,
				       void *value, size_t len,
				       enum canopen_sdo_abort_code *abort_code)
{
	canopen_od_callback_handler_t callback;
	struct canopen_od_object *obj;
	struct canopen_od_entry *entry;
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
	/* TODO: check writeable */
	/* TODO: check within limits */

	callback = obj->callback;
	if (callback != NULL) {
		err = callback(od, obj, entry, true, value, len, abort_code, obj->user_data);
	}

	if (err == 0) {
		/* TODO: if callback returns 0, ... */
	}

	return err;
}
