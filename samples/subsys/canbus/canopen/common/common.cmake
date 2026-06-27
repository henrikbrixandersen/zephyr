# SPDX-FileCopyrightText: Copyright (c) 2026 Henrik Brix Andersen <henrik@brixandersen.dk>
#
# SPDX-License-Identifier: Apache-2.0

set(SAMPLE_CANOPEN_DIR ${ZEPHYR_BASE}/samples/subsys/canbus/canopen/common)

target_include_directories(app PRIVATE ${SAMPLE_CANOPEN_DIR})
target_sources_ifdef(CONFIG_CANOPEN app PRIVATE ${SAMPLE_CANOPEN_DIR}/sample_canopen_leds.c)
