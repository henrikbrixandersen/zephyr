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
    # TODO: use look-up table for c-type, bits, and size
    impl.write(f"{tabs}CANOPEN_OD_ENTRY({entry.subindex}U, 0x{entry.data_type:04x}U, "
               f"{len(entry)}U, NULL, NULL, NULL, 0U,\n"
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

    return parser.parse_args()

def main():
    """Parse arguments and generate CANopen object dictionary code"""
    # TODO: store the cmd used for generating the files
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
