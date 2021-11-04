#!/usr/bin/env python3
#
# Copyright (c) 2018-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
#
# SPDX-License-Identifier: Apache-2.0

"""Generate CANopen object dictionary code from EDS"""

import argparse
import sys
from typing import TextIO

import canopen
from canopen.objectdictionary.datatypes import (
    BOOLEAN,
    DATA_TYPES,
    DOMAIN,
    INTEGER8,
    INTEGER16,
    INTEGER24,
    INTEGER32,
    INTEGER40,
    INTEGER48,
    INTEGER56,
    INTEGER64,
    OCTET_STRING,
    REAL32,
    REAL64,
    SIGNED_TYPES,
    TIME_DIFFERENCE,
    TIME_OF_DAY,
    UNICODE_STRING,
    UNSIGNED8,
    UNSIGNED16,
    UNSIGNED24,
    UNSIGNED32,
    UNSIGNED40,
    UNSIGNED48,
    UNSIGNED56,
    UNSIGNED64,
    UNSIGNED_TYPES,
    VISIBLE_STRING,
)

# Mapping from CANopen data type to C-type + number of bits
data_types = {
    BOOLEAN: ("bool", 1),
    INTEGER8: ("int8_t", 8),
    INTEGER16: ("int16_t", 16),
    INTEGER32: ("int32_t", 32),
    UNSIGNED8: ("uint8_t", 8),
    UNSIGNED16: ("uint16_t", 16),
    UNSIGNED32: ("uint32_t", 32),
    REAL32: ("float", 32),
    VISIBLE_STRING: ("uint8_t", 8),
    OCTET_STRING: ("uint8_t", 8),
    UNICODE_STRING: ("uint16_t", 16),
    TIME_OF_DAY: ("uint64_t", 48),
    TIME_DIFFERENCE: ("uint64_t", 48),
    DOMAIN: (None, 0),
    INTEGER24: ("int32_t", 24),
    REAL64: ("double", 64),
    INTEGER40: ("int64_t", 40),
    INTEGER48: ("int64_t", 48),
    INTEGER56: ("int64_t", 56),
    INTEGER64: ("int64_t", 64),
    UNSIGNED24: ("uint32_t", 24),
    UNSIGNED40: ("uint64_t", 40),
    UNSIGNED48: ("uint64_t", 48),
    UNSIGNED56: ("uint64_t", 56),
    UNSIGNED64: ("uint64_t", 64),
}


class UnknownDataTypeError(Exception):
    """
    Unknown CANopen data type exception.
    """

    def __init__(self, var: canopen.objectdictionary.ODVariable) -> None:
        self.var = var

    def __str__(self):
        return (
            f"index {self.var.index:04x}h subindex {self.var.subindex} has "
            f"unsupported data type 0x{self.var.data_type:04x}. Missing --deftype?"
        )


class Entry:
    """Wrapper class representing a CANopen object dictionary object entry."""

    def __init__(self, prefix: str, var: canopen.objectdictionary.ODVariable) -> None:
        self.prefix = prefix
        self.var = var

    @property
    def index(self) -> int:
        """Object Dictionary entry 16-bit index."""
        return self.var.index

    @property
    def subindex(self) -> int:
        """Object Dictionary entry 8-bit subindex."""
        return self.var.subindex

    @property
    def name(self) -> str:
        """Object Dictionary entry name."""
        return self.var.name

    @property
    def data_type(self) -> int:
        """Object Dictionary entry data type."""
        return self.var.data_type

    @property
    def low_limit(self) -> str:
        """Object Dictionary entry data value low limit."""
        if self.var.min:
            if self.var.data_type == REAL32:
                return f"{self.var.min}f"
            if self.var.data_type in UNSIGNED_TYPES:
                return f"0x{self.var.min:x}U"
            if self.var.data_type in SIGNED_TYPES:
                return f"0x{self.var.min:x}"

            return str(self.var.min)

        return None

    @property
    def high_limit(self) -> str:
        """Object Dictionary entry data value high limit."""
        if self.var.max:
            if self.var.data_type == REAL32:
                return f"{self.var.max}f"
            if self.var.data_type in UNSIGNED_TYPES:
                return f"0x{self.var.max:x}U"
            if self.var.data_type in SIGNED_TYPES:
                return f"0x{self.var.max:x}"

            return str(self.var.max)

        return None

    @property
    def default(self) -> str:
        """Object Dictionary entry data default value."""
        if self.var.default is not None:
            if self.var.data_type == REAL32:
                return f"{self.var.default}f"
            if self.var.data_type == OCTET_STRING:
                return f"{{{', '.join(f'0x{b:02x}U' for b in self.var.default)}}}"
            if self.var.data_type == UNICODE_STRING:
                return f"{{{', '.join(f'0x{u:04x}U' for u in self.var.default)}}}"
            if self.var.data_type == VISIBLE_STRING:
                return f"{{{', '.join(f'\'{c}\'' for c in self.var.default)}}}"
            if self.var.data_type in UNSIGNED_TYPES:
                return f"0x{self.var.default:x}U"
            if self.var.data_type in SIGNED_TYPES:
                return f"0x{self.var.default:x}"

            return str(self.var.default)

        return None

    @property
    def cname(self) -> str:
        """Object Dictionary entry C name."""
        return f"{self.prefix}_{self.var.index:04x}sub{self.var.subindex}"

    @property
    def carray(self) -> str:
        """Object Dictionary entry C array specifier"""
        if self.var.data_type in DATA_TYPES:
            return "[]"

        return ""

    @property
    def ctype(self) -> str:
        """Object Dictionary entry C type."""
        if self.var.data_type not in data_types:
            raise UnknownDataTypeError(self.var)

        return data_types[self.var.data_type][0]

    @property
    def csize(self) -> str:
        """Object Dictionary entry C size."""
        if self.ctype:
            if self.var.data_type in DATA_TYPES:
                return f"{len(self.var.default)}U * (sizeof({self.ctype}))"

            return f"sizeof({self.ctype})"

        return "0"

    @property
    def bits(self) -> int:
        """Object Dictionary entry bits."""
        if self.var.data_type not in data_types:
            raise UnknownDataTypeError(self.var)

        return data_types[self.var.data_type][1]

    @property
    def attr(self) -> str:
        """Object Dictionary entry attributes."""
        match self.var.access_type:
            case "ro":
                attr = "CANOPEN_OD_ATTR_ACCESS_RO"
            case "wo":
                attr = "CANOPEN_OD_ATTR_ACCESS_WO"
            case "rw" | "rwr" | "rww":
                attr = "CANOPEN_OD_ATTR_ACCESS_RW"
            case "const":
                attr = "CANOPEN_OD_ATTR_ACCESS_CONST"

        if self.var.pdo_mappable:
            if self.var.access_type == "rwr":
                attr += " | CANOPEN_OD_ATTR_PDO_MAPPABLE_TPDO"
            elif self.var.access_type == "rww":
                attr += " | CANOPEN_OD_ATTR_PDO_MAPPABLE_RPDO"
            else:
                attr += " | CANOPEN_OD_ATTR_PDO_MAPPABLE"

        if self.var.relative:
            attr += " | CANOPEN_OD_ATTR_RELATIVE"

        return attr


class EntryIterator:
    """Class for iterating over CANopen object dictionary object entries."""

    def __init__(
        self,
        prefix: str,
        obj: canopen.objectdictionary.ODVariable
        | canopen.objectdictionary.ODArray
        | canopen.objectdictionary.ODRecord,
    ) -> None:
        self.prefix = prefix
        self.obj = obj
        self.it = None
        self.done = False

    def __iter__(self):
        return EntryIterator(self.prefix, self.obj)

    def __next__(self) -> Entry:
        if isinstance(
            self.obj, canopen.objectdictionary.ODArray | canopen.objectdictionary.ODRecord
        ):
            if self.it is None:
                self.it = iter(self.obj.values())
            return Entry(self.prefix, self.it.__next__())

        if isinstance(self.obj, canopen.objectdictionary.ODVariable) and not self.done:
            self.done = True
            return Entry(self.prefix, self.obj)

        raise StopIteration


def generate_header(cmd: str, header: TextIO, prefix: str) -> None:
    """Generate CANopen object dictionary header file"""
    guard = f"__{prefix.upper()}_H__"
    header.write(
        "/*\n"
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
        f"#endif /* {guard} */\n"
    )


def generate_impl(cmd: str, objdict: canopen.ObjectDictionary, impl: TextIO, prefix: str) -> None:
    """Generate CANopen object dictionary implementation file"""
    impl.write(
        "/*\n"
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
        f"#define APP_DMEM K_APP_DMEM({prefix}_partition)\n"
        "#else /* CONFIG_USERSPACE */\n"
        "#define APP_DMEM\n"
        "#endif /* !CONFIG_USERSPACE */\n\n"
    )

    for obj in objdict.values():
        impl.write(f"/* {obj.index:04x}h - {obj.name} */\n")
        for entry in EntryIterator(prefix, obj):
            if entry.ctype:
                if entry.default is not None:
                    impl.write(
                        f"APP_DMEM static {entry.ctype} {entry.cname}{entry.carray} = "
                        f"{entry.default};\n"
                    )
                else:
                    impl.write(f"APP_DMEM static {entry.ctype} {entry.cname};\n")

                if entry.low_limit:
                    impl.write(
                        f"static const {entry.ctype} {entry.cname}_min{entry.carray} = "
                        f"{entry.low_limit};\n"
                    )

                if entry.high_limit:
                    impl.write(
                        f"static const {entry.ctype} {entry.cname}_max{entry.carray} = "
                        f"{entry.high_limit};\n"
                    )

        impl.write(
            f"static const struct canopen_od_entry {prefix}_{obj.index:04x}_entries[] = {{\n"
        )
        for entry in EntryIterator(prefix, obj):
            impl.write(f"\t/* {entry.subindex} - {entry.name} */\n")
            data = "NULL"
            low = "NULL"
            high = "NULL"

            if entry.ctype:
                data = f"&{entry.cname}"

                if entry.low_limit:
                    low = f"&{entry.cname}_min"

                if entry.high_limit:
                    high = f"&{entry.cname}_max"

            impl.write(
                f"\tCANOPEN_OD_ENTRY({entry.subindex}U, 0x{entry.data_type:04x}U, "
                f"{entry.bits}U, {data}, {low}, {high}, {entry.csize},\n"
                f"\t\t\t {entry.attr}),\n"
            )
        impl.write("};\n\n")

    impl.write(f"APP_DMEM static struct canopen_od_object {prefix}_objects[] = {{\n")
    for obj in objdict.values():
        impl.write(
            f"\t/* {obj.index:04x}h - {obj.name} */\n"
            f"\tCANOPEN_OD_OBJECT(0x{obj.index:04x}U,\n"
            f"\t\t\t  ARRAY_SIZE({prefix}_{obj.index:04x}_entries),\n"
            f"\t\t\t  {prefix}_{obj.index:04x}_entries),\n"
        )
    impl.write("};\n\n")

    impl.write(
        f"APP_DMEM CANOPEN_OD_DEFINE({prefix}, ARRAY_SIZE({prefix}_objects), {prefix}_objects);\n"
    )


def parse_args():
    """Parse arguments"""
    parser = argparse.ArgumentParser(
        description="CANopen object dictionary code generator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("-z", "--zephyr-base", help="Zephyr base directory")

    group = parser.add_argument_group("input arguments")
    group.add_argument(
        "-i",
        "--input",
        required=True,
        type=argparse.FileType('r'),
        metavar="FILE",
        help="CANopen object dictionary input file (EDS)",
    )
    group.add_argument(
        "-d",
        "--deftype",
        dest="deftypes",
        action='append',
        default=[],
        metavar="DEFTYPE:CTYPE:BITS",
        help="Add custom DEFTYPE definition. This argument can be specified multiple times.",
    )

    group = parser.add_argument_group("output arguments")
    group.add_argument(
        "--header",
        required=True,
        type=argparse.FileType('w'),
        metavar="FILE",
        help="CANopen object dictionary C header file",
    )
    group.add_argument(
        "--impl",
        required=True,
        type=argparse.FileType('w'),
        metavar="FILE",
        help="CANopen object dictionary C implementation file",
    )
    group.add_argument(
        "-p",
        "--prefix",
        default="objdict",
        help="prefix of the generated CANopen object dictionary structures (default: %(default)s)",
    )
    group.add_argument(
        "--bindir", type=str, help="CMAKE_BINARY_DIR for pure logging purposes. No trailing slash."
    )

    return parser.parse_args()


def main():
    """Parse arguments and generate CANopen object dictionary code"""
    args = parse_args()
    objdict = canopen.import_od(args.input, 0)

    for deftype in args.deftypes:
        data_type, ctype, bits = deftype.split(':')
        if ctype == "":
            ctype = None
        data_types[int(data_type, 0)] = (ctype, int(bits, 0))

    # Store the command used for generating the files
    cmd = []
    for arg in sys.argv:
        if arg.startswith("--bindir"):
            # Drop. Assumes --bindir= was passed with '=' sign.
            continue
        if args.bindir and arg.startswith(args.bindir):
            # +1 to also strip '/' or '\' separator
            striplen = min(len(args.bindir) + 1, len(arg))
            cmd.append(arg[striplen:])
            continue

        if args.zephyr_base is not None:
            cmd.append(arg.replace(args.zephyr_base, '"${ZEPHYR_BASE}"'))
        else:
            cmd.append(arg)
    cmd = " ".join(cmd)

    try:
        generate_header(cmd, args.header, args.prefix)
        generate_impl(cmd, objdict, args.impl, args.prefix)
    except UnknownDataTypeError as e:
        sys.exit(f"{sys.argv[0]}: {args.input.name}: {e}")


if __name__ == "__main__":
    main()
