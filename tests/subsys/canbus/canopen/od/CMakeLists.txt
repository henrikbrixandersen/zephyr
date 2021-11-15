# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(canopen_od)

include(${ZEPHYR_BASE}/tests/subsys/canbus/canopen/common/test-cia301.cmake)
FILE(GLOB app_sources src/test_*.c)
target_sources(app PRIVATE ${app_sources})
