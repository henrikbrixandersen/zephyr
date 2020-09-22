/*
 * Copyright (c) 2020 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT xlnx_xps_iic_2_00_a

#include <device.h>
#include <drivers/i2c.h>
#include <sys/sys_io.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(xlnx_iic, CONFIG_I2C_LOG_LEVEL);

#include <drivers/i2c/xlnx_axi_iic.h>

/* AXI IIC v2.0 register offsets (See Xilinx PG090 for details) */
#define GIE_OFFSET          0x01cU
#define ISR_OFFSET          0x020U
#define IER_OFFSET          0x028U
#define SOFTR_OFFSET        0x040U
#define CR_OFFSET           0x100U
#define SR_OFFSET           0x104U
#define TX_FIFO_OFFSET      0x108U
#define RX_FIFO_OFFSET      0x10cU
#define ADR_OFFSET          0x110U
#define TX_FIFO_OCY_OFFSET  0x114U
#define RX_FIFO_OCY_OFFSET  0x118U
#define TEN_ADR_OFFSET      0x11cU
#define RX_FIFO_PIRQ_OFFSET 0x120U
#define GPO_OFFSET          0x124U
#define TSUSTA_OFFSET       0x128U
#define TSUSTO_OFFSET       0x12cU
#define THDSTA_OFFSET       0x130U
#define TSUDAT_OFFSET       0x134U
#define TBUF_OFFSET         0x138U
#define THIGH_OFFSET        0x13cU
#define TLOW_OFFSET         0x140U
#define THDDAT_OFFSET       0x144U

/* GIE bit definitions */
#define GIE_GIE BIT(31)

/* SOFTR bit definitions */
#define SOFTR_RKEY 0xaU

/* CR bit definitions */
#define CR_EN            BIT(0)
#define CR_TX_FIFO_RESET BIT(1)
#define CR_MSMS          BIT(2)
#define CR_TX            BIT(3)
#define CR_TXAK          BIT(4)
#define CR_RSTA          BIT(5)
#define CR_GC_EN         BIT(6)

/* TX_FIFO bit definitions */
#define TX_FIFO_READ      BIT(0)
#define TX_FIFO_START     BIT(8)
#define TX_FIFO_STOP      BIT(9)

/* RX_FIFO_PIRQ bit definitions */
#define RX_FIFO_PIRQ_MASK BIT_MASK(4)

/* Maximum number of RX bytes */
#define MAX_RX_BYTES BIT_MASK(8)

struct xlnx_axi_iic_config {
	mm_reg_t base;
	void (*config_func)(const struct device *dev);
};

struct xlnx_axi_iic_data {

};

static inline uint32_t xlnx_axi_iic_read32(const struct device *dev,
					   mm_reg_t offset)
{
	const struct xlnx_axi_iic_config *config = dev->config;

	return sys_read32(config->base + offset);
}

static inline void xlnx_axi_iic_write32(const struct device *dev,
					uint32_t value,
					mm_reg_t offset)
{
	const struct xlnx_axi_iic_config *config = dev->config;

	sys_write32(value, config->base + offset);
}

static inline void xlnx_axi_iic_write16(const struct device *dev,
					uint16_t value,
					mm_reg_t offset)
{
	const struct xlnx_axi_iic_config *config = dev->config;

	sys_write16(value, config->base + offset);
}

static int xlnx_axi_iic_configure(const struct device *dev, uint32_t dev_config)
{
	ARG_UNUSED(dev);

	/* TODO: support slave mode? */
	if ((dev_config & I2C_MODE_MASTER) == 0U) {
		return -ENOTSUP;
	}

	/* TODO: support 10 bit addressing? */
	if (dev_config & I2C_ADDR_10_BITS) {
		return -ENOTSUP;
	}

	return 0;
}

static int xlnx_axi_iic_transfer(const struct device *dev, struct i2c_msg *msgs,
			         uint8_t num_msgs, uint16_t addr)
{
	struct i2c_msg *msg = msgs;
	uint16_t val = TX_FIFO_START;

	/* TODO */
	i2c_dump_msgs("xlnx_axi_iic", msgs, num_msgs, addr);

	/* TODO: check bus not busy */
	/* TODO: reset FIFOs */

	while (num_msgs--) {
		val |= addr;

		if (msg->flags & I2C_MSG_ADDR_10_BITS) {
			/* TODO */
		}

		if (msg->flags & I2C_MSG_RESTART) {
			val |= TX_FIFO_START;
		}

		if ((msg->flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) {
			/* TODO */
			/* if (msg->flags & I2C_MSG_STOP) { */
			/* 	val |= TX_FIFO_STOP; */
			/* } */
			xlnx_axi_iic_write16(dev, val, TX_FIFO_OFFSET);
		} else {
			/* TODO: check for msg->len > 256 */
			/* Setup read address */
			val |= TX_FIFO_READ;
			xlnx_axi_iic_write16(dev, val, TX_FIFO_OFFSET);

			/* Setup read length */
			val = msg->len;
			if (msg->flags & I2C_MSG_STOP) {
				val |= TX_FIFO_STOP;
			}
			xlnx_axi_iic_write16(dev, val, TX_FIFO_OFFSET);
		}

		/* TODO: handle IRQs */

		msg++;
		val = 0;
	}

	return 0;
}

static void xlnx_axi_iic_isr(const struct device *dev)
{

}

uint32_t xlnx_axi_iic_read_gpo(const struct device *dev)
{
	__ASSERT_NO_MSG(dev != NULL);

	return xlnx_axi_iic_read32(dev, GPO_OFFSET) & BIT_MASK(8);
}

void xlnx_axi_iic_write_gpo(const struct device *dev, uint32_t value)
{
	__ASSERT_NO_MSG(dev != NULL);

	xlnx_axi_iic_write32(dev, value & BIT_MASK(8), GPO_OFFSET);
}

static int xlnx_axi_iic_init(const struct device *dev)
{
	const struct xlnx_axi_iic_config *config = dev->config;

	/* Reset and configure */
	xlnx_axi_iic_write32(dev, SOFTR_RKEY, SOFTR_OFFSET);
	config->config_func(dev);

	/* Dump calculated/overwritten timing values to aid in debugging */
	LOG_DBG("tsusta = %5d", xlnx_axi_iic_read32(dev, TSUSTA_OFFSET));
	LOG_DBG("tsusto = %5d", xlnx_axi_iic_read32(dev, TSUSTO_OFFSET));
	LOG_DBG("thdsta = %5d", xlnx_axi_iic_read32(dev, THDSTA_OFFSET));
	LOG_DBG("tsudat = %5d", xlnx_axi_iic_read32(dev, TSUDAT_OFFSET));
	LOG_DBG("tbuf   = %5d", xlnx_axi_iic_read32(dev, TBUF_OFFSET));
	LOG_DBG("thigh  = %5d", xlnx_axi_iic_read32(dev, THIGH_OFFSET));
	LOG_DBG("tlow   = %5d", xlnx_axi_iic_read32(dev, TLOW_OFFSET));
	LOG_DBG("thddat = %5d", xlnx_axi_iic_read32(dev, THDDAT_OFFSET));

	/* Set the RX_FIFO depth to maximum */
	xlnx_axi_iic_write32(dev, RX_FIFO_PIRQ_MASK, RX_FIFO_PIRQ_OFFSET);

	/* Reset TX FIFO */
	xlnx_axi_iic_write32(dev, CR_TX_FIFO_RESET, CR_OFFSET);

	/* Global interrupt enable */
	xlnx_axi_iic_write32(dev, GIE_GIE, GIE_OFFSET);

	/* Enable controller */
	xlnx_axi_iic_write32(dev, CR_EN, CR_OFFSET);

	return 0;
}

static const struct i2c_driver_api xlnx_axi_iic_driver_api = {
	.configure = xlnx_axi_iic_configure,
	.transfer = xlnx_axi_iic_transfer,
};

#define XLNX_AXI_IIC_SET_TIMING_PARAM(n, param, offset)			\
	IF_ENABLED(DT_INST_NODE_HAS_PROP(n, xlnx_##param),		\
		xlnx_axi_iic_write32(dev, DT_INST_PROP(n, xlnx_##param),\
				     offset))

#define XLNX_AXI_IIC_INIT(n)						\
	static void xlnx_axi_iic_config_func_##n(const struct device *dev); \
									\
	static const struct xlnx_axi_iic_config xlnx_axi_iic_config_##n = { \
		.base = DT_INST_REG_ADDR(n),				\
		.config_func = xlnx_axi_iic_config_func_##n,		\
	};								\
									\
	static struct xlnx_axi_iic_data xlnx_axi_iic_data_##n;		\
									\
	DEVICE_AND_API_INIT(xlnx_axi_iic_##n, DT_INST_LABEL(n),		\
			    &xlnx_axi_iic_init, &xlnx_axi_iic_data_##n,	\
			    &xlnx_axi_iic_config_##n, POST_KERNEL,	\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,		\
			    &xlnx_axi_iic_driver_api);			\
									\
	static void xlnx_axi_iic_config_func_##n(const struct device *dev) \
	{								\
		XLNX_AXI_IIC_SET_TIMING_PARAM(n, tsusta, TSUSTA_OFFSET);\
		XLNX_AXI_IIC_SET_TIMING_PARAM(n, tsusto, TSUSTO_OFFSET);\
		XLNX_AXI_IIC_SET_TIMING_PARAM(n, thdsta, THDSTA_OFFSET);\
		XLNX_AXI_IIC_SET_TIMING_PARAM(n, tsudat, TSUDAT_OFFSET);\
		XLNX_AXI_IIC_SET_TIMING_PARAM(n, tbuf, TBUF_OFFSET);	\
		XLNX_AXI_IIC_SET_TIMING_PARAM(n, thigh, THIGH_OFFSET);	\
		XLNX_AXI_IIC_SET_TIMING_PARAM(n, tlow, TLOW_OFFSET);	\
		XLNX_AXI_IIC_SET_TIMING_PARAM(n, thddat, THDDAT_OFFSET);\
									\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority),	\
			    xlnx_axi_iic_isr,				\
			    DEVICE_GET(xlnx_axi_iic_##n), 0);		\
		irq_enable(DT_INST_IRQN(n));				\
	}

DT_INST_FOREACH_STATUS_OKAY(XLNX_AXI_IIC_INIT)
