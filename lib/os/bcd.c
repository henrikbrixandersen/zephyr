/*
 * Copyright (c) 2020 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/util.h>

u8_t bcd2bin(u8_t value)
{
	return (value & 0xf) + (value >> 4) * 10;
}

u8_t bin2bcd(u8_t value)
{
	return ((value / 10) << 4) + value % 10;
}
