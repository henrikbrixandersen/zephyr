/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_SOC_SERIES_SAME51) || defined(CONFIG_SOC_SERIES_SAME54)
    /* SAME51/SAME54 requires CAN message RAM to be placed within the first 64 KB of system RAM */
    /* TODO: nocache? */
    SECTION_PROLOGUE (can_message_ram, CONFIG_SRAM_BASE_ADDRESS + CONFIG_CAN_SAM0_MRAM_LOCATION (NOLOAD),)
    {
        __can_message_ram_start = .;
        KEEP(*(SORT_BY_NAME(".can_message_ram*")))
        . = ALIGN(4);
        __can_message_ram_end = .;
    } GROUP_LINK_IN(RAMABLE_REGION)
#endif
