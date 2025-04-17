/*
 * Copyright (c) 2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/syscon.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util_macro.h>

#include <soc.h>

LOG_MODULE_REGISTER(wdt_neorv32, CONFIG_WDT_LOG_LEVEL);

#define DT_DRV_COMPAT neorv32_wdt

/* Control register */
#define NEORV32_WDT_CTRL         0x0
#define NEORV32_WDT_CTRL_EN      BIT(0)
#define NEORV32_WDT_CTRL_LOCK    BIT(1)
#define NEORV32_WDT_CTRL_STRICT  BIT(2)
#define NEORV32_WDT_CTRL_RCAUSE  GENMASK(4, 3)
#define NEORV32_WDT_CTRL_TIMEOUT GENMASK(31, 8)

/* Reset register */
#define NEORV32_WDT_RESET          0x4
#define NEORV32_WDT_RESET_PASSWORD 0x709d1ab3

/* Fixed watchdog clock prescaler */
#define NEORV32_WDT_PRESCALER 4096

struct neorv32_wdt_config {
	const struct device *syscon;
	mm_reg_t base;
	bool lock;
	bool strict;
};

struct neorv32_wdt_data {
	struct k_spinlock lock;
	bool timeout_installed;
};

static int neorv32_wdt_setup(const struct device *dev, uint8_t options)
{
	const struct neorv32_wdt_config *config = dev->config;
	struct neorv32_wdt_data *data = dev->data;
	k_spinlock_key_t key;
	uint32_t ctrl;
	int err = 0;

	if (!data->timeout_installed) {
		LOG_ERR("no timeout installed");
		return -EINVAL;
	}

	if (options != 0U) {
		LOG_ERR("unsupported options 0x%02x", options);
		return -ENOTSUP;
	}

	key = k_spin_lock(&data->lock);

	ctrl = sys_read32(config->base + NEORV32_WDT_CTRL);

	if ((ctrl & NEORV32_WDT_CTRL_EN) != 0U) {
		LOG_WRN("watchdog already enabled");
		err = -EBUSY;
		goto unlock;
	}

	ctrl |= NEORV32_WDT_CTRL_EN;

	if (config->strict) {
		ctrl |= NEORV32_WDT_CTRL_STRICT;
	}

	sys_write32(ctrl, config->base + NEORV32_WDT_CTRL);

	if (config->lock) {
		/* Lock bit can only be written if watchdog already enabled */
		ctrl |= NEORV32_WDT_CTRL_LOCK;
		sys_write32(ctrl, config->base + NEORV32_WDT_CTRL);
	}

unlock:
	k_spin_unlock(&data->lock, key);

	return err;
}

static int neorv32_wdt_disable(const struct device *dev)
{
	const struct neorv32_wdt_config *config = dev->config;
	struct neorv32_wdt_data *data = dev->data;
	k_spinlock_key_t key;
	uint32_t ctrl;
	int err = 0;

	key = k_spin_lock(&data->lock);

	ctrl = sys_read32(config->base + NEORV32_WDT_CTRL);

	if ((ctrl & NEORV32_WDT_CTRL_EN) == 0U) {
		LOG_WRN("watchdog not enabled");
		err = -EFAULT;
		goto unlock;
	}

	if ((ctrl & NEORV32_WDT_CTRL_LOCK) != 0U) {
		LOG_WRN("watchdog locked");
		err = -EPERM;
		goto unlock;
	}

	ctrl &= ~(NEORV32_WDT_CTRL_EN);
	sys_write32(ctrl, config->base + NEORV32_WDT_CTRL);

	data->timeout_installed = false;

unlock:
	k_spin_unlock(&data->lock, key);

	return err;
}

static int neorv32_wdt_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg)
{
	const struct neorv32_wdt_config *config = dev->config;
	struct neorv32_wdt_data *data = dev->data;
	k_spinlock_key_t key;
	uint32_t timeout;
	uint32_t ctrl;
	uint32_t clk;
	int err;

	__ASSERT_NO_MSG(cfg != NULL);

	if (data->timeout_installed) {
		LOG_ERR("timeout already installed");
		return -ENOMEM;
	}

	if (cfg->window.min != 0U) {
		LOG_ERR("window timeouts not supported");
		return -EINVAL;
	}

	if (cfg->callback != NULL) {
		LOG_ERR("callbacks not supported");
		return -ENOTSUP;
	}

	if (cfg->flags != WDT_FLAG_RESET_SOC) {
		LOG_ERR("unsupported flags 0x%02x", cfg->flags);
		return -ENOTSUP;
	}

	err = syscon_read_reg(config->syscon, NEORV32_SYSINFO_CLK, &clk);
	if (err < 0) {
		LOG_ERR("failed to determine clock rate (err %d)", err);
		return -EIO;
	}

	/* TODO: calculate timeout with proper rounding */
	timeout = clk * cfg->window.max / 1000U / NEORV32_WDT_PRESCALER;

	key = k_spin_lock(&data->lock);

	ctrl = sys_read32(config->base + NEORV32_WDT_CTRL);
	ctrl &= ~(NEORV32_WDT_CTRL_TIMEOUT);
	ctrl |= FIELD_PREP(NEORV32_WDT_CTRL_TIMEOUT, timeout);

	sys_write32(ctrl, config->base + NEORV32_WDT_CTRL);

	data->timeout_installed = true;

	k_spin_unlock(&data->lock, key);

	return 0;
}

static int neorv32_wdt_feed(const struct device *dev, int channel_id)
{
	const struct neorv32_wdt_config *config = dev->config;
	struct neorv32_wdt_data *data = dev->data;

	if (channel_id != 0) {
		LOG_ERR("invalid channel id %d", channel_id);
		return -EINVAL;
	}

	if (!data->timeout_installed) {
		LOG_ERR("no timeout installed");
		return -EINVAL;
	}

	sys_write32(NEORV32_WDT_RESET_PASSWORD, config->base + NEORV32_WDT_RESET);

	return 0;
}

static int neorv32_wdt_init(const struct device *dev)
{
	const struct neorv32_wdt_config *config = dev->config;
	uint32_t features;
	int err;

	if (!device_is_ready(config->syscon)) {
		LOG_ERR("syscon device not ready");
		return -EINVAL;
	}

	err = syscon_read_reg(config->syscon, NEORV32_SYSINFO_SOC, &features);
	if (err < 0) {
		LOG_ERR("failed to determine implemented features (err %d)", err);
		return -EIO;
	}

	if ((features & NEORV32_SYSINFO_SOC_IO_WDT) == 0U) {
		LOG_ERR("neorv32 wdt not supported");
		return -ENODEV;
	}

	return 0;
}

static DEVICE_API(wdt, neorv32_wdt_driver_api) = {
	.setup = neorv32_wdt_setup,
	.disable = neorv32_wdt_disable,
	.install_timeout = neorv32_wdt_install_timeout,
	.feed = neorv32_wdt_feed,
};

#define NEORV32_WDT_INIT(n)                                                                        \
	static struct neorv32_wdt_data neorv32_wdt_##n##_data;                                     \
                                                                                                   \
	static const struct neorv32_wdt_config neorv32_wdt_##n##_config = {                        \
		.syscon = DEVICE_DT_GET(DT_INST_PHANDLE(n, syscon)),                               \
		.base = DT_INST_REG_ADDR(n),                                                       \
		.lock = DT_INST_PROP(n, lock),                                                     \
		.strict = DT_INST_PROP(n, strict),                                                 \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, neorv32_wdt_init, NULL, &neorv32_wdt_##n##_data,                  \
			      &neorv32_wdt_##n##_config, POST_KERNEL,                              \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &neorv32_wdt_driver_api);

DT_INST_FOREACH_STATUS_OKAY(NEORV32_WDT_INIT)
