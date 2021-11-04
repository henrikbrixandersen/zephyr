#!/usr/bin/env python3
#
# Copyright (c) 2018-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
#
# SPDX-License-Identifier: Apache-2.0

"""Generate CANopen object dictionary code from EDS"""

import argparse
import re

import canopen
from typing import TextIO

def data_type_to_c_type(data_type: int) -> str:
    return "uint32_t"

def generate_header(objdict: canopen.ObjectDictionary, header: TextIO, prefix: str) -> None:
    """Generate CANopen object dictionary header file"""
    guard = f"__{prefix.upper()}_H__"
    header.write("/*\n"
                 " * This file was automatically generated using the following command:\n"
                 "* {cmd}\n"
                 "*\n"
                 "*/\n"
                 "\n"
                 f"#ifndef {guard}\n"
                 f"#define {guard}\n"
                 "\n"
                 "#include <zephyr/canbus/canopen/od.h>\n"
                 "\n"
                 f"CANOPEN_OD_DECLARE({prefix});\n"
                 "\n"
                 f"#endif /* {guard} */\n")

def generate_impl(objdict: canopen.ObjectDictionary, impl: TextIO, prefix: str) -> None:
    """Generate CANopen object dictionary implementation file"""
    impl.write("/*\n"
               " * This file was automatically generated using the following command:\n"
               "* {cmd}\n"
               "*\n"
               "*/\n"
               "\n"
               "#include <zephyr/canbus/canopen/od.h>\n"
               "\n"
               f"CANOPEN_OD_DEFINE_OBJECTS({prefix});\n")

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
    args = parse_args()
    objdict = canopen.import_od(args.input)
    generate_header(objdict, args.header, args.prefix)
    generate_impl(objdict, args.impl, args.prefix)

if __name__ == "__main__":
    main()
