/*
 * Copyright (c) 2020 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_lpc_i2s

#include <drivers/i2s.h>
#include <fsl_i2s.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(i2s_mcux_flexcomm);

/* TODO: replace I2S_NUM_BUFFERS with CONFIG_... */

struct i2s_mcux_flexcomm_config {
	I2S_Type *tx_base;
	I2S_Type *rx_base;
	void (*irq_config_func)(struct device *dev);
};

struct i2s_mcux_flexcomm_stream {
	enum i2s_state state;
	i2s_handle_t handle;
	struct i2s_config cfg;
	struct k_sem sem;
	bool last_block;
	/* TODO: remove these functions from struct */
	void (*defaults)(i2s_config_t *config);
	void (*init)(I2S_Type *base, const i2s_config_t *config);
};

struct i2s_mcux_flexcomm_data {
	struct i2s_mcux_flexcomm_stream rx;
	struct i2s_mcux_flexcomm_stream tx;
};

static int i2s_mcux_flexcomm_configure(struct device *dev, enum i2s_dir dir,
				       struct i2s_config *cfg)
{
	const struct i2s_mcux_flexcomm_config *config = dev->config_info;
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;
	struct i2s_mcux_flexcomm_stream *stream;
	i2s_config_t mcux_cfg;
	I2S_Type *base;

	switch (dir) {
	case I2S_DIR_TX:
		base = config->tx_base;
		stream = &data->tx;
		break;
	case I2S_DIR_RX:
		base = config->rx_base;
		stream = &data->rx;
		break;
	default:
		__ASSERT(0, "invalid I2S direction");
		return -EINVAL;
	}

	if (stream->state != I2S_STATE_NOT_READY &&
	    stream->state != I2S_STATE_READY) {
		LOG_DBG("attempt to configure in state %d", stream->state);
		return -EINVAL;
	}

	if (cfg->frame_clk_freq == 0U) {
		/* TODO: drop queue */
		(void)memset(&stream->cfg, 0, sizeof(struct i2s_config));
		stream->state = I2S_STATE_NOT_READY;
		return 0;
	}

	stream->defaults(&mcux_cfg);

	/* I2S format */
	switch (cfg->format & I2S_FMT_DATA_FORMAT_MASK) {
	case I2S_FMT_DATA_FORMAT_I2S:
		mcux_cfg.mode = kI2S_ModeI2sClassic;
		break;
	case I2S_FMT_DATA_FORMAT_PCM_SHORT:
		mcux_cfg.mode = kI2S_ModeDspWsShort;
		break;
	case I2S_FMT_DATA_FORMAT_PCM_LONG:
		mcux_cfg.mode = kI2S_ModeDspWsLong;
		break;
	case I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED:
		mcux_cfg.mode = 0/* TODO */;
		mcux_cfg.leftJust = true;
		break;
	case I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED:
		mcux_cfg.mode = 0/* TODO */;
		break;
	default:
		LOG_ERR("Unsupported I2S data format");
		return -EINVAL;
	}

	/* Endianess */
	if (cfg->format & I2S_FMT_DATA_ORDER_INV) {
		LOG_ERR("inverted data order not supported");
		return -ENOTSUP;
	}

	/* Bit clock polarity */
	if (cfg->format & I2S_FMT_BIT_CLK_INV) {
		mcux_cfg.sckPol = true;
	}

	/* Frame clock polarity */
	if (cfg->format & I2S_FMT_FRAME_CLK_INV) {
		mcux_cfg.wsPol = true;
	}

	/* Continous or gated bit clock */
	if (cfg->options & I2S_OPT_BIT_CLK_GATED) {
		LOG_ERR("gated bit clock not supported");
		return -ENOTSUP;
	}

	if (cfg->options & I2S_OPT_BIT_CLK_SLAVE) {
		
	}

	if (cfg->options & I2S_OPT_FRAME_CLK_SLAVE) {
		
	}

	/* Loopback */
	if (cfg->options & I2S_OPT_LOOPBACK) {
		LOG_ERR("loopback not supported");
		return -ENOTSUP;
	}

	/* Ping pong */
	if (cfg->options & I2S_OPT_PINGPONG) {
		LOG_ERR("pingpong not supported");
		return -ENOTSUP;
	}

	/* TODO: configure bit clock */

	memcpy(&stream->cfg, cfg, sizeof(struct i2s_config));

	stream->init(base, &mcux_cfg);
	stream->state = I2S_STATE_READY;

	return 0;
}

static struct i2s_config *i2s_mcux_flexcomm_config_get(struct device *dev,
						       enum i2s_dir dir)
{
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;
	struct i2s_mcux_flexcomm_stream *stream;

	switch (dir) {
	case I2S_DIR_TX:
		stream = &data->tx;
		break;
	case I2S_DIR_RX:
		stream = &data->rx;
		break;
	default:
		__ASSERT(0, "invalid I2S direction");
		return NULL;
	}

	if (stream->state == I2S_STATE_NOT_READY) {
		return NULL;
	}

	return &stream->cfg;
}

static int i2s_mcux_flexcomm_read(struct device *dev, void **mem_block,
				  size_t *size)
{
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;
	int err;

	if (data->rx.state == I2S_STATE_NOT_READY) {
		LOG_DBG("attempt to read while not ready");
		return -EIO;
	}

	if (data->rx.state != I2S_STATE_ERROR) {
		err = k_sem_take(&data->rx.sem,
				 SYS_TIMEOUT_MS(data->rx.cfg.timeout));
		if (err < 0) {
			return -EIO;
		}
	}

	/* TODO: read from queue */

	return 0;
}

static int i2s_mcux_flexcomm_write(struct device *dev, void *mem_block,
				   size_t size)
{
	const struct i2s_mcux_flexcomm_config *config = dev->config_info;
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;
	i2s_transfer_t transfer;
	status_t status;
	int err;

	if (data->tx.state != I2S_STATE_READY &&
	    data->tx.state != I2S_STATE_RUNNING) {
		LOG_DBG("attemp to write while not ready/running");
		return -EIO;
	}

	/* TODO: check size, return -EINVAL if size > queue memory block */

	/* Wait for queue to have room for tx block */
	err = k_sem_take(&data->tx.sem,
			 SYS_TIMEOUT_MS(data->tx.cfg.timeout));
	if (err < 0) {
		return err;
	}

	/* Enqueue transfer */
	transfer.data = mem_block;
	transfer.dataSize = size;
	status = I2S_TxTransferNonBlocking(config->tx_base, &data->tx.handle,
					   transfer);
	if (status != kStatus_Success) {
		/* TODO: revisit status/error codes */
		LOG_ERR("failed to enqueue I2S tx block (status = %d)", status);
		return -ENOMEM;
	}

	return 0;
}

static int i2s_mcux_flexcomm_trigger(struct device *dev, enum i2s_dir dir,
				     enum i2s_trigger_cmd cmd)
{
	const struct i2s_mcux_flexcomm_config *config = dev->config_info;
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;
	struct i2s_mcux_flexcomm_stream *stream;
	I2S_Type *base;

	switch (dir) {
	case I2S_DIR_TX:
		base = config->tx_base;
		stream = &data->tx;
		break;
	case I2S_DIR_RX:
		base = config->rx_base;
		stream = &data->rx;
		break;
	default:
		__ASSERT(0, "invalid I2S direction");
		return -EINVAL;
	}

	switch (cmd) {
	case I2S_TRIGGER_START:
		/* TODO */
		break;
	case I2S_TRIGGER_STOP:
		/* TODO */
		break;
	case I2S_TRIGGER_DRAIN:
		/* TODO */
		break;
	case I2S_TRIGGER_DROP:
		/* TODO */
		break;
	case I2S_TRIGGER_PREPARE:
		/* TODO */
		break;
	default:
		LOG_ERR("unsupported trigger command");
		return -EINVAL;
	};

	return 0;
}

static void i2s_mcux_flexcomm_tx_callback(I2S_Type *base, i2s_handle_t *handle,
                                          status_t status, void *user_data)
{
	struct device *dev = (struct device *)user_data;
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;

	/* TODO: free memory slab passed to _write() here */

	if (status == kStatus_I2S_BufferComplete) {
		k_sem_give(&data->tx.sem);
	}

	/* TODO: check for errors */
}

static void i2s_mcux_flexcomm_rx_callback(I2S_Type *base, i2s_handle_t *handle,
                                          status_t status, void *user_data)
{
	struct device *dev = (struct device *)user_data;
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;

	/* TODO */
}

static void i2s_mcux_flexcomm_tx_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct i2s_mcux_flexcomm_config *config = dev->config_info;
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;

	I2S_TxHandleIRQ(config->tx_base, &data->tx.handle);
}

static void i2s_mcux_flexcomm_rx_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct i2s_mcux_flexcomm_config *config = dev->config_info;
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;

	I2S_RxHandleIRQ(config->rx_base, &data->rx.handle);
}

static int i2s_mcux_flexcomm_init(struct device *dev)
{
	const struct i2s_mcux_flexcomm_config *config = dev->config_info;
	struct i2s_mcux_flexcomm_data *data = dev->driver_data;

	k_sem_init(&data->tx.sem, 0, I2S_NUM_BUFFERS);
	data->tx.init = I2S_TxInit;
	data->tx.defaults = I2S_TxGetDefaultConfig;

	k_sem_init(&data->rx.sem, 0, I2S_NUM_BUFFERS);
	data->rx.init = I2S_RxInit;
	data->rx.defaults = I2S_RxGetDefaultConfig;

	I2S_TxTransferCreateHandle(config->tx_base, &data->tx.handle,
				   &i2s_mcux_flexcomm_tx_callback, dev);
	I2S_RxTransferCreateHandle(config->rx_base, &data->rx.handle,
				   &i2s_mcux_flexcomm_rx_callback, dev);

	config->irq_config_func(dev);

	return 0;
}

static const struct i2s_driver_api i2s_mcux_flexcomm_driver_api = {
	.configure = i2s_mcux_flexcomm_configure,
	.config_get = i2s_mcux_flexcomm_config_get,
	.read = i2s_mcux_flexcomm_read,
	.write = i2s_mcux_flexcomm_write,
	.trigger = i2s_mcux_flexcomm_trigger,
};

#define I2S_MCUX_FLEXCOMM_DEVICE(id)					\
	static void i2s_mcux_flexcomm_config_func_##id(struct device *dev); \
	static const struct i2s_mcux_flexcomm_config i2s_mcux_flexcomm_config_##id = { \
		.tx_base = (I2S_Type *)DT_REG_ADDR(DT_INST_PHANDLE(id, nxp_flexcomm_tx)), \
		.rx_base = (I2S_Type *)DT_REG_ADDR(DT_INST_PHANDLE(id, nxp_flexcomm_rx)), \
		.irq_config_func = i2s_mcux_flexcomm_config_func_##id,	\
	};								\
	static struct i2s_mcux_flexcomm_data i2s_mcux_flexcomm_data_##id; \
	DEVICE_AND_API_INIT(i2s_mcux_flexcomm_##id, DT_INST_LABEL(id),	\
			&i2s_mcux_flexcomm_init, &i2s_mcux_flexcomm_data_##id, \
			&i2s_mcux_flexcomm_config_##id, POST_KERNEL,	\
			CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		\
			&i2s_mcux_flexcomm_driver_api);			\
	static void i2s_mcux_flexcomm_config_func_##id(struct device *dev) \
	{								\
		IRQ_CONNECT(DT_IRQN(DT_INST_PHANDLE(id, nxp_flexcomm_tx)), \
			DT_IRQ(DT_INST_PHANDLE(id, nxp_flexcomm_tx), priority),	\
			i2s_mcux_flexcomm_tx_isr,			\
			DEVICE_GET(i2s_mcux_flexcomm_##id), 0);		\
		irq_enable(DT_IRQN(DT_INST_PHANDLE(id, nxp_flexcomm_tx))); \
									\
		IRQ_CONNECT(DT_IRQN(DT_INST_PHANDLE(id, nxp_flexcomm_rx)), \
			DT_IRQ(DT_INST_PHANDLE(id, nxp_flexcomm_rx), priority),	\
			i2s_mcux_flexcomm_rx_isr,			\
			DEVICE_GET(i2s_mcux_flexcomm_##id), 0);		\
		irq_enable(DT_IRQN(DT_INST_PHANDLE(id, nxp_flexcomm_rx))); \
	}

DT_INST_FOREACH_STATUS_OKAY(I2S_MCUX_FLEXCOMM_DEVICE)
