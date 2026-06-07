# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(canopen_indicators)

target_sources(app PRIVATE src/common.c)
target_sources(app PRIVATE src/test_canopen_indicators_error_run.c)
target_sources_ifdef(CONFIG_CANOPEN_INDICATORS_MULTICOLOR_LED_SUPPORT app PRIVATE src/test_canopen_indicators_multicolor.c)
target_sources_ifdef(CONFIG_CANOPEN_INDICATORS_STATUS_LED_SUPPORT app PRIVATE src/test_canopen_indicators_status.c)
