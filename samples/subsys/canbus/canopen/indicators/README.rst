.. zephyr:code-sample:: canopen-indicators
   :name: CANopen Indicators
   :relevant-api: canopen_indicators

   Interactive shell for CANopen indicators

Overview
********

TODO

Requirements
************

.. include:: ../common/sample_canopen_leds.rst

Building and Running
********************

TODO: single LED example

Example building for :zephyr:board:`lpcxpresso55s16` with bi-color LED:

TODO

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/canbus/canopen/indicators
   :board: lpcxpresso55s16
   :goals: build flash
