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

# Mapping from CANopen data type to number of bits + C-type
data_types = {
    canopen.objectdictionary.datatypes.BOOLEAN: (1, "bool"),
    canopen.objectdictionary.datatypes.INTEGER8: (8, "int8_t"),
    canopen.objectdictionary.datatypes.INTEGER16: (16, "int16_t"),
    canopen.objectdictionary.datatypes.INTEGER32: (32, "int32_t"),
    canopen.objectdictionary.datatypes.UNSIGNED8: (8, "uint8_t"),
    canopen.objectdictionary.datatypes.UNSIGNED16: (16, "uint16_t"),
    canopen.objectdictionary.datatypes.UNSIGNED32: (32, "uint32_t"),
    canopen.objectdictionary.datatypes.REAL32: (32, "float"),
    canopen.objectdictionary.datatypes.VISIBLE_STRING: (8, "uint8_t"),
    canopen.objectdictionary.datatypes.OCTET_STRING: (8, "uint8_t"),
    canopen.objectdictionary.datatypes.UNICODE_STRING: (16, "uint16_t"),
    canopen.objectdictionary.datatypes.TIME_OF_DAY: (48, "uint64_t"),
    canopen.objectdictionary.datatypes.TIME_DIFFERENCE: (48, "uint64_t"),
    canopen.objectdictionary.datatypes.DOMAIN: (0, "int"), # TODO
    canopen.objectdictionary.datatypes.INTEGER24: (24, "int32_t"),
    canopen.objectdictionary.datatypes.REAL64: (64, "double"),
    canopen.objectdictionary.datatypes.INTEGER40: (40, "int64_t"),
    canopen.objectdictionary.datatypes.INTEGER48: (48, "int64_t"),
    canopen.objectdictionary.datatypes.INTEGER56: (56, "int64_t"),
    canopen.objectdictionary.datatypes.INTEGER64: (64, "int64_t"),
    canopen.objectdictionary.datatypes.UNSIGNED24: (24, "uint32_t"),
    canopen.objectdictionary.datatypes.UNSIGNED40: (40, "uint64_t"),
    canopen.objectdictionary.datatypes.UNSIGNED48: (48, "uint64_t"),
    canopen.objectdictionary.datatypes.UNSIGNED56: (56, "uint64_t"),
    canopen.objectdictionary.datatypes.UNSIGNED64: (64, "uint64_t"),
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
                 f"CANOPEN_OD_DECLARE({prefix});\n"
                 "\n"
                 f"#endif /* {guard} */\n")

def write_entry(impl: TextIO, indent: int, comment: bool,
                entry: canopen.objectdictionary.ODVariable) -> None:
    """Write CANopen object dictionary object entry definition"""
    tabs = "\t" * indent
    attr = "0U"

    if comment:
        impl.write(f"{tabs}/* {entry.subindex} - {entry.name} */\n")

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
    bits = data_types[entry.data_type][0]
    ctype = data_types[entry.data_type][1]

    impl.write(f"{tabs}CANOPEN_OD_ENTRY({entry.subindex}U, 0x{entry.data_type:04x}U, "
               f"{bits}U, &({ctype}){{ 0U }}, NULL, NULL, sizeof({ctype}),\n"
               f"{tabs}		 {attr}),\n")

def write_object(impl: TextIO, indent: int, obj: Union[canopen.objectdictionary.ODVariable,
                                                       canopen.objectdictionary.ODArray,
                                                       canopen.objectdictionary.ODRecord]) -> None:
    """Write CANopen object dictionary object definition"""
    tabs = "\t" * indent

    # TODO: DOMAIN? Perhaps drop objcode in code?
    if isinstance(obj, canopen.objectdictionary.ODVariable):
        objcode = canopen.objectdictionary.eds.VAR
    elif isinstance(obj, canopen.objectdictionary.ODRecord):
        objcode = canopen.objectdictionary.eds.RECORD
    elif isinstance(obj, canopen.objectdictionary.ODArray):
        objcode = canopen.objectdictionary.eds.ARR

    impl.write(f"{tabs}/* {obj.index:04x}h - {obj.name} */\n"
               f"{tabs}CANOPEN_OD_OBJECT_ENTRIES(0x{obj.index:04x}U, {objcode}U,\n")

    if isinstance(obj, canopen.objectdictionary.ODVariable):
        write_entry(impl, 2, False, obj)
    elif isinstance(obj, canopen.objectdictionary.ODArray):
        for entry in obj.values():
            write_entry(impl, 2, True, entry)
    elif isinstance(obj, canopen.objectdictionary.ODRecord):
        for entry in obj.values():
            write_entry(impl, 2, True, entry)

    impl.write(f"{tabs}),\n")

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
               f"CANOPEN_OD_DEFINE_OBJECTS({prefix},\n")

    for obj in objdict.values():
        write_object(impl, 1, obj)

    impl.write(");\n")

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

    # TODO: add options for appending to the data type map

    return parser.parse_args()

def main():
    """Parse arguments and generate CANopen object dictionary code"""
    args = parse_args()
    objdict = canopen.import_od(args.input)

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
