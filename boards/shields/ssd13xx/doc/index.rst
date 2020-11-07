.. _ssd13xx_shield:

Solomon SSD13xx generic shield
##############################

Overview
********

This is a generic shield for OLED displays based on Solomon SSD13xx display
controllers. These displays have an I2C or SPI interface and usually feature a
low interface pin count. Display pins can be connected to the pin header of a
board using jumper wires.

Current supported displays
==========================

+---------------------+---------------+-----------+---------------------+
| Display             | Controller /  | Interface | Shield Designation  |
|                     | Driver        |           |                     |
+=====================+===============+===========+=====================+
| No Name             | SSD1306 /     | I2C       | ssd1306_128x64      |
| 128x64 pixel        | ssd1306       |           |                     |
+---------------------+---------------+-----------+---------------------+
| No Name             | SSD1306 /     | SPI       | ssd1306_128x64_spi  |
| 128x64 pixel        | ssd1306       |           |                     |
+---------------------+---------------+-----------+---------------------+
| No Name             | SSD1306 /     | I2C       | ssd1306_128x32      |
| 128x32 pixel        | ssd1306       |           |                     |
+---------------------+---------------+-----------+---------------------+
| No Name             | SH1106 /      | I2C       | sh1106_128x64       |
| 128x64 pixel        | ssd1306       |           |                     |
+---------------------+---------------+-----------+---------------------+

Requirements
************

This shield can only be used with a board which provides a configuration for
Arduino connectors and defines a node alias for either the I2C or SPI interface
(see :ref:`shields` for more details).

Programming
***********

Set e.g. ``-DSHIELD=ssd1306_128x64`` when you invoke ``west build``. For example:

.. zephyr-app-commands::
   :zephyr-app: samples/gui/lvgl
   :board: frdm_k64f
   :shield: ssd1306_128x64
   :goals: build
