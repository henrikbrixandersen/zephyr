.. _gs_usb:

USB/CAN Adapter Sample Application
##################################

Overview
********

Requirements
************

Building and Running
********************

.. code-block:: console

   sudo modprobe gs_usb
   echo 2fe3 000f | sudo tee /sys/bus/usb/drivers/gs_usb/new_id
