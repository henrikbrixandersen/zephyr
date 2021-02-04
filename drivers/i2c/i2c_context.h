/*
 * Copyright (c) 2020 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Private API for I2C drivers
 */

#ifndef ZEPHYR_DRIVERS_I2C_I2C_CONTEXT_H_
#define ZEPHYR_DRIVERS_I2C_I2C_CONTEXT_H_

#include <drivers/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

struct i2c_context {
	struct k_sem lock;
	struct k_sem sync;
	int sync_status;

	const struct i2c_msg *current_msg;
	uint32_t msg_count;
	bool first_msg;
	uint16_t addr;

	uint8_t *buf;
	uint32_t buf_len;
};

#define I2C_CONTEXT_INIT_LOCK(_data, _ctx_name)				\
	._ctx_name.lock = Z_SEM_INITIALIZER(_data._ctx_name.lock, 0, 1)

#define I2C_CONTEXT_INIT_SYNC(_data, _ctx_name)				\
	._ctx_name.sync = Z_SEM_INITIALIZER(_data._ctx_name.sync, 0, 1)

static inline void i2c_context_lock(struct i2c_context *ctx)
{
	k_sem_take(&ctx->lock, K_FOREVER);
}

static inline void i2c_context_release(struct i2c_context *ctx)
{
	k_sem_give(&ctx->lock);
}

static inline int i2c_context_wait_for_completion(struct i2c_context *ctx)
{
	k_sem_take(&ctx->sync, K_FOREVER);
	return ctx->sync_status;
}

static inline void i2c_context_complete(struct i2c_context *ctx, int status)
{
	ctx->sync_status = status;
	k_sem_give(&ctx->sync);
}

static inline void i2c_context_transfer_setup(struct i2c_context *ctx,
					      const struct i2c_msg *msgs,
					      uint8_t num_msgs, uint16_t addr)
{
	LOG_DBG("msgs = %p, num_msgs = %d", msgs, num_msgs);

	ctx->current_msg = msgs;
	ctx->msg_count = num_msgs;
	ctx->addr = addr;

	ctx->buf = ctx->current_msg->buf;
	ctx->buf_len = ctx->current_msg->len;

	ctx->sync_status = 0;
	ctx->first_msg = true;
}

static ALWAYS_INLINE void i2c_context_update(struct i2c_context *ctx,
					     uint32_t len)
{
	if (len > ctx->buf_len) {
		LOG_ERR("Update exceeds current buffer");
		return;
	}

	ctx->buf_len -= len;
	if (!ctx->buf_len) {
		ctx->msg_count--;
		if (ctx->msg_count) {
			ctx->current_msg++;
			ctx->first_msg = false;
			ctx->buf = ctx->current_msg->buf;
			ctx->buf_len = ctx->current_msg->len;
		}
	} else {
		ctx->buf += len;
	}
}

static inline bool i2c_context_is_first_msg(struct i2c_context *ctx)
{
	return ctx->first_msg;
}

static inline bool i2c_context_is_start_of_msg(struct i2c_context *ctx)
{
	return ctx->buf_len == ctx->current_msg->len;
}

static inline bool i2c_context_is_end_of_msg(struct i2c_context *ctx)
{
	return ctx->buf_len == 1;
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_I2C_I2C_CONTEXT_H_ */
