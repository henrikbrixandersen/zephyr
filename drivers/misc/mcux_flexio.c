/*
 * Copyright (c) 2021 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_kinetis_flexio

#include <device.h>
#include <shared_irq.h>
#include <logging/log.h>

#include <fsl_flexio.h>

LOG_MODULE_REGISTER(mcux_flexio, CONFIG_LOG_DEFAULT_LEVEL);

struct mcux_flexio_config {
	FLEXIO_Type *base;
	struct mcux_flexio_child *children;
	size_t num_children;
	void (*irq_config_func)(const struct device *dev);
};

struct mcux_flexio_child {
	const struct device *dev;
	bool disabled;
	isr_t isr;
};

struct mcux_flexio_data {
	struct k_mutex lock;
};

static int mcux_flexio_shared_irq_register(const struct device *dev,
					   isr_t isr_func,
					   const struct device *isr_dev)
{
	const struct mcux_flexio_config *config = dev->config;
	struct mcux_flexio_data *data = dev->data;
	struct mcux_flexio_child *child;
	int err = -ENOMEM;
	size_t i;

	if (isr_dev == NULL) {
		LOG_ERR("cannot register IRQ for NULL child device");
		return -EINVAL;
	}

	if (isr_func == NULL) {
		LOG_ERR("cannot register IRQ for NULL isr function");
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	for (i = 0; i < config->num_children; i++) {
		child = &config->children[i];

		if (child->dev == isr_dev) {
			LOG_ERR("child device %p already registered", isr_dev);
			err = -EINVAL;
			break;
		}

		if (child->dev == NULL) {
			child->dev = isr_dev;
			child->isr = isr_func;
			err = 0;
			break;
		}
	}

	k_mutex_unlock(&data->lock);

	return err;
}

static int mcux_flexio_shared_irq_set_disabled(const struct device *dev,
					       const struct device *isr_dev,
					       bool value)
{
	const struct mcux_flexio_config *config = dev->config;
	struct mcux_flexio_child *child;
	size_t i;

	if (isr_dev == NULL) {
		LOG_ERR("cannot %s IRQ for NULL child device",
			value ? "disable" : "enable");
		return -EINVAL;
	}

	for (i = 0; i < config->num_children; i++) {
		child = &config->children[i];

		if (child->dev == isr_dev) {
			child->disabled = value;
			return 0;
		}
	}

	return -EINVAL;
}

static int mcux_flexio_shared_irq_enable(const struct device *dev,
					 const struct device *isr_dev)
{
	return mcux_flexio_shared_irq_set_disabled(dev, isr_dev, false);
}

static int mcux_flexio_shared_irq_disable(const struct device *dev,
					  const struct device *isr_dev)
{
	return mcux_flexio_shared_irq_set_disabled(dev, isr_dev, true);
}

static void mcux_flexio_isr(const struct device *dev)
{
	const struct mcux_flexio_config *config = dev->config;
	struct mcux_flexio_child *child;
	const struct device *isr_dev;
	isr_t isr_func;
	size_t i;

	for (i = 0; i < config->num_children; i++) {
		child = &config->children[i];

		if (!child->disabled) {
			isr_func = child->isr;
			isr_dev = child->dev;

			if (isr_func != NULL && isr_dev != NULL) {
				isr_func(isr_dev);
			}
		}
	}
}

static int mcux_flexio_init(const struct device *dev)
{
	const struct mcux_flexio_config *config = dev->config;
	struct mcux_flexio_data *data = dev->data;
	flexio_config_t flexio_config;

	k_mutex_init(&data->lock);

	FLEXIO_GetDefaultConfig(&flexio_config);
	FLEXIO_Init(config->base, &flexio_config);
	config->irq_config_func(dev);

	return 0;
}

static struct shared_irq_driver_api mcux_flexio_driver_api = {
	.isr_register = mcux_flexio_shared_irq_register,
	.enable = mcux_flexio_shared_irq_enable,
	.disable = mcux_flexio_shared_irq_disable,
};

#define MCUX_FLEXIO_CHILD_INIT(child_node_id)		\
	{						\
	},

#define MCUX_FLEXIO_INIT(n)						\
	static void mcux_flexio_config_func_##n(const struct device *dev); \
									\
	static struct mcux_flexio_child mcux_flexio_children_##n[] = {	\
		DT_INST_FOREACH_CHILD(n, MCUX_FLEXIO_CHILD_INIT)	\
	};								\
									\
	static const struct mcux_flexio_config mcux_flexio_config_##n = { \
		.base = (FLEXIO_Type *)DT_INST_REG_ADDR(n),		\
		.children = mcux_flexio_children_##n,			\
		.num_children = ARRAY_SIZE(mcux_flexio_children_##n),	\
		.irq_config_func = mcux_flexio_config_func_##n,		\
	};								\
									\
	static struct mcux_flexio_data mcux_flexio_data_##n;		\
									\
	DEVICE_DT_INST_DEFINE(n, &mcux_flexio_init,			\
			      NULL,					\
			      &mcux_flexio_data_##n,			\
			      &mcux_flexio_config_##n,			\
			      POST_KERNEL,				\
			      CONFIG_MCUX_FLEXIO_INIT_PRIORITY,		\
			      &mcux_flexio_driver_api);			\
									\
	static void mcux_flexio_config_func_##n(const struct device *dev) \
	{								\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority),	\
			    mcux_flexio_isr, DEVICE_DT_INST_GET(n), 0);	\
		irq_enable(DT_INST_IRQN(n));				\
	}

DT_INST_FOREACH_STATUS_OKAY(MCUX_FLEXIO_INIT)
