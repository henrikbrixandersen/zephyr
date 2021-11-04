# SPDX-License-Identifier: Apache-2.0

if(CONFIG_CANOPEN_STACK_NATIVE)
  zephyr_library()
  zephyr_library_sources(canopen.c)
  zephyr_library_sources(nmt.c)
  zephyr_library_sources(od.c)
  zephyr_library_sources(sdo.c)

  zephyr_linker_sources_ifdef(CONFIG_CANOPEN_OD_INFO DATA_SECTIONS canopen.ld)
endif()
