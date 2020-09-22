/*
 * Copyright (c) 2020 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_I2C_XLNX_AXI_IIC_H_
#define ZEPHYR_INCLUDE_DRIVERS_I2C_XLNX_AXI_IIC_H_

#include <device.h>

/**
 * @brief Read the GPO register.
 *
 * Read the value of the Xilinx AXI IIC controller General Purpose Output
 * Register (GPO).
 *
 * @param dev Pointer to the device structure for the driver instance.
 *
 * @return The value of the GPO register.
 */
uint32_t xlnx_axi_iic_read_gpo(const struct device *dev);

/**
 * @brief Write the GPO register.
 *
 * Write a value to the Xilinx AXI IIC controller General Purpose Output
 * Register (GPO).
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param value The value to write to the GPO register.
 */
void xlnx_axi_iic_write_gpo(const struct device *dev, uint32_t value);

#endif /* ZEPHYR_INCLUDE_DRIVERS_I2C_XLNX_AXI_IIC_H_ */
