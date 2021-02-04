/*
 * Copyright (c) 2020 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT solomon_ssd1331fb

#include <device.h>
#include <drivers/display.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(ssd1331, CONFIG_DISPLAY_LOG_LEVEL);

/* SSD1331 commands */
#define SSD1331_CMD_SET_COL_START_END  0x15
#define SSD1331_CMD_SET_ROW_START_END  0x75

#define SSD1331_CMD_REMAP_AND_COLOR    0xA0

#define SSD1331_CMD_NORMAL_DISPLAY     0xA4
#define SSD1331_CMD_ENTIRE_DISPLAY_OFF 0xA6

#define SSD1331_CMD_SET_MASTER_CONFIG  0xAD

#define SSD1331_CMD_DISPLAY_ON_NORMAL  0xAF
#define SSD1331_CMD_DISPLAY_OFF        0xAE

struct ssd1331_config {
	const char *spi_name;
	const char *spi_cs_name;
	gpio_pin_t spi_cs_pin;
	gpio_dt_flags_t spi_cs_dt_flags;
	const char *gpio_data_cmd_name;
	gpio_pin_t gpio_data_cmd_pin;
	gpio_dt_flags_t gpio_data_cmd_flags;
	const char *gpio_reset_name;
	gpio_pin_t gpio_reset_pin;
	gpio_dt_flags_t gpio_reset_flags;
	const char *gpio_enable_name;
	gpio_pin_t gpio_enable_pin;
	gpio_dt_flags_t gpio_enable_flags;
	struct spi_config spi_cfg;
	uint8_t width;
	uint8_t height;
};

struct ssd1331_data {
	const struct device *spi_dev;
	const struct device *gpio_data_cmd_dev;
	const struct device *gpio_reset_dev;
	const struct device *gpio_enable_dev;
	struct spi_cs_control spi_cs;
};

static int ssd1331_write_buffer(const struct device *dev, const uint8_t *buf,
				size_t len, bool is_data)
{
	const struct ssd1331_config *config = dev->config;
	struct ssd1331_data *data = dev->data;
	int err;
	const struct spi_buf tx_buf = {
		.buf = (uint8_t *)buf,
		.len = len,
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1,
	};

	err = gpio_pin_set(data->gpio_data_cmd_dev, config->gpio_data_cmd_pin,
			   is_data);
	if (err) {
		return err;
	}

	return spi_transceive(data->spi_dev, &config->spi_cfg, &tx, NULL);
}

static int ssd1331_write_cmd(const struct device *dev, const uint8_t *cmd,
			     size_t len)
{
	return ssd1331_write_buffer(dev, cmd, len, false);
}

static int ssd1331_write_cmd8(const struct device *dev, uint8_t cmd)
{
	uint8_t buf[1] = { cmd };

	return ssd1331_write_cmd(dev, buf, sizeof(buf));
}

static int ssd1331_write_data(const struct device *dev, const uint8_t *data,
			      size_t len)
{
	return ssd1331_write_buffer(dev, data, len, true);
}

static int ssd1331_power_on(const struct device *dev)
{
	const struct ssd1331_config *config = dev->config;
	struct ssd1331_data *data = dev->data;
	uint8_t cmds[] = { SSD1331_CMD_SET_MASTER_CONFIG, 0x8E,
			   SSD1331_CMD_REMAP_AND_COLOR,
			   BIT(6) | // 65k format 1 (pixel format)
			   BIT(5) | // COM split odd/even (com-sequential?)
			   BIT(1) | // reverse SEG (segment-remap)
			   BIT(4)   // reverse COM (com-invdir)
	};
	int err;

	/* Pulse reset, if available */
	if (data->gpio_reset_dev) {
		err = gpio_pin_set(data->gpio_reset_dev,
				   config->gpio_reset_pin, 1);
		if (err) {
			return err;
		}

		k_busy_wait(3);

		err = gpio_pin_set(data->gpio_reset_dev,
				   config->gpio_reset_pin, 0);
		if (err) {
			return err;
		}
	}

	/* Enable VCC, if needed */
	if (data->gpio_enable_dev) {
		err = gpio_pin_set(data->gpio_enable_dev,
				   config->gpio_enable_pin, 1);
		if (err) {
			return err;
		}
	}

	/* Select external VCC supply */
	err = ssd1331_write_cmd(dev, cmds, sizeof(cmds));
	if (err) {
		LOG_ERR("failed to initialize display on (err %d)", err);
		return err;
	}

	return 0;
}

static int ssd1331_power_off(const struct device *dev)
{
	const struct ssd1331_config *config = dev->config;
	struct ssd1331_data *data = dev->data;
	int err;

	/* Disable VCC, if available */
	if (data->gpio_enable_dev) {
		err = gpio_pin_set(data->gpio_enable_dev,
				   config->gpio_enable_pin, 0);
		if (err) {
			return err;
		}
	}

	return 0;
}

static int ssd1331_blanking_on(const struct device *dev)
{
	int err;

	err = ssd1331_write_cmd8(dev, SSD1331_CMD_DISPLAY_OFF);
	if (err) {
		LOG_ERR("failed to turn display off (err %d)", err);
		return err;
	}

	return 0;
}

static int ssd1331_blanking_off(const struct device *dev)
{
	uint16_t buf2[96];
	int err;
	int i;

	err = ssd1331_write_cmd8(dev, SSD1331_CMD_DISPLAY_ON_NORMAL);
	if (err) {
		LOG_ERR("failed to turn display on (err %d)", err);
		return err;
	}

	return 0;
}

static int ssd1331_write(const struct device *dev, const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	uint8_t cmds[] = { SSD1331_CMD_SET_COL_START_END, x, x + desc->width - 1,
			   SSD1331_CMD_SET_ROW_START_END, y, y + desc->height - 1 };
	size_t len;
	int err;

	/* TODO: check bounds */
	//LOG_INF("write");

	LOG_INF("x = %u, y = %d, buf_size = %u, width = %u, height = %u, pitch = %u",
		x, y, desc->buf_size, desc->width, desc->height, desc->pitch);

	if (desc->width != desc->pitch) {
		/* TODO: support this */
		LOG_ERR("huh");
	}

	err = ssd1331_write_cmd(dev, cmds, sizeof(cmds));
	if (err) {
		LOG_ERR("failed to write data (err %d)", err);
		return err;
	}

	len = desc->width * desc->height * 2;
	/* TODO: error out if buf_size too small */
	err = ssd1331_write_data(dev, buf, MIN(len, desc->buf_size));
	if (err) {
		LOG_ERR("failed to write data (err %d)", err);
		return err;
	}

	return 0;
}

static int ssd1331_read(const struct device *dev, const uint16_t x,
			const uint16_t y,
			const struct display_buffer_descriptor *desc,
			void *buf)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(x);
	ARG_UNUSED(y);
	ARG_UNUSED(desc);
	ARG_UNUSED(buf);

	/* Read via SPI is not supported by SSD1331 */
	LOG_WRN("read not supported");

	return -ENOTSUP;
}

static void *ssd1331_get_framebuffer(const struct device *dev)
{
	ARG_UNUSED(dev);

	LOG_WRN("get framebuffer not supported");

	return NULL;
}

static int ssd1331_set_brightness(const struct device *dev,
				  const uint8_t brightness)
{
	/* TODO */
	LOG_INF("set_brightness");
	return -ENOTSUP;
}

static int ssd1331_set_contrast(const struct device *dev,
				const uint8_t contrast)
{
	/* TODO */
	LOG_INF("set_contrast");
	return -ENOTSUP;
}

static void ssd1331_get_capabilities(const struct device *dev,
				     struct display_capabilities *caps)
{
	const struct ssd1331_config *config = dev->config;

	memset(caps, 0, sizeof(*caps));
	caps->x_resolution = config->width;
	caps->y_resolution = config->height;
	/* TODO: support PIXEL_FORMAT_BGR_565? */
	caps->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	caps->screen_info = 0;
	caps->current_pixel_format = PIXEL_FORMAT_RGB_565;
	/* TODO: support other orientations? */
	caps->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int ssd1331_set_pixel_format(const struct device *dev,
				    const enum display_pixel_format pixel_fmt)
{
	/* TODO */
	LOG_INF("set_pixel_format");
	return -ENOTSUP;
}

static int ssd1331_set_orientation(const struct device *dev,
				   const enum display_orientation orientation)
{
	/* TODO */
	LOG_INF("set_orientation");
	return -ENOTSUP;
}

static int ssd1331_init(const struct device *dev)
{
	const struct ssd1331_config *config = dev->config;
	struct ssd1331_data *data = dev->data;
	uint8_t cmds[] = { SSD1331_CMD_ENTIRE_DISPLAY_OFF,
			   SSD1331_CMD_NORMAL_DISPLAY };
	int err;

	data->spi_dev = device_get_binding(config->spi_name);
	if (!data->spi_dev) {
		LOG_ERR("SPI device '%s' not found", config->spi_name);
		return -EINVAL;
	}

	if (config->spi_cs_name) {
		data->spi_cs.gpio_dev =
			device_get_binding(config->spi_cs_name);
		if (!data->spi_cs.gpio_dev) {
			LOG_ERR("SPI CS GPIO device '%s' not found",
				config->spi_cs_name);
			return -EINVAL;
		}

		data->spi_cs.gpio_pin = config->spi_cs_pin;
		data->spi_cs.gpio_dt_flags = config->spi_cs_dt_flags;
	}

	data->gpio_data_cmd_dev =
		device_get_binding(config->gpio_data_cmd_name);
	if (!data->gpio_data_cmd_dev) {
		LOG_ERR("data/command GPIO device '%s' not found",
			config->gpio_data_cmd_name);
		return -EINVAL;
	}

	err = gpio_pin_configure(data->gpio_data_cmd_dev,
				 config->gpio_data_cmd_pin,
				 GPIO_OUTPUT | config->gpio_data_cmd_flags);
	if (err) {
		LOG_ERR("failed to configure data/command GPIO (err %d", err);
		return err;
	}

	if (config->gpio_reset_name) {
		data->gpio_reset_dev =
			device_get_binding(config->gpio_reset_name);
		if (!data->gpio_reset_dev) {
			LOG_ERR("reset GPIO device '%s' not found",
				config->gpio_reset_name);
			return -EINVAL;
		}

		err = gpio_pin_configure(data->gpio_reset_dev,
					 config->gpio_reset_pin,
					 GPIO_OUTPUT_INACTIVE |
					 config->gpio_reset_flags);
		if (err) {
			LOG_ERR("failed to configure reset GPIO (err %d", err);
			return err;
		}
	}

	if (config->gpio_enable_name) {
		data->gpio_enable_dev =
			device_get_binding(config->gpio_enable_name);
		if (!data->gpio_enable_dev) {
			LOG_ERR("enable GPIO device '%s' not found",
				config->gpio_enable_name);
			return -EINVAL;
		}

		err = gpio_pin_configure(data->gpio_enable_dev,
					 config->gpio_enable_pin,
					 GPIO_OUTPUT_INACTIVE |
					 config->gpio_enable_flags);
		if (err) {
			LOG_ERR("failed to configure enable GPIO (err %d", err);
			return err;
		}
	}

	/* TODO: optional VCCEN GPIO (or regulator?) */

	/* TODO: segment-offset */
	/* TODO: page-offset */
	/* TODO: display-offset */
	/* TODO: multiplex-ratio */
	/* TODO: optional segment-remap */
	/* TODO: optional com-invdir */
	/* TODO: optional com-sequential */
	/* TODO: prechargep */

	err = ssd1331_power_on(dev);
	if (err) {
		LOG_ERR("failed to power on ssd1331 (err %d)", err);
		return err;
	}

	err = ssd1331_write_cmd(dev, cmds, sizeof(cmds));
	if (err) {
		LOG_ERR("failed to initialize display (err %d)", err);
		return err;
	}

	return 0;
}

static const struct display_driver_api ssd1331_display_api = {
	.blanking_on = ssd1331_blanking_on,
	.blanking_off = ssd1331_blanking_off,
	.write = ssd1331_write,
	.read = ssd1331_read,
	.get_framebuffer = ssd1331_get_framebuffer,
	.set_brightness = ssd1331_set_brightness,
	.set_contrast = ssd1331_set_contrast,
	.get_capabilities = ssd1331_get_capabilities,
	.set_pixel_format = ssd1331_set_pixel_format,
	.set_orientation = ssd1331_set_orientation,
};

#define SSD1331_DEVICE_INIT(n)						\
	static struct ssd1331_data ssd1331_data_##n;			\
									\
	static const struct ssd1331_config ssd1331_config_##n = {	\
		.spi_name = DT_BUS_LABEL(DT_DRV_INST(n)),		\
		.spi_cs_name = UTIL_AND(				\
			DT_SPI_DEV_HAS_CS_GPIOS(DT_DRV_INST(n)),	\
			DT_SPI_DEV_CS_GPIOS_LABEL(DT_DRV_INST(n))	\
		),							\
		.spi_cs_pin = UTIL_AND(					\
			DT_SPI_DEV_HAS_CS_GPIOS(DT_DRV_INST(n)),	\
			DT_SPI_DEV_CS_GPIOS_PIN(DT_DRV_INST(n))		\
		),							\
		.spi_cs_dt_flags = UTIL_AND(				\
			DT_SPI_DEV_HAS_CS_GPIOS(DT_DRV_INST(n)),	\
			DT_SPI_DEV_CS_GPIOS_FLAGS(DT_DRV_INST(n))	\
		),							\
		.spi_cfg = {						\
			.operation =					\
				(SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | \
				SPI_TRANSFER_MSB),			\
			.frequency = DT_PROP(DT_DRV_INST(n),		\
					spi_max_frequency),		\
			.slave = DT_REG_ADDR(DT_DRV_INST(n)),		\
			.cs = &ssd1331_data_##n.spi_cs,			\
		},							\
		.gpio_data_cmd_name =					\
			DT_INST_GPIO_LABEL(n, data_cmd_gpios),		\
		.gpio_data_cmd_pin =					\
			DT_INST_GPIO_PIN(n, data_cmd_gpios),		\
		.gpio_data_cmd_flags =					\
			DT_INST_GPIO_FLAGS(n, data_cmd_gpios),		\
		.gpio_reset_name = UTIL_AND(				\
			DT_INST_NODE_HAS_PROP(n, reset_gpios),		\
			DT_INST_GPIO_LABEL(n, reset_gpios)),		\
		.gpio_reset_pin = UTIL_AND(				\
			DT_INST_NODE_HAS_PROP(n, reset_gpios),		\
			DT_INST_GPIO_PIN(n, reset_gpios)),		\
		.gpio_reset_flags =					\
			DT_INST_GPIO_FLAGS(n, reset_gpios),		\
		.gpio_enable_name = UTIL_AND(				\
			DT_INST_NODE_HAS_PROP(n, enable_gpios),		\
			DT_INST_GPIO_LABEL(n, enable_gpios)),		\
		.gpio_enable_pin = UTIL_AND(				\
			DT_INST_NODE_HAS_PROP(n, enable_gpios),		\
			DT_INST_GPIO_PIN(n, enable_gpios)),		\
		.gpio_enable_flags =					\
			DT_INST_GPIO_FLAGS(n, enable_gpios),		\
		.width = DT_INST_PROP(n, width),			\
		.height = DT_INST_PROP(n, height),			\
	};								\
									\
	DEVICE_AND_API_INIT(ssd1331_##n, DT_INST_LABEL(n),		\
			    &ssd1331_init, &ssd1331_data_##n,		\
			    &ssd1331_config_##n, POST_KERNEL,		\
			    CONFIG_APPLICATION_INIT_PRIORITY,		\
			    &ssd1331_display_api)

DT_INST_FOREACH_STATUS_OKAY(SSD1331_DEVICE_INIT);
