/*
 * Copyright (c) 2021-2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/canbus/canopen.h>
#include <zephyr/canbus/canopen/sdo.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(canopen_sdo, CONFIG_CANOPEN_LOG_LEVEL);

/* SDO request/response protocol */
#define CANOPEN_SDO_DLC 8U

/* SDO request Client Command Specifier (CCS) */
#define CANOPEN_SDO_CCS_MASK                      GENMASK(7, 5)
#define CANOPEN_SDO_CCS_DOWNLOAD_SEGMENT_REQUEST  0U
#define CANOPEN_SDO_CCS_INITIATE_DOWNLOAD_REQUEST 1U
#define CANOPEN_SDO_CCS_INITIATE_UPLOAD_REQUEST   2U
#define CANOPEN_SDO_CCS_UPLOAD_SEGMENT_REQUEST    3U
#define CANOPEN_SDO_CCS_BLOCK_UPLOAD_REQUEST      5U
#define CANOPEN_SDO_CCS_BLOCK_DOWNLOAD_REQUEST    6U

/* SDO response Server Command Specifier (SCS) */
#define CANOPEN_SDO_SCS_MASK                       GENMASK(7, 5)
#define CANOPEN_SDO_SCS_UPLOAD_SEGMENT_RESPONSE    0U
#define CANOPEN_SDO_SCS_DOWNLOAD_SEGMENT_RESPONSE  1U
#define CANOPEN_SDO_SCS_INITIATE_UPLOAD_RESPONSE   2U
#define CANOPEN_SDO_SCS_INITIATE_DOWNLOAD_RESPONSE 3U
#define CANOPEN_SDO_SCS_BLOCK_DOWNLOAD_RESPONSE    5U
#define CANOPEN_SDO_SCS_BLOCK_UPLOAD_RESPONSE      6U

/* SDO request/response Command Specifier (CS) */
#define CANOPEN_SDO_CS_MASK                   GENMASK(7, 5)
#define CANOPEN_SDO_CS_ABORT_TRANSFER_REQUEST 4U

enum canopen_sdo_server_state {
	CANOPEN_SDO_SERVER_STATE_IDLE,
	CANOPEN_SDO_SERVER_STATE_DOWNLOAD,
	CANOPEN_SDO_SERVER_STATE_UPLOAD,
	CANOPEN_SDO_SERVER_STATE_BLOCK_DOWNLOAD,
	CANOPEN_SDO_SERVER_STATE_BLOCK_UPLOAD,
};

struct canopen_sdo_response {
	uint8_t data[8];
};

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

static inline uint8_t canopen_sdo_request_get_ccs(struct canopen_sdo_request *request)
{
	return FIELD_GET(CANOPEN_SDO_CCS_MASK, request->data[0]);
}

static inline uint16_t canopen_sdo_request_get_index(struct canopen_sdo_request *request)
{
	return sys_get_le16(&request->data[1]);
}

static inline uint8_t canopen_sdo_request_get_subindex(struct canopen_sdo_request *request)
{
	return request->data[3];
}

static inline void canopen_sdo_response_set_abort(struct canopen_sdo_response *response,
						  uint16_t index, uint8_t subindex,
						  uint32_t abort_code)
{
	response->data[0] = FIELD_PREP(CANOPEN_SDO_CS_MASK, CANOPEN_SDO_CS_ABORT_TRANSFER_REQUEST);

	sys_put_le16(index, &response->data[1]);
	response->data[3] = subindex;

	sys_put_le32(abort_code, &response->data[4]);
}

static void canopen_sdo_server_initiate_download(struct canopen_sdo_server *server,
						 struct canopen_sdo_response *response)
{
	/* TODO: check if expedited, send response */
	/* TODO: if not expedited, switch to download state */
	LOG_INF("initiate download");

	/* TODO: remove debug */
	response->data[0] = FIELD_PREP(CANOPEN_SDO_SCS_MASK,
				       CANOPEN_SDO_SCS_INITIATE_DOWNLOAD_RESPONSE);
	sys_put_le16(canopen_sdo_request_get_index(&server->request), &response->data[1]);
	response->data[3] = canopen_sdo_request_get_subindex(&server->request);
}

static void canopen_sdo_server_initiate_upload(struct canopen_sdo_server *server,
					       struct canopen_sdo_response *response)
{
	/* TODO: check if expedited, send response */
	/* TODO: if not expedited, switch to upload state */
	LOG_INF("initiate upload");

	/* TODO: remove debug */
	response->data[0] = FIELD_PREP(CANOPEN_SDO_SCS_MASK,
				       CANOPEN_SDO_SCS_INITIATE_UPLOAD_RESPONSE);
	response->data[0] |= BIT(1) | BIT(0); /* n = 0, e = 1, s = 1 = 32 bit expedited */
	sys_put_le16(canopen_sdo_request_get_index(&server->request), &response->data[1]);
	response->data[3] = canopen_sdo_request_get_subindex(&server->request);
	sys_put_le32(0xdeadbeef, &response->data[4]);
}

static void canopen_sdo_server_initiate_block_upload(struct canopen_sdo_server *server,
						     struct canopen_sdo_response *response)
{
	/* TODO: send response */
	LOG_INF("initiate block download");
}

static void canopen_sdo_server_initiate_block_download(struct canopen_sdo_server *server,
						       struct canopen_sdo_response *response)
{
	/* TODO: send response */
	LOG_INF("initiate block upload");
}

static enum smf_state_result canopen_sdo_server_state_idle_run(void *obj)
{
	struct canopen_sdo_server *server = obj;
	struct canopen_sdo_response response = {0};
	struct can_frame frame = {0};
	uint8_t ccs;
	int err;

	ccs = canopen_sdo_request_get_ccs(&server->request);

	LOG_INF("ccs = %u, index = %04xh, subindex = %u", ccs,
		canopen_sdo_request_get_index(&server->request),
		canopen_sdo_request_get_subindex(&server->request));

	switch (ccs) {
	case CANOPEN_SDO_CCS_INITIATE_DOWNLOAD_REQUEST:
		canopen_sdo_server_initiate_download(server, &response);
		break;
	case CANOPEN_SDO_CCS_INITIATE_UPLOAD_REQUEST:
		canopen_sdo_server_initiate_upload(server, &response);
		break;
	case CANOPEN_SDO_CCS_BLOCK_UPLOAD_REQUEST:
		canopen_sdo_server_initiate_block_upload(server, &response);
		break;
	case CANOPEN_SDO_CCS_BLOCK_DOWNLOAD_REQUEST:
		canopen_sdo_server_initiate_block_download(server, &response);
		break;
	default:
		LOG_DBG("invalid ccs %d, aborting", ccs);
		canopen_sdo_response_set_abort(&response, 0U, 0U,
					       CANOPEN_SDO_ABORT_CLIENT_SERVER_CMD_NOT_VALID);
		break;
	}

	/* TODO: use COB-ID from OD */
	frame.id = 0x5ff;
	frame.dlc = CANOPEN_SDO_DLC;
	memcpy(frame.data, response.data, sizeof(response.data));

	/* TODO: use async callback and not forever */
	err = can_send(server->can, &frame, K_FOREVER, NULL, NULL);
	if (err != 0) {
		LOG_ERR("failed to enqueue SDO response (err %d)", err);
		/* TODO: alert application/EMCY */
	}

	return SMF_EVENT_HANDLED;
}

/* CANopen SDO server state table */
static const struct smf_state canopen_sdo_server_states[] = {
	[CANOPEN_SDO_SERVER_STATE_IDLE] =
		SMF_CREATE_STATE(NULL, canopen_sdo_server_state_idle_run, NULL, NULL, NULL),
	[CANOPEN_SDO_SERVER_STATE_DOWNLOAD] =
		SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL),
	[CANOPEN_SDO_SERVER_STATE_UPLOAD] =
		SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL),
	[CANOPEN_SDO_SERVER_STATE_BLOCK_DOWNLOAD] =
		SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL),
	[CANOPEN_SDO_SERVER_STATE_BLOCK_UPLOAD] =
		SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL),
};

static void canopen_sdo_server_request_callback(const struct device *can, struct can_frame *frame,
						void *user_data)
{
	struct canopen_sdo_server *server = user_data;
	struct canopen_sdo_request request;
	int err;

	ARG_UNUSED(can);

	if (frame->dlc != CANOPEN_SDO_DLC) {
		/* Non-compliant frame length, ignore */
		return;
	}

	memcpy(request.data, frame->data, sizeof(request.data));

	err = k_msgq_put(&server->requestq, &request, K_NO_WAIT);
	if (err != 0) {
		LOG_ERR("failed to enqueue SDO request (err %d)", err);
	}
}

static void canopen_sdo_server_requestq_triggered_work_handler(struct k_work *work)
{
	struct k_work_poll *pwork = CONTAINER_OF(work, struct k_work_poll, work);
	struct canopen_sdo_server *server =
		CONTAINER_OF(pwork, struct canopen_sdo_server, requestq_work);
	int err;

	err = k_msgq_get(&server->requestq, &server->request, K_FOREVER);
	if (err != 0) {
		LOG_ERR("failed to get SDO request from queue (err %d)", err);
		goto resubmit;
	}

	err = smf_run_state(SMF_CTX(server));
	if (err != 0) {
		LOG_ERR("SDO server finite-state machine terminated (err %d)", err);
		goto resubmit;
	}

resubmit:
	err = k_work_poll_submit_to_queue(server->work_q, &server->requestq_work,
					  server->requestq_poll_events,
					  ARRAY_SIZE(server->requestq_poll_events), K_FOREVER);
	if (err != 0) {
		LOG_ERR("failed to re-submit SDO request queue polling (err %d)", err);
	}
}

int canopen_sdo_server_init(struct canopen_sdo_server *server, struct k_work_q *work_q,
			    const struct device *can, uint8_t sdo_number)
{
	struct can_filter filter;
	int err;

	__ASSERT_NO_MSG(server != NULL);
	__ASSERT_NO_MSG(work_q != NULL);
	__ASSERT_NO_MSG(can != NULL);

	if (sdo_number < CANOPEN_SDO_NUMBER_MIN || sdo_number > CANOPEN_SDO_NUMBER_MAX) {
		LOG_ERR("invalid SDO number %d", sdo_number);
		return -EINVAL;
	}

	server->work_q = work_q;
	server->can = can;
	server->sdo_number = sdo_number;

	k_msgq_init(&server->requestq, server->requestq_buf, sizeof(struct canopen_sdo_request),
		    CONFIG_CANOPEN_SDO_REQUEST_MSGQ_SIZE);

	k_poll_event_init(&server->requestq_poll_events[0], K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
			  K_POLL_MODE_NOTIFY_ONLY, &server->requestq);

	k_work_poll_init(&server->requestq_work,
			 canopen_sdo_server_requestq_triggered_work_handler);

	err = k_work_poll_submit_to_queue(server->work_q, &server->requestq_work,
					  server->requestq_poll_events,
					  ARRAY_SIZE(server->requestq_poll_events), K_FOREVER);
	if (err != 0) {
		LOG_ERR("failed to submit SDO request queue polling (err %d)", err);
		return -EIO;
	}

	/* TODO: move to canopen_sdo_server_enable() */
	smf_set_initial(SMF_CTX(server), &canopen_sdo_server_states[CANOPEN_SDO_SERVER_STATE_IDLE]);

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
