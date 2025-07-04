/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen.h>
#include <zephyr/canbus/canopen/sdo.h>
#include <zephyr/drivers/can.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(canopen_sdo, CONFIG_CANOPEN_LOG_LEVEL);

/* SDO Client Command Specifier (CCS) */
#define CCS_MASK                      GENMASK(7, 5)
#define CCS_DOWNLOAD_SEGMENT_REQUEST  0U
#define CCS_INITIATE_DOWNLOAD_REQUEST 1U
#define CCS_INITIATE_UPLOAD_REQUEST   2U
#define CCS_UPLOAD_SEGMENT_REQUEST    3U
#define CCS_BLOCK_UPLOAD_REQUEST      5U
#define CCS_BLOCK_DOWNLOAD_REQUEST    6U

/* SDO Server Command Specifier (SCS) */
#define SCS_MASK                       GENMASK(7, 5)
#define SCS_UPLOAD_SEGMENT_RESPONSE    0U
#define SCS_DOWNLOAD_SEGMENT_RESPONSE  1U
#define SCS_INITIATE_UPLOAD_RESPONSE   2U
#define SCS_INITIATE_DOWNLOAD_RESPONSE 3U
#define SCS_BLOCK_DOWNLOAD_RESPONSE    5U
#define SCS_BLOCK_UPLOAD_RESPONSE      6U

/* SDO Command Specifier (CS) */
#define CS_MASK                   GENMASK(7, 5)
#define CS_ABORT_TRANSFER_REQUEST 4U

/* SDO number limits */
#define SDO_NUMBER_MIN 1
#define SDO_NUMBER_MAX 128

const char *canopen_sdo_abort_code_str(uint32_t abort_code)
{
	switch (abort_code) {
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

static void canopen_sdo_server_request_callback(const struct device *can, struct can_frame *frame,
						void *user_data)
{
	/* struct canopen_sdo_server *server = user_data; */

	ARG_UNUSED(can);

	LOG_INF("data[0] = 0x%02x", frame->data[0]);
}

int canopen_sdo_server_init(struct canopen_sdo_server *server, uint8_t sdo_number,
			    const struct device *can)
{
	struct can_filter filter;
	int err;

	if (sdo_number < SDO_NUMBER_MIN || sdo_number > SDO_NUMBER_MAX) {
		LOG_ERR("invalid SDO number %d", sdo_number);
		return -EINVAL;
	}

	server->sdo_number = sdo_number;
	server->can = can;

	/* TODO: obtain COB-ID from OD */
	/* TODO: check if COB-ID is enabled/"exists" */
	canopen_cob_id_to_can_filter(0x67f, &filter);
	err = can_add_rx_filter(server->can, canopen_sdo_server_request_callback, server, &filter);
	if (err < 0) {
		LOG_ERR("failed to add CANopen SDO server CAN filter (err %d)", err);
		return -EIO;
	}

	return 0;
}
