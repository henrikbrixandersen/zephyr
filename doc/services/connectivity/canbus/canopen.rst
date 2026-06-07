.. _canopen:

CANopen
#######

.. contents::
    :local:
    :depth: 2

Overview
********

API Reference
*************

CANopen
-------

.. doxygengroup:: canopen

Network Management (NMT)
------------------------

.. _canopen_nmt:

.. graphviz::
   :caption: CANopen NMT finite-state automaton

   digraph canopen_nmt_states {
      node [style = rounded];
      init [shape = point];
      INITIALISATION [label = "Initialisation", shape = box];
      PRE_OPERATIONAL [label = "Pre-operational", shape = box];
      OPERATIONAL [label = "Operational", shape = box];
      STOPPED [label = "Stopped", shape = box];

      init -> INITIALISATION;

      INITIALISATION -> PRE_OPERATIONAL [dir = both];
      PRE_OPERATIONAL -> OPERATIONAL [dir = both];
      OPERATIONAL -> STOPPED [dir = both];

      OPERATIONAL -> INITIALISATION;
      STOPPED -> INITIALISATION;

      PRE_OPERATIONAL -> STOPPED [dir = both];
   }

.. doxygengroup:: canopen_nmt

Object Dictionary
-----------------

.. _canopen_od:

.. table:: CANopen object dictionary types
    :align: center

    +-----------------+---------+---------------------+--------------+
    | CANopen object  | Bit     | CANopen basic       | C type       |
    | dictionary type | size    | constructor         |              |
    +=================+=========+=====================+==============+
    | BOOLEAN         |       1 | BOOLEAN             | ``bool``     |
    +-----------------+---------+---------------------+--------------+
    | INTEGER8        |       8 | INTEGER             | ``int8_t``   |
    +-----------------+---------+                     +--------------+
    | INTEGER16       |      16 |                     | ``int16_t``  |
    +-----------------+---------+                     +--------------+
    | INTEGER24       |      24 |                     | ``int32_t``  |
    +-----------------+---------+                     +              +
    | INTEGER32       |      32 |                     |              |
    +-----------------+---------+                     +--------------+
    | INTEGER40       |      40 |                     | ``int64_t``  |
    +-----------------+---------+                     +              +
    | INTEGER48       |      48 |                     |              |
    +-----------------+---------+                     +              +
    | INTEGER56       |      56 |                     |              |
    +-----------------+---------+                     +              +
    | INTEGER64       |      64 |                     |              |
    +-----------------+---------+---------------------+--------------+
    | UNSIGNED8       |       8 | UNSIGNED            | ``uint8_t``  |
    +-----------------+---------+                     +--------------+
    | UNSIGNED16      |      16 |                     | ``uint16_t`` |
    +-----------------+---------+                     +--------------+
    | UNSIGNED24      |      24 |                     | ``uint32_t`` |
    +-----------------+---------+                     +              +
    | UNSIGNED32      |      32 |                     |              |
    +-----------------+---------+                     +--------------+
    | UNSIGNED40      |      40 |                     | ``uint64_t`` |
    +-----------------+---------+                     +              +
    | UNSIGNED48      |      48 |                     |              |
    +-----------------+---------+                     +              +
    | UNSIGNED56      |      56 |                     |              |
    +-----------------+---------+                     +              +
    | UNSIGNED64      |      64 |                     |              |
    +-----------------+---------+---------------------+--------------+
    | REAL32          |      32 | REAL32              | ``float``    |
    +-----------------+---------+---------------------+--------------+
    | REAL64          |      64 | REAL64              | ``double``   |
    +-----------------+---------+---------------------+--------------+
    | VISIBLE_STRING  |   n * 8 | UNSIGNED            | ``uint8_t``  |
    |                 |         |                     | array        |
    +-----------------+---------+                     +              +
    | OCTET_STRING    |   n * 8 |                     |              |
    |                 |         |                     |              |
    +-----------------+---------+                     +--------------+
    | UNICODE_STRING  |  n * 16 |                     | ``uint16_t`` |
    |                 |         |                     | array        |
    +-----------------+---------+---------------------+--------------+
    | TIME_OF_DAY     |      48 | UNSIGNED [1]_       | ``uint64_t`` |
    +-----------------+---------+---------------------+--------------+
    | TIME_DIFFERENCE |      48 | UNSIGNED [1]_       | ``uint64_t`` |
    +-----------------+---------+---------------------+--------------+
    | DOMAIN          |       0 | Application         | Application  |
    |                 |         | specific            | specific     |
    +-----------------+---------+---------------------+--------------+

.. [1]

   The CANopen object dictionary data types ``TIME_OF_DAY`` and
   ``TIME_DIFFERENCE`` are extended data types consisting of a mix of
   ``UNSIGNED`` and ``VOID`` (don't care) components. In Zephyr, these can be
   represented as a single ``UNSIGNED`` type with a bit size of 48 bits.

.. doxygengroup:: canopen_od

Service Data Object (SDO)
-------------------------

.. doxygengroup:: canopen_sdo
