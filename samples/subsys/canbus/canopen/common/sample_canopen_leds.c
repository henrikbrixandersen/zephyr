/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>

#if DT_HAS_ALIAS(canopen_status_led)
/* Dedicated bicolor CANopen "status" LED */
struct led_dt_spec red_led = LED_DT_SPEC_GET(DT_ALIAS(canopen_status_led));
struct led_dt_spec green_led = LED_DT_SPEC_GET(DT_ALIAS(canopen_status_led));
#elif (DT_HAS_ALIAS(canopen_error_led) && DT_HAS_ALIAS(canopen_run_led))
/* Dedicated red CANopen "error" LED + green CANopen "run" LED */
struct led_dt_spec red_led = LED_DT_SPEC_GET(DT_ALIAS(canopen_error_led));
struct led_dt_spec green_led = LED_DT_SPEC_GET(DT_ALIAS(canopen_run_led));
#elif (DT_NODE_EXISTS(DT_NODELABEL(red_led)) && DT_NODE_EXISTS(DT_NODELABEL(green_led)))
/* Red LED + green LED */
struct led_dt_spec red_led = LED_DT_SPEC_GET(DT_NODELABEL(red_led));
struct led_dt_spec green_led = LED_DT_SPEC_GET(DT_NODELABEL(green_led));
#else
/* No LEDs */
struct led_dt_spec red_led = {0};
struct led_dt_spec green_led = {0};
#endif
