#!/usr/bin/env python3
#
# Copyright (c) 2018-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
#
# SPDX-License-Identifier: Apache-2.0

"""Generate CANopen object dictionary code from EDS"""

import argparse
import sys
from typing import TextIO, Union

import canopen

# Mapping from CANopen data type to C-type + number of bits
data_types = {
    canopen.objectdictionary.datatypes.BOOLEAN: ("bool", 1),
    canopen.objectdictionary.datatypes.INTEGER8: ("int8_t", 8),
    canopen.objectdictionary.datatypes.INTEGER16: ("int16_t", 16),
    canopen.objectdictionary.datatypes.INTEGER32: ("int32_t", 32),
    canopen.objectdictionary.datatypes.UNSIGNED8: ("uint8_t", 8),
    canopen.objectdictionary.datatypes.UNSIGNED16: ("uint16_t", 16),
    canopen.objectdictionary.datatypes.UNSIGNED32: ("uint32_t", 32),
    canopen.objectdictionary.datatypes.REAL32: ("float", 32),
    canopen.objectdictionary.datatypes.VISIBLE_STRING: ("uint8_t", 8),
    canopen.objectdictionary.datatypes.OCTET_STRING: ("uint8_t", 8),
    canopen.objectdictionary.datatypes.UNICODE_STRING: ("uint16_t", 16),
    canopen.objectdictionary.datatypes.TIME_OF_DAY: ("uint64_t", 48),
    canopen.objectdictionary.datatypes.TIME_DIFFERENCE: ("uint64_t", 48),
    canopen.objectdictionary.datatypes.DOMAIN: ("int", 0), # TODO
    canopen.objectdictionary.datatypes.INTEGER24: ("int32_t", 24),
    canopen.objectdictionary.datatypes.REAL64: ("double", 64),
    canopen.objectdictionary.datatypes.INTEGER40: ("int64_t", 40),
    canopen.objectdictionary.datatypes.INTEGER48: ("int64_t", 48),
    canopen.objectdictionary.datatypes.INTEGER56: ("int64_t", 56),
    canopen.objectdictionary.datatypes.INTEGER64: ("int64_t", 64),
    canopen.objectdictionary.datatypes.UNSIGNED24: ("uint32_t", 24),
    canopen.objectdictionary.datatypes.UNSIGNED40: ("uint64_t", 40),
    canopen.objectdictionary.datatypes.UNSIGNED48: ("uint64_t", 48),
    canopen.objectdictionary.datatypes.UNSIGNED56: ("uint64_t", 56),
    canopen.objectdictionary.datatypes.UNSIGNED64: ("uint64_t", 64),
}

def generate_header(cmd: str, header: TextIO, prefix: str) -> None:
    """Generate CANopen object dictionary header file"""
    guard = f"__{prefix.upper()}_H__"
    header.write("/*\n"
                 " * This file was automatically generated using the following command:\n"
                 f" * {cmd}\n"
                 " *\n"
                 " */\n"
                 "\n"
                 f"#ifndef {guard}\n"
                 f"#define {guard}\n"
                 "\n"
                 "#include <zephyr/canbus/canopen/od.h>\n"
                 "\n"
                 "#ifdef CONFIG_USERSPACE\n"
                 "#include <zephyr/kernel.h>\n"
                 f"extern struct k_mem_partition {prefix}_partition;\n"
                 "#endif /* CONFIG_USERSPACE */\n"
                 "\n"
                 f"CANOPEN_OD_DECLARE({prefix});\n"
                 "\n"
                 f"#endif /* {guard} */\n")

def write_entry(impl: TextIO, comment: bool,
                entry: canopen.objectdictionary.ODVariable) -> None:
    """Write CANopen object dictionary object entry definition"""
    attr = "0U"

    if comment:
        impl.write(f"\t/* {entry.subindex} - {entry.name} */\n")

    match entry.access_type:
        case "ro":
            attr = "CANOPEN_OD_ATTR_ACCESS_RO"
        case "wo":
            attr = "CANOPEN_OD_ATTR_ACCESS_WO"
        case "rw" | "rwr" | "rww":
            attr = "CANOPEN_OD_ATTR_ACCESS_RW"
        case "const":
            attr = "CANOPEN_OD_ATTR_ACCESS_CONST"

    if entry.pdo_mappable:
        if entry.access_type == "rwr":
            attr += " | CANOPEN_OD_ATTR_PDO_MAPPABLE_TPDO"
        elif entry.access_type == "rww":
            attr += " | CANOPEN_OD_ATTR_PDO_MAPPABLE_RPDO"
        else:
            attr += " | CANOPEN_OD_ATTR_PDO_MAPPABLE"

    if entry.relative:
        attr += " | CANOPEN_OD_ATTR_RELATIVE"

    # TODO: fill data, min, max, size

    # TODO: handle non-existing mapping
    ctype = data_types[entry.data_type][0]
    bits = data_types[entry.data_type][1]

    impl.write(f"CANOPEN_OD_ENTRY({entry.subindex}U, 0x{entry.data_type:04x}U, "
               f"{bits}U, NULL, NULL, NULL, sizeof({ctype}),\n"
               f"\t\t\t {attr}),\n")

def write_object_entries(impl: TextIO, obj: Union[canopen.objectdictionary.ODVariable,
                                                  canopen.objectdictionary.ODArray,
                                                  canopen.objectdictionary.ODRecord],
                         prefix: str) -> None:
    """Write CANopen object dictionary object entries"""

    impl.write(f"/* {obj.index:04x}h - {obj.name} */\n"
               f"static const struct canopen_od_entry "
               f"{prefix}_object_{obj.index:04x}_entries[] = {{\n")

    if isinstance(obj, canopen.objectdictionary.ODVariable):
        write_entry(impl, False, obj)
    elif isinstance(obj, canopen.objectdictionary.ODArray):
        for entry in obj.values():
            write_entry(impl, True, entry)
    elif isinstance(obj, canopen.objectdictionary.ODRecord):
        for entry in obj.values():
            write_entry(impl, True, entry)

    impl.write("};\n\n")

def generate_impl(cmd: str, objdict: canopen.ObjectDictionary, impl: TextIO, prefix: str) -> None:
    """Generate CANopen object dictionary implementation file"""
    impl.write("/*\n"
               " * This file was automatically generated using the following command:\n"
               f" * {cmd}\n"
               " *\n"
               " */\n"
               "\n"
               "#include <zephyr/canbus/canopen/od.h>\n"
               "#include <zephyr/types.h>\n"
               "\n"
               "#ifdef CONFIG_USERSPACE\n"
               "#include <zephyr/app_memory/app_memdomain.h>\n"
               f"K_APPMEM_PARTITION_DEFINE({prefix}_partition);\n"
               f"#define APP_BMEM K_APP_BMEM({prefix}_partition)\n"
               f"#define APP_DMEM K_APP_DMEM({prefix}_partition)\n"
               "#else /* CONFIG_USERSPACE */\n"
               "#define APP_BMEM\n"
               "#define APP_DMEM\n"
               "#endif /* !CONFIG_USERSPACE */\n\n")

    for obj in objdict.values():
        write_object_entries(impl, obj, prefix)

    impl.write(f"APP_DMEM static struct canopen_od_object {prefix}_objects[] = {{\n")
    for obj in objdict.values():
        impl.write(f"\t/* {obj.index:04x}h - {obj.name} */\n"
                   f"\tCANOPEN_OD_OBJECT(0x{obj.index:04x}U,\n"
                   f"\t\t\t  ARRAY_SIZE({prefix}_object_{obj.index:04x}_entries),\n"
                   f"\t\t\t  {prefix}_object_{obj.index:04x}_entries),\n")
    impl.write("};\n\n")

    impl.write(f"APP_DMEM CANOPEN_OD_DEFINE({prefix}, ARRAY_SIZE({prefix}_objects), {prefix}_objects);\n")

def parse_args():
    """Parse arguments"""
    parser = argparse.ArgumentParser(
        description="CANopen object dictionary code generator",
        formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument(
        "-z", "--zephyr-base",
        help="Zephyr base directory")

    group = parser.add_argument_group("input arguments")
    group.add_argument(
        "-i", "--input", required=True, type=argparse.FileType('r'), metavar="FILE",
        help="CANopen object dictionary input file (EDS)")
    group.add_argument(
        "-d", "--deftype", action='append', metavar="DEFTYPE:CTYPE:BITS",
        help="Add custom DEFTYPE definition")

    group = parser.add_argument_group("output arguments")
    group.add_argument(
        "--header", required=True, type=argparse.FileType('w'), metavar="FILE",
        help="CANopen object dictionary header file")
    group.add_argument(
        "--impl", required=True, type=argparse.FileType('w'), metavar="FILE",
        help="CANopen object dictionary implementation file")
    group.add_argument(
        "-p", "--prefix", default="objdict",
        help="prefix of the generated CANopen object dictionary structures "
        "(default: %(default)s)")
    group.add_argument(
        "--bindir", type=str,
        help="CMAKE_BINARY_DIR for pure logging purposes. No trailing slash.")

    return parser.parse_args()

def main():
    """Parse arguments and generate CANopen object dictionary code"""
    args = parse_args()
    objdict = canopen.import_od(args.input)

    # TODO: parse args.deftype for appending to the data type map

    # Store the command used for generating the files
    cmd = []
    for arg in sys.argv:
        if arg.startswith("--bindir"):
            # Drop. Assumes --bindir= was passed with '=' sign.
            continue
        if args.bindir and arg.startswith(args.bindir):
            # +1 to also strip '/' or '\' separator
            striplen = min(len(args.bindir)+1, len(arg))
            cmd.append(arg[striplen:])
            continue

        if args.zephyr_base is not None:
            cmd.append(arg.replace(args.zephyr_base, '"${ZEPHYR_BASE}"'))
        else:
            cmd.append(arg)
    cmd = " ".join(cmd)

    generate_header(cmd, args.header, args.prefix)
    generate_impl(cmd, objdict, args.impl, args.prefix)

if __name__ == "__main__":
    main()
