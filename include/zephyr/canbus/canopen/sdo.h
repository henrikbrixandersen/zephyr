/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_CANBUS_CANOPEN_SDO_H_
#define ZEPHYR_INCLUDE_CANBUS_CANOPEN_SDO_H_

/**
 * @file canbus/canopen/sdo.h
 * @brief Service Data Object (SDO)
 * @defgroup canopen_sdo Service Data Object (SDO)
 * @ingroup canopen
 * @{
 */

#include <zephyr/canbus/canopen/od.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CANopen SDO abort codes
 */
/* clang-format off */
enum canopen_sdo_abort_code {
	/** Toggle bit not alternated. */
	CANOPEN_SDO_ABORT_TOGGLE_BIT_NOT_ALTERNATED =   0x05030000U,
	/** SDO protocol timed out. */
	CANOPEN_SDO_ABORT_SDO_PROTOCOL_TIMED_OUT =      0x05040000U,
	/** Client/server command specifier not valid or unknown. */
	CANOPEN_SDO_ABORT_CLIENT_SERVER_CMD_NOT_VALID = 0x05040001U,
	/** Invalid block size (block mode only). */
	CANOPEN_SDO_ABORT_INVALID_BLOCK_SIZE =          0x05040002U,
	/** Invalid sequence number (block mode only). */
	CANOPEN_SDO_ABORT_INVALID_SEQUENCE_NUMBER =     0x05040003U,
	/** CRC error (block mode only). */
	CANOPEN_SDO_ABORT_CRC_ERROR =                   0x05040004U,
	/** Out of memory. */
	CANOPEN_SDO_ABORT_OUT_OF_MEMORY =               0x05040005U,
	/** Unsupported access to an object. */
	CANOPEN_SDO_ABORT_UNSUPPORTED_ACCESS =          0x06010000U,
	/** Attempt to read a write only object. */
	CANOPEN_SDO_ABORT_WRITE_ONLY =                  0x06010001U,
	/** Attempt to write a read only object. */
	CANOPEN_SDO_ABORT_READ_ONLY =                   0x06010002U,
	/** Object does not exist in the object dictionary. */
	CANOPEN_SDO_ABORT_OBJECT_DOES_NOT_EXIST =       0x06020000U,
	/** Object cannot be mapped to the PDO. */
	CANOPEN_SDO_ABORT_OBJECT_CANNOT_BE_MAPPED =     0x06040041U,
	/** The number and length of the objects to be mapped would exceed PDO length. */
	CANOPEN_SDO_ABORT_PDO_LENGTH_EXCEEDED =         0x06040042U,
	/** General parameter incompatibility reason. */
	CANOPEN_SDO_ABORT_PARAMETER_INCOMPATIBLE =      0x06040043U,
	/** General internal incompatibility in the device. */
	CANOPEN_SDO_ABORT_DEVICE_INCOMPATIBLE =         0x06040047U,
	/** Access failed due to a hardware error. */
	CANOPEN_SDO_ABORT_HARDWARE_ERROR =              0x06060000U,
	/** Data type does not match, length of service parameter does not match. */
	CANOPEN_SDO_ABORT_LENGTH_MISMATCH =             0x06070010U,
	/** Data type does not match, length of service parameter too high. */
	CANOPEN_SDO_ABORT_LENGTH_TOO_HIGH =             0x06070012U,
	/** Data type does not match, length of service parameter too low. */
	CANOPEN_SDO_ABORT_LENGTH_TOO_LOW =              0x06070013U,
	/** Sub-index does not exist. */
	CANOPEN_SDO_ABORT_SUBINDEX_DOES_NOT_EXIST =     0x06090011U,
	/** Invalid value for parameter (download only). */
	CANOPEN_SDO_ABORT_PARAMETER_VALUE_INVALID =     0x06090030U,
	/** Value of parameter written too high (download only). */
	CANOPEN_SDO_ABORT_PARAMETER_VALUE_TOO_HIGH =    0x06090031U,
	/** Value of parameter written too low (download only). */
	CANOPEN_SDO_ABORT_PARAMETER_VALUE_TOO_LOW =     0x06090032U,
	/** Maximum value is less than minimum value. */
	CANOPEN_SDO_ABORT_MAX_LESS_THAN_MIN =           0x06090036U,
	/** Resource not available: SDO connection. */
	CANOPEN_SDO_ABORT_RESOURCE_NOT_AVAILABLE =      0x060A0023U,
	/** General error. */
	CANOPEN_SDO_ABORT_GENERAL_ERROR =               0x08000000U,
	/** Data cannot be transferred or stored to the application. */
	CANOPEN_SDO_ABORT_APPLICATION_CANNOT_STORE =    0x08000020U,
	/** Data cannot be transferred or stored to the application because of local control. */
	CANOPEN_SDO_ABORT_APPLICATION_LOCAL_CONTROL =   0x08000021U,
	/** Data cannot be transferred or stored to the application because of the device state. */
	CANOPEN_SDO_ABORT_APPLICATION_DEVICE_STATE =    0x08000022U,
	/** Object dictionary dynamic generation fails or no object dictionary is present. */
	CANOPEN_SDO_ABORT_NO_OBJECT_DICTIONARY =        0x08000023U,
	/** No data available. */
	CANOPEN_SDO_ABORT_NO_DATA_AVAILABLE =           0x08000024U,
};
/* clang-format on */

struct canopen_sdo_server {
	struct canopen_od *od;
};

/**
 * @brief Get the description of a CANopen SDO abort code
 *
 * @param code CANopen SDO abort code which description should be returned.
 * @return String describing the provided abort code.
 */
const char *canopen_sdo_abort_code_str(enum canopen_sdo_abort_code code);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_SDO_H_ */
