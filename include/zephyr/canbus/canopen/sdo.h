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

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/smf.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name CANopen SDO abort codes
 * @{
 */
/** @brief Toggle bit not alternated. */
#define CANOPEN_SDO_ABORT_TOGGLE_BIT_NOT_ALTERNATED   0x05030000U
/** @brief SDO protocol timed out. */
#define CANOPEN_SDO_ABORT_SDO_PROTOCOL_TIMED_OUT      0x05040000U
/** @brief Client/server command specifier not valid or unknown. */
#define CANOPEN_SDO_ABORT_CLIENT_SERVER_CMD_NOT_VALID 0x05040001U
/** @brief Invalid block size (block mode only). */
#define CANOPEN_SDO_ABORT_INVALID_BLOCK_SIZE          0x05040002U
/** @brief Invalid sequence number (block mode only). */
#define CANOPEN_SDO_ABORT_INVALID_SEQUENCE_NUMBER     0x05040003U
/** @brief CRC error (block mode only). */
#define CANOPEN_SDO_ABORT_CRC_ERROR                   0x05040004U
/** @brief Out of memory. */
#define CANOPEN_SDO_ABORT_OUT_OF_MEMORY               0x05040005U
/** @brief Unsupported access to an object. */
#define CANOPEN_SDO_ABORT_UNSUPPORTED_ACCESS          0x06010000U
/** @brief Attempt to read a write only object. */
#define CANOPEN_SDO_ABORT_WRITE_ONLY                  0x06010001U
/** @brief Attempt to write a read only object. */
#define CANOPEN_SDO_ABORT_READ_ONLY                   0x06010002U
/** @brief Object does not exist in the object dictionary. */
#define CANOPEN_SDO_ABORT_OBJECT_DOES_NOT_EXIST       0x06020000U
/** @brief Object cannot be mapped to the PDO. */
#define CANOPEN_SDO_ABORT_OBJECT_CANNOT_BE_MAPPED     0x06040041U
/** @brief The number and length of the objects to be mapped would exceed PDO length. */
#define CANOPEN_SDO_ABORT_PDO_LENGTH_EXCEEDED         0x06040042U
/** @brief General parameter incompatibility reason. */
#define CANOPEN_SDO_ABORT_PARAMETER_INCOMPATIBLE      0x06040043U
/** @brief General internal incompatibility in the device. */
#define CANOPEN_SDO_ABORT_DEVICE_INCOMPATIBLE         0x06040047U
/** @brief Access failed due to a hardware error. */
#define CANOPEN_SDO_ABORT_HARDWARE_ERROR              0x06060000U
/** @brief Data type does not match, length of service parameter does not match. */
#define CANOPEN_SDO_ABORT_LENGTH_MISMATCH             0x06070010U
/** @brief Data type does not match, length of service parameter too high. */
#define CANOPEN_SDO_ABORT_LENGTH_TOO_HIGH             0x06070012U
/** @brief Data type does not match, length of service parameter too low. */
#define CANOPEN_SDO_ABORT_LENGTH_TOO_LOW              0x06070013U
/** @brief Sub-index does not exist. */
#define CANOPEN_SDO_ABORT_SUBINDEX_DOES_NOT_EXIST     0x06090011U
/** @brief Invalid value for parameter (download only). */
#define CANOPEN_SDO_ABORT_PARAMETER_VALUE_INVALID     0x06090030U
/** @brief Value of parameter written too high (download only). */
#define CANOPEN_SDO_ABORT_PARAMETER_VALUE_TOO_HIGH    0x06090031U
/** @brief Value of parameter written too low (download only). */
#define CANOPEN_SDO_ABORT_PARAMETER_VALUE_TOO_LOW     0x06090032U
/** @brief Maximum value is less than minimum value. */
#define CANOPEN_SDO_ABORT_MAX_LESS_THAN_MIN           0x06090036U
/** @brief Resource not available: SDO connection. */
#define CANOPEN_SDO_ABORT_RESOURCE_NOT_AVAILABLE      0x060A0023U
/** @brief General error. */
#define CANOPEN_SDO_ABORT_GENERAL_ERROR               0x08000000U
/** @brief Data cannot be transferred or stored to the application. */
#define CANOPEN_SDO_ABORT_APPLICATION_CANNOT_STORE    0x08000020U
/** @brief Data cannot be transferred or stored to the application because of local control. */
#define CANOPEN_SDO_ABORT_APPLICATION_LOCAL_CONTROL   0x08000021U
/** @brief Data cannot be transferred or stored to the application because of the device state. */
#define CANOPEN_SDO_ABORT_APPLICATION_DEVICE_STATE    0x08000022U
/** @brief Object dictionary dynamic generation fails or no object dictionary is present. */
#define CANOPEN_SDO_ABORT_NO_OBJECT_DICTIONARY        0x08000023U
/** @brief No data available. */
#define CANOPEN_SDO_ABORT_NO_DATA_AVAILABLE           0x08000024U

/** @} */

/**
 * @brief Minimum allowed value for a CANopen SDO number
 */
#define CANOPEN_SDO_NUMBER_MIN 1

/**
 * @brief Maximum allowed value for a CANopen SDO number
 */
#define CANOPEN_SDO_NUMBER_MAX 128

/** @cond INTERNAL_HIDDEN */
/**
 * @brief Internal representation of a CANopen SDO request.
 */
struct canopen_sdo_request {
	uint8_t data[8];
};
/** @endcond */

/** @brief CANopen Service Data Object (SDO) server
 *
 * This type is opaque. Member data should not be accessed directly by the application.
 */
struct canopen_sdo_server {
	/** State machine framework context (needs to be first). */
	struct smf_ctx ctx;
	/** Associated CAN interface. */
	const struct device *can;
	/** SDO number (1 to 128). */
	uint8_t sdo_number;
	/** Request queue buffer. */
	char requestq_buf[sizeof(struct canopen_sdo_request) *
			  CONFIG_CANOPEN_SDO_REQUEST_MSGQ_SIZE];
	/** Current request. */
	struct canopen_sdo_request request;
	/** Request queue. */
	struct k_msgq requestq;
	/** State machine processing work queue. */
	struct k_work_q *work_q;
	/** Request queue processing work queue item. */
	struct k_work_poll requestq_work;
	/** Request queue polling events. */
	struct k_poll_event requestq_poll_events[1];
};

/**
 * @brief Initialize CANopen SDO server
 *
 * The CANopen SDO server must be initialized prior to calling any other CANopen SDO server API
 * functions.
 *
 * @param server Pointer to the CANopen SDO server.
 * @param work_q Pointer to the work queue to be used by the CANopen SDO server.
 * @param can Pointer to the CAN controller device instance.
 * @param sdo_number SDO number (1 to 128).
 * @retval 0 on success.
 * @retval -EINVAL if the provided SDO number is invalid.
 * @retval -EIO if configuration of the CAN device failed.
 */
int canopen_sdo_server_init(struct canopen_sdo_server *server, struct k_work_q *work_q,
			    const struct device *can, uint8_t sdo_number);

/**
 * @brief Get the description of a CANopen SDO abort code
 *
 * @param abort_code CANopen SDO abort code which description should be returned.
 * @return String describing the provided abort code.
 */
const char *canopen_sdo_abort_code_str(uint32_t abort_code);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_CANBUS_CANOPEN_SDO_H_ */
