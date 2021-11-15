# SPDX-License-Identifier: Apache-2.0

include(${ZEPHYR_BASE}/cmake/canopen.cmake NO_POLICY_SCOPE)

# Generate object dictionary headers and implementation files
set(od test-cia301)
set(od_header ${ZEPHYR_BINARY_DIR}/include/generated/${od}.h)
set(od_impl ${ZEPHYR_BINARY_DIR}/misc/generated/${od}.c)
generate_canopen_objdict_for_target(
  app ${ZEPHYR_BASE}/tests/subsys/canbus/canopen/common/objdict/${od}.eds
  ${od_header}
  ${od_impl}
  "--prefix=test_cia301"
)

target_sources(app PRIVATE ${od_impl})
