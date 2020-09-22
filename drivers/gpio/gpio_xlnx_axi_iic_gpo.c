/*
 * Copyright (c) 2020 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT xlnx_xps_iic_2_00_a_gpo

#include <device.h>
#include <drivers/gpio.h>
#include <drivers/i2c/xlnx_axi_iic.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(xlnx_aci_iic_gpo, CONFIG_GPIO_LOG_LEVEL);

#include "gpio_utils.h"

/* Maximum number of GPIOs supported */
#define MAX_GPIOS 8

struct gpio_xlnx_axi_iic_gpo_config {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;
	const char *xlnx_axi_iic_name;
};

struct gpio_xlnx_axi_iic_gpo_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data common;
	const struct device *xlnx_axi_iic;
};

static int gpio_xlnx_axi_iic_gpo_pin_config(const struct device *dev,
					    gpio_pin_t pin,
					    gpio_flags_t flags)
{
	const struct gpio_xlnx_axi_iic_gpo_config *config = dev->config;
	struct gpio_xlnx_axi_iic_gpo_data *data = dev->data;
	unsigned int key;
	uint32_t gpo;

	if (!(BIT(pin) & config->common.port_pin_mask)) {
		return -EINVAL;
	}

	if ((flags & GPIO_INPUT) != 0) {
		return -ENOTSUP;
	}

	if ((flags & GPIO_SINGLE_ENDED) != 0) {
		return -ENOTSUP;
	}

	if ((flags & (GPIO_PULL_UP | GPIO_PULL_DOWN)) != 0) {
		return -ENOTSUP;
	}

	if (flags & (GPIO_OUTPUT_INIT_HIGH | GPIO_OUTPUT_INIT_LOW)) {
		key = irq_lock();
		gpo = xlnx_axi_iic_read_gpo(data->xlnx_axi_iic);

		if ((flags & GPIO_OUTPUT_INIT_HIGH) != 0) {
			gpo |= BIT(pin);
		} else if ((flags & GPIO_OUTPUT_INIT_LOW) != 0) {
			gpo &= ~BIT(pin);
		}

		xlnx_axi_iic_write_gpo(data->xlnx_axi_iic, gpo);
		irq_unlock(key);
	}

	return 0;
}

static int gpio_xlnx_axi_iic_gpo_port_get_raw(const struct device *dev,
				      gpio_port_value_t *value)
{
	struct gpio_xlnx_axi_iic_gpo_data *data = dev->data;

	*value = (gpio_port_value_t)xlnx_axi_iic_read_gpo(data->xlnx_axi_iic);
	return 0;
}

static int gpio_xlnx_axi_iic_gpo_port_set_masked_raw(const struct device *dev,
						     gpio_port_pins_t mask,
						     gpio_port_value_t value)
{
	struct gpio_xlnx_axi_iic_gpo_data *data = dev->data;
	unsigned int key;
	uint32_t gpo;

	key = irq_lock();
	gpo = xlnx_axi_iic_read_gpo(data->xlnx_axi_iic);
	gpo = (gpo & ~mask) | (mask & value);
	xlnx_axi_iic_write_gpo(data->xlnx_axi_iic, gpo);
	irq_unlock(key);

	return 0;
}

static int gpio_xlnx_axi_iic_gpo_port_set_bits_raw(const struct device *dev,
						   gpio_port_pins_t pins)
{
	struct gpio_xlnx_axi_iic_gpo_data *data = dev->data;
	unsigned int key;
	uint32_t gpo;

	key = irq_lock();
	gpo = xlnx_axi_iic_read_gpo(data->xlnx_axi_iic);
	gpo |= pins;
	xlnx_axi_iic_write_gpo(data->xlnx_axi_iic, gpo);
	irq_unlock(key);

	return 0;
}

static int gpio_xlnx_axi_iic_gpo_port_clear_bits_raw(const struct device *dev,
						     gpio_port_pins_t pins)
{
	struct gpio_xlnx_axi_iic_gpo_data *data = dev->data;
	unsigned int key;
	uint32_t gpo;

	key = irq_lock();
	gpo = xlnx_axi_iic_read_gpo(data->xlnx_axi_iic);
	gpo &= ~pins;
	xlnx_axi_iic_write_gpo(data->xlnx_axi_iic, gpo);
	irq_unlock(key);

	return 0;
}

static int gpio_xlnx_axi_iic_gpo_port_toggle_bits(const struct device *dev,
						  gpio_port_pins_t pins)
{
	struct gpio_xlnx_axi_iic_gpo_data *data = dev->data;
	unsigned int key;
	uint32_t gpo;

	key = irq_lock();
	gpo = xlnx_axi_iic_read_gpo(data->xlnx_axi_iic);
	gpo ^= pins;
	xlnx_axi_iic_write_gpo(data->xlnx_axi_iic, gpo);
	irq_unlock(key);

	return 0;
}

static int gpio_xlnx_axi_iic_gpo_pin_interrupt_config(const struct device *dev,
						      gpio_pin_t pin,
						      enum gpio_int_mode mode,
						      enum gpio_int_trig trig)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(pin);
	ARG_UNUSED(mode);
	ARG_UNUSED(trig);

	return -ENOTSUP;
}

static int gpio_xlnx_axi_iic_gpo_manage_callback(const struct device *dev,
						 struct gpio_callback *cb,
						 bool set)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(set);

	return -ENOTSUP;
}

static uint32_t gpio_xlnx_axi_iic_gpo_get_pending_int(const struct device *dev)
{
	return 0;
}

static int gpio_xlnx_axi_iic_gpo_init(const struct device *dev)
{
	const struct gpio_xlnx_axi_iic_gpo_config *config = dev->config;
	struct gpio_xlnx_axi_iic_gpo_data *data = dev->data;

	data->xlnx_axi_iic = device_get_binding(config->xlnx_axi_iic_name);
	if (!data->xlnx_axi_iic) {
		LOG_ERR("parent device %s not found",
			config->xlnx_axi_iic_name);
		return -EINVAL;
	}

	return 0;
}

static const struct gpio_driver_api gpio_xlnx_axi_iic_gpo_driver_api = {
	.pin_configure = gpio_xlnx_axi_iic_gpo_pin_config,
	.port_get_raw = gpio_xlnx_axi_iic_gpo_port_get_raw,
	.port_set_masked_raw = gpio_xlnx_axi_iic_gpo_port_set_masked_raw,
	.port_set_bits_raw = gpio_xlnx_axi_iic_gpo_port_set_bits_raw,
	.port_clear_bits_raw = gpio_xlnx_axi_iic_gpo_port_clear_bits_raw,
	.port_toggle_bits = gpio_xlnx_axi_iic_gpo_port_toggle_bits,
	.pin_interrupt_configure = gpio_xlnx_axi_iic_gpo_pin_interrupt_config,
	.manage_callback = gpio_xlnx_axi_iic_gpo_manage_callback,
	.get_pending_int = gpio_xlnx_axi_iic_gpo_get_pending_int,
};

#define GPIO_XLNX_AXI_IIC_GPO_INIT(n)					\
	static struct gpio_xlnx_axi_iic_gpo_data			\
		gpio_xlnx_axi_iic_gpo_##n##_data;			\
									\
	static const struct gpio_xlnx_axi_iic_gpo_config		\
		gpio_xlnx_axi_iic_gpo_##n##_config = {			\
		.common = {						\
			.port_pin_mask = GPIO_PORT_PIN_MASK_FROM_DT_INST(n), \
		},							\
		.xlnx_axi_iic_name =					\
			DT_LABEL(DT_INST_PHANDLE(n, xlnx_axi_iic)),	\
	};								\
									\
	DEVICE_AND_API_INIT(gpio_xlnx_axi_iic_gpo_##n, DT_INST_LABEL(n),\
			&gpio_xlnx_axi_iic_gpo_init,			\
			&gpio_xlnx_axi_iic_gpo_##n##_data,		\
			&gpio_xlnx_axi_iic_gpo_##n##_config,		\
			POST_KERNEL,					\
			CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,		\
			&gpio_xlnx_axi_iic_gpo_driver_api);

/* TODO: init level after xlnx,axi-iic */

DT_INST_FOREACH_STATUS_OKAY(GPIO_XLNX_AXI_IIC_GPO_INIT)
