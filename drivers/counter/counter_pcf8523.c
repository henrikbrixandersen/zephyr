/*
 * Copyright (c) 2019 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <time.h>

#include <drivers/counter.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(pcf8523, CONFIG_COUNTER_LOG_LEVEL);

/* PCF8523 registers */
#define PCF8523_REG_CONTROL_1     0x00
#define PCF8523_REG_CONTROL_2     0x01
#define PCF8523_REG_CONTROL_3     0x02
#define PCF8523_REG_SECONDS       0x03
#define PCF8523_REG_MINUTES       0x04
#define PCF8523_REG_HOURS         0x05
#define PCF8523_REG_DAYS          0x06
#define PCF8523_REG_WEEKDAYS      0x07
#define PCF8523_REG_MONTHS        0x08
#define PCF8523_REG_YEARS         0x09
#define PCF8523_REG_MINUTE_ALARM  0x0a
#define PCF8523_REG_HOUR_ALARM    0x0B
#define PCF8523_REG_DAY_ALARM     0x0C
#define PCF8523_REG_WEEKDAY_ALARM 0x0D
#define PCF8523_REG_OFFSET        0x0E

/* PCF8523 register masks and bit macros */
#define PCF8523_CAP_SEL    BIT(7)
#define PCF8523_T          BIT(6)
#define PCF8523_STOP       BIT(5)
#define PCF8523_SR         BIT(4)
#define PCF8523_12_24      BIT(3)
#define PCF8523_SIE        BIT(2)
#define PCF8523_AIE        BIT(1)
#define PCF8523_CIE        BIT(0)

#define PCF8523_WTAF       BIT(7)
#define PCF8523_CTAF       BIT(6)
#define PCF8523_CTBF       BIT(5)
#define PCF8523_SF         BIT(4)
#define PCF8523_AF         BIT(3)
#define PCF8523_WTAIE      BIT(2)
#define PCF8523_CTAIE      BIT(1)
#define PCF8523_CTBIE      BIT(0)

#define PCF8523_PM(x)      ((x & BIT_MASK(3)) << 5)
#define PCF8523_BSF        BIT(3)
#define PCF8523_BLF        BIT(2)
#define PCF8523_BSIE       BIT(1)
#define PCF8523_BLIE       BIT(0)

#define PCF8523_OS         BIT(7)

#define PCF8523_HAS_INTERRUPT(config) (config->int_gpio_dev_name != NULL)

struct pcf8523_config {
	struct counter_config_info counter_info;
	const char *bus_dev_name;
	u16_t bus_addr;
	const char *int_gpio_dev_name;
	gpio_pin_t int_gpio_pin;
	gpio_dt_flags_t int_gpio_flags;
};

struct pcf8523_data {
	struct device *bus_dev;
	struct k_mutex lock;
	struct gpio_callback int_gpio_cb;
};

static int pcf8523_read_reg(struct device *dev, u8_t addr, void *dptr,
			    size_t len)
{
	const struct pcf8523_config *config = dev->config->config_info;
	struct pcf8523_data *data = dev->driver_data;
	int err;

	err = i2c_write_read(data->bus_dev, config->bus_addr,
			     &addr, sizeof(addr), dptr, len);
	if (err) {
		LOG_ERR("failed to read reg addr 0x%02x (err %d)", addr, err);
		return err;
	}

	return 0;
}

static int pcf8523_read_reg8(struct device *dev, u8_t addr, u8_t *val)
{
	return pcf8523_read_reg(dev, addr, val, sizeof(*val));
}

static int pcf8723_write_reg(struct device *dev, u8_t addr, void *dptr,
			     size_t len)
{
	const struct pcf8523_config *config = dev->config->config_info;
	struct pcf8523_data *data = dev->driver_data;
	u8_t block[sizeof(addr) + len];
	int err;

	block[0] = addr;
	memcpy(&block[1], dptr, len);

	err = i2c_write(data->bus_dev, block, sizeof(addr) + len,
			config->bus_addr);
	if (err) {
		LOG_ERR("failed to write reg addr 0x%02x (err %d)", addr, err);
		return err;
	}

	return 0;
}

static int pcf8523_write_reg8(struct device *dev, u8_t addr, u8_t val)
{
	return pcf8723_write_reg(dev, addr, &val, sizeof(val));
}

static int pcf8523_set_stop_bit(struct device *dev, bool value)
{
	const struct pcf8523_config *config = dev->config->config_info;
	struct pcf8523_data *data = dev->driver_data;
	u8_t ctrl1;
	int err;

	k_mutex_lock(&data->lock, K_FOREVER);

	err = pcf8523_read_reg8(dev, PCF8523_REG_CONTROL_1, &ctrl1);
	if (err) {
		LOG_ERR("failed to read ctrl1 (err %d)", err);
		k_mutex_unlock(&data->lock);
		return err;
	}

	/* TODO: use WRITE_BIT()? */
	if (value) {
		ctrl1 |= PCF8523_STOP;
	} else {
		ctrl1 &= ~PCF8523_STOP;
	}

	err = pcf8523_write_reg8(dev, PCF8523_REG_CONTROL_1, ctrl1);
	if (err) {
		LOG_ERR("failed to write ctrl1 (err %d)", err);
		k_mutex_unlock(&data->lock);
		return err;
	}

	k_mutex_unlock(&data->lock);

	return 0;
}

static int pcf8523_start(struct device *dev)
{
	return pcf8523_set_stop_bit(dev, false);
}

static int pcf8523_stop(struct device *dev)
{
	return pcf8523_set_stop_bit(dev, true);
}

static int pcf8523_get_value(struct device *dev, u32_t *ticks)
{
	const struct pcf8523_config *config = dev->config->config_info;
	struct pcf8523_data *data = dev->driver_data;
	u8_t buffer[7];

	/* TODO */
	pcf8523_read_reg(dev, PCF8523_REG_SECONDS, &buffer, sizeof(buffer));
	LOG_HEXDUMP_ERR(buffer, sizeof(buffer), "buffer");
	*ticks = 0;

	return 0;
}

static int pcf8523_set_alarm(struct device *dev, u8_t chan_id,
			     const struct counter_alarm_cfg *alarm_cfg)
{
	const struct pcf8523_config *config = dev->config->config_info;

	if (!PCF8523_HAS_INTERRUPT(config)) {
		return -ENOTSUP;
	}

	/* TODO */
}

static int pcf8523_cancel_alarm(struct device *dev, u8_t chan_id)
{
	/* TODO */
}

static u32_t pcf8523_get_pending_int(struct device *dev)
{
	/* TODO */
}

static int pcf8523_set_top_value(struct device *dev,
				 const struct counter_top_cfg *cfg)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cfg);

	return -ENOTSUP;
}

static u32_t pcf8523_get_top_value(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;

	return info->max_top_value;
}

static u32_t pcf8523_get_max_relative_alarm(struct device *dev)
{
	/* TODO */
}

static void pcf8523_int_callback(struct device *port,
				 struct gpio_callback *cb,
				 gpio_port_pins_t pins)
{
	struct pcf8523_data *data = CONTAINER_OF(cb, struct pcf8523_data,
						 int_gpio_cb);

	/* TODO: schedule read */
}

static int pcf8523_init(struct device *dev)
{
	const struct pcf8523_config *config = dev->config->config_info;
	struct pcf8523_data *data = dev->driver_data;
	struct device *gpio_dev;
	int err;

	k_mutex_init(&data->lock);

	data->bus_dev = device_get_binding(config->bus_dev_name);
	if (!data->bus_dev) {
		LOG_ERR("could not get parent bus device");
		return -EINVAL;
	}

	/* TODO: support cap dts setting? */

	if (PCF8523_HAS_INTERRUPT(config)) {
		gpio_dev = device_get_binding(config->int_gpio_dev_name);
		if (!gpio_dev) {
			LOG_ERR("could not get INT GPIO device");
			return -EINVAL;
		}

		/* TODO: configure device to disable CLKOUT and enable INT1 */

		err = gpio_pin_configure(gpio_dev, config->int_gpio_pin,
					 GPIO_INPUT | config->int_gpio_flags);

		gpio_init_callback(&data->int_gpio_cb, pcf8523_int_callback,
				   BIT(config->int_gpio_pin));

		err = gpio_add_callback(gpio_dev, &data->int_gpio_cb);
		if (err) {
			LOG_ERR("failed to add INT GPIO callback (err %d)",
				err);
			return err;
		}

		err = gpio_pin_interrupt_configure(gpio_dev,
						   config->int_gpio_pin,
						   GPIO_INT_EDGE_TO_ACTIVE);
		if (err) {
			LOG_ERR("failed to configure INT GPIO IRQ");
			return err;
		}
	}

	return 0;
}

static const struct counter_driver_api pcf8523_api = {
	.start = pcf8523_start,
	.stop = pcf8523_stop,
	.get_value = pcf8523_get_value,
	.set_alarm = pcf8523_set_alarm,
	.cancel_alarm = pcf8523_cancel_alarm,
	.get_pending_int = pcf8523_get_pending_int,
	.set_top_value = pcf8523_set_top_value,
	.get_top_value = pcf8523_get_top_value,
	.get_max_relative_alarm = pcf8523_get_max_relative_alarm,
};

#ifdef DT_INST_0_NXP_PCF8523
static const struct pcf8523_config pcf8523_config_0 = {
	.counter_info = {
		/* TODO: specify correct top value */
		.max_top_value = UINT32_MAX,
		.freq = 1,
		.flags = COUNTER_CONFIG_INFO_COUNT_UP,
		.channels = 1,
	},
	.bus_dev_name = DT_INST_0_NXP_PCF8523_BUS_NAME,
	.bus_addr = DT_INST_0_NXP_PCF8523_BASE_ADDRESS,
#ifdef DT_INST_0_NXP_PCF8523_INT_GPIOS_CONTROLLER
	.int_gpio_dev_name = DT_INST_0_NXP_PCF8523_INT_GPIOS_CONTROLLER,
	.int_gpio_pin = DT_INST_0_NXP_PCF8523_INT_GPIOS_PIN,
	.int_gpio_flags = DT_INST_0_NXP_PCF8523_INT_GPIOS_FLAGS,
#endif /* DT_INST_0_NXP_PCF8523_INT_GPIOS_CONTROLLER */
};

static struct pcf8523_data pcf8523_data_0;

DEVICE_AND_API_INIT(pcf8523_0, DT_INST_0_NXP_PCF8523_LABEL,
		    &pcf8523_init, &pcf8523_data_0,
		    &pcf8523_config_0, POST_KERNEL,
		    CONFIG_COUNTER_PCF8523_INIT_PRIORITY,
		    &pcf8523_api);

#endif /* DT_INST_0_NXP_PCF8523 */
