# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(CANopen)

# Generate object dictionary headers and implementation files
include(${ZEPHYR_BASE}/cmake/canopen.cmake NO_POLICY_SCOPE)
set(od objdict)
set(od_header ${ZEPHYR_BINARY_DIR}/include/generated/${od}.h)
set(od_impl ${ZEPHYR_BINARY_DIR}/misc/generated/${od}.c)
generate_canopen_objdict_for_target(
  app objdict/${od}.eds
  ${od_header}
  ${od_impl}
)

target_sources(app PRIVATE src/main.c ${od_impl})
