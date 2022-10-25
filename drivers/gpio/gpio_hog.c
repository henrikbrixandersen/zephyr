/*
 * Copyright (c) 2022 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
//#include <zephyr/logging/log.h>

// LOG_MODULE_REGISTER(gpio_hog, CONFIG_GPIO_LOG_LEVEL);

struct gpio_hog_spec {
	gpio_pin_t pin;
	gpio_flags_t flags;
};

struct gpio_hog {
	const struct device *port;
	const struct gpio_hog_spec *specs;
	size_t num_specs;
};

/* Expands to non-empty if node_id is a GPIO hog */
#define GPIO_HOG_NODE_IS_HOG(node_id) DT_PROP(node_id, gpio_hog)

/* Expands to non-empty if GPIO controller node_id has GPIO hog children */
#define GPIO_HOG_GPIO_CTLR_HAS_HOGS(node_id)                                                       \
	DT_FOREACH_CHILD_STATUS_OKAY(node_id, GPIO_HOG_NODE_IS_HOG)

// TODO:
// - Iterate all gpio_hog children and test for input/output-low/output-high props
// - Generate array of specs

/* TODO: store extra flags */
/* TODO: iterate all IDX'es using LISTIFY */
#define GPIO_HOG_INIT_GPIO_HOG_SPEC(node_id) \
	DT_FOREACH_PROP_ELEM_SEP(node_id, gpios, GPIO_DT_SPEC_GET_BY_IDX, (,))

/* Called for each GPIO controller child node */
#define GPIO_HOG_INIT_GPIO_CTLR_CHILD(node_id)				\
	COND_CODE_1(GPIO_HOG_NODE_IS_HOG(node_id), (GPIO_HOG_INIT_GPIO_HOG_SPEC(node_id)), ())

/* Called for each GPIO controller dts node */
#define GPIO_HOG_INIT_GPIO_CTLR(node_id)                                                           \
	DT_FOREACH_CHILD_STATUS_OKAY(node_id, GPIO_HOG_INIT_GPIO_CTLR_CHILD)

/* Called for each dts node */
#define GPIO_HOG_INIT(node_id)                                                                     \
	IF_ENABLED(DT_PROP(node_id, gpio_controller), (GPIO_HOG_INIT_GPIO_CTLR(node_id)))

static const struct gpio_hog gpio_hogs[] = {
//	DT_FOREACH_STATUS_OKAY_NODE(GPIO_HOG_INIT)
};

static int gpio_hog_init(const struct device *dev)
{
	const struct gpio_hog *hog;
	const struct gpio_hog_spec *spec;
	int err;
	int i;
	int j;

	ARG_UNUSED(dev);

	for (i = 0; i < ARRAY_SIZE(gpio_hogs); i++) {
		hog = &gpio_hogs[i];

		if (!device_is_ready(hog->port)) {
			/* LOG_ERR("GPIO port %s not ready", hog->port->name); */
			return -ENODEV;
		}

		for (j = 0; j < hog->num_specs; j++) {
			spec = &hog->specs[j];

			err = gpio_pin_configure(hog->port, spec->pin, spec->flags);
			if (err < 0) {
				/* LOG_ERR("failed to configure GPIO hog for port %s pin %u (err
				 * %d)", spec->port->name, spec->pin, err); */
				return err;
			}
		}
	}

	return 0;
}

/* TODO: init level and priority */
SYS_INIT(gpio_hog_init, POST_KERNEL, CONFIG_GPIO_HOG_INIT_PRIORITY);
