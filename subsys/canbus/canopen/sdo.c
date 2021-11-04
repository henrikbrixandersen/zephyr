/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen/sdo.h>

const char *canopen_sdo_abort_code_str(enum canopen_sdo_abort_code code)
{
	switch (code) {
	case CANOPEN_SDO_ABORT_TOGGLE_BIT_NOT_ALTERNATED:
		return "Toggle bit not alternated";
	case CANOPEN_SDO_ABORT_SDO_PROTOCOL_TIMED_OUT:
		return "SDO protocol timed out";
	case CANOPEN_SDO_ABORT_CLIENT_SERVER_CMD_NOT_VALID:
		return "Client/server command specifier not valid or unknown";
	case CANOPEN_SDO_ABORT_INVALID_BLOCK_SIZE:
		return "Invalid block size";
	case CANOPEN_SDO_ABORT_INVALID_SEQUENCE_NUMBER:
		return "Invalid sequence number";
	case CANOPEN_SDO_ABORT_CRC_ERROR:
		return "CRC error";
	case CANOPEN_SDO_ABORT_OUT_OF_MEMORY:
		return "Out of memory";
	case CANOPEN_SDO_ABORT_UNSUPPORTED_ACCESS:
		return "Unsupported access to an object";
	case CANOPEN_SDO_ABORT_WRITE_ONLY:
		return "Attempt to read a write only object";
	case CANOPEN_SDO_ABORT_READ_ONLY:
		return "Attempt to write a read only object";
	case CANOPEN_SDO_ABORT_OBJECT_DOES_NOT_EXIST:
		return "Object does not exist in the object dictionary";
	case CANOPEN_SDO_ABORT_OBJECT_CANNOT_BE_MAPPED:
		return "Object cannot be mapped to the PDO";
	case CANOPEN_SDO_ABORT_PDO_LENGTH_EXCEEDED:
		return "The number and length of the objects to be mapped would exceed PDO length";
	case CANOPEN_SDO_ABORT_PARAMETER_INCOMPATIBLE:
		return "General parameter incompatibility";
	case CANOPEN_SDO_ABORT_DEVICE_INCOMPATIBLE:
		return "General internal incompatibility in the device";
	case CANOPEN_SDO_ABORT_HARDWARE_ERROR:
		return "Access failed due to a hardware error";
	case CANOPEN_SDO_ABORT_LENGTH_MISMATCH:
		return "Length of service parameter does not match";
	case CANOPEN_SDO_ABORT_LENGTH_TOO_HIGH:
		return "Length of service parameter too high";
	case CANOPEN_SDO_ABORT_LENGTH_TOO_LOW:
		return "Length of service parameter too low";
	case CANOPEN_SDO_ABORT_SUBINDEX_DOES_NOT_EXIST:
		return "Sub-index does not exist";
	case CANOPEN_SDO_ABORT_PARAMETER_VALUE_INVALID:
		return "Invalid value for parameter";
	case CANOPEN_SDO_ABORT_PARAMETER_VALUE_TOO_HIGH:
		return "Value of parameter written too high";
	case CANOPEN_SDO_ABORT_PARAMETER_VALUE_TOO_LOW:
		return "Value of parameter written too low";
	case CANOPEN_SDO_ABORT_MAX_LESS_THAN_MIN:
		return "Maximum value is less than minimum value";
	case CANOPEN_SDO_ABORT_RESOURCE_NOT_AVAILABLE:
		return "Resource not available";
	case CANOPEN_SDO_ABORT_GENERAL_ERROR:
		return "General error";
	case CANOPEN_SDO_ABORT_APPLICATION_CANNOT_STORE:
		return "Data cannot be transferred or stored to the application";
	case CANOPEN_SDO_ABORT_APPLICATION_LOCAL_CONTROL:
		return "Data cannot be transferred or stored to the application (local control)";
	case CANOPEN_SDO_ABORT_APPLICATION_DEVICE_STATE:
		return "Data cannot be transferred or stored to the application (device state)";
	case CANOPEN_SDO_ABORT_NO_OBJECT_DICTIONARY:
		return "No object dictionary present";
	case CANOPEN_SDO_ABORT_NO_DATA_AVAILABLE:
		return "No data available";
	default:
		return "(Unknown)";
	};
}
