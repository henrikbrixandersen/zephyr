# Copyright (c) 2020 Vestas Wind Systems A/S
#
# SPDX-License-Identifier: Apache-2.0

'''west "erase" command'''

from textwrap import dedent

from west.commands import WestCommand

from run_common import desc_common, add_parser_common, do_run_common

class Erase(WestCommand):

    def __init__(self):
        super(Erase, self).__init__(
            'erase',
            # Keep this in sync with the string in west-commands.yml.
            'mass-erase the onboard flash',
            dedent('''
            Connects to the board and performs a mass-erase of the onboard flash\n\n''') +
            desc_common('erase'),
            accepts_unknown_args=True)

    def do_add_parser(self, parser_adder):
        return add_parser_common(parser_adder, self)

    def do_run(self, my_args, runner_args):
        do_run_common(self, my_args, runner_args,
                      'ZEPHYR_BOARD_FLASH_RUNNER')
