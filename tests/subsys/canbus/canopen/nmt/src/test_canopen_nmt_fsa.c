/*
 * Copyright (c) 2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/canbus/canopen/nmt.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/can/can_fake.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define FAKE_CAN_NAME               DEVICE_DT_NAME(DT_NODELABEL(fake_can))
#define NODE_ID                     127U
#define NMT_COB_ID                  (0x700U + NODE_ID)
#define MAX_STATE_TRANSITIONS       10
#define STATE_TRANSITION_TIMEOUT_MS 100

/* Node control protocol type used for testing */
enum node_control_protocol {
	NODE_CONTROL_PROTOCOL_LOCAL,
	NODE_CONTROL_PROTOCOL_REMOTE_NODE_ID,
	NODE_CONTROL_PROTOCOL_REMOTE_BROADCAST,
};

/* Remote node control protocol command specifiers */
enum node_control_cs {
	NODE_CONTROL_CS_START = 1,
	NODE_CONTROL_CS_STOP = 2,
	NODE_CONTROL_CS_ENTER_PRE_OPERATIONAL = 128,
	NODE_CONTROL_CS_RESET_NODE = 129,
	NODE_CONTROL_CS_RESET_COMMUNICATION = 130,
};

/* Structure for capturing NMT FSA state transitions */
struct state_transition_capture {
	struct canopen_nmt *nmt;
	struct canopen_nmt_state_callback *cb;
	enum canopen_nmt_state state;
	uint8_t node_id;
};

/* Global variables */
static struct canopen_nmt dut;
static const struct device *const fake_can_dev = DEVICE_DT_GET(DT_NODELABEL(fake_can));
static struct canopen_nmt_state_callback state_transition_cb;
K_MSGQ_DEFINE(state_transition_queue, sizeof(struct state_transition_capture),
	      MAX_STATE_TRANSITIONS, 1);
static struct can_frame frame_capture;
static bool frame_capture_no_ack;
static can_tx_callback_t frame_capture_callback;
static void *frame_capture_user_data;
static can_rx_callback_t frame_inject;
static void *frame_inject_user_data;
DEFINE_FFF_GLOBALS;

static int fake_can_send_delegate(const struct device *dev, const struct can_frame *frame,
				  k_timeout_t timeout, can_tx_callback_t callback, void *user_data)
{
	ARG_UNUSED(timeout);
	ARG_UNUSED(user_data);

	zassert_equal_ptr(dev, fake_can_dev);
	zassert_not_null(callback);

	frame_capture_callback = callback;
	frame_capture_user_data = user_data;

	memcpy(&frame_capture, frame, sizeof(frame_capture));

	if (!frame_capture_no_ack) {
		callback(dev, 0, user_data);
	}

	return 0;
}

static int fake_can_add_rx_filter_delegate(const struct device *dev, can_rx_callback_t callback,
					   void *user_data, const struct can_filter *filter)
{
	zassert_equal_ptr(dev, fake_can_dev);
	zassert_not_null(callback);

	zassert_equal(filter->id, 0U);
	zassert_equal(filter->mask, CAN_STD_ID_MASK);
	zassert_equal(filter->flags, 0U);

	frame_inject = callback;
	frame_inject_user_data = user_data;

	return 0;
}

static void install_fake_can_delegates(void)
{
	fake_can_add_rx_filter_fake.custom_fake = fake_can_add_rx_filter_delegate;
	fake_can_send_fake.custom_fake = fake_can_send_delegate;
}

static void state_transition_capture(struct canopen_nmt *nmt, struct canopen_nmt_state_callback *cb,
				     enum canopen_nmt_state state, uint8_t node_id)
{
	struct state_transition_capture capture;

	capture.nmt = nmt;
	capture.cb = cb;
	capture.state = state;
	capture.node_id = node_id;

	zassert_ok(k_msgq_put(&state_transition_queue, &capture, K_NO_WAIT));
}

static void verify_state_transitions(enum canopen_nmt_state *transitions, uint8_t num_transitions)
{
	struct state_transition_capture capture;
	bool boot_up_expected = false;

	zassert_true(num_transitions <= MAX_STATE_TRANSITIONS);

	for (int i = 0; i < num_transitions; i++) {
		zassert_ok(k_msgq_get(&state_transition_queue, &capture,
				      K_MSEC(STATE_TRANSITION_TIMEOUT_MS)));

		zassert_equal_ptr(capture.nmt, &dut);
		zassert_equal_ptr(capture.cb, &state_transition_cb);
		zassert_equal(capture.node_id, NODE_ID);
		zassert_equal(capture.state, transitions[i], "expected %s, captured %s",
			      canopen_nmt_state_str(transitions[i]),
			      canopen_nmt_state_str(capture.state));

		if (capture.state == CANOPEN_NMT_STATE_RESET_COMMUNICATION) {
			boot_up_expected = true;
		}
	}

	/* verify that there are no more queued transitions */
	zassert_not_ok(k_msgq_get(&state_transition_queue, &capture,
				  K_MSEC(STATE_TRANSITION_TIMEOUT_MS)));

	if (boot_up_expected) {
		/* Verify boot-up write took place */
		zassert_equal(fake_can_send_fake.call_count, 1);

		zassert_equal(frame_capture.id, NMT_COB_ID);
		zassert_equal(frame_capture.dlc, 1U);
		zassert_equal(frame_capture.flags, 0U);
		zassert_equal(frame_capture.data[0], 0U);

		/* Prepare for next boot-up write */
		RESET_FAKE(fake_can_send);
		install_fake_can_delegates();
	} else {
		/* Verify that no unexpected boot-up writes took place */
		zassert_equal(fake_can_send_fake.call_count, 0);
	}
}

static void verify_state_transition(enum canopen_nmt_state transition)
{
	enum canopen_nmt_state transitions[] = {transition};

	verify_state_transitions(transitions, ARRAY_SIZE(transitions));
}

static void verify_reset_node_transitions(void)
{
	enum canopen_nmt_state transitions[] = {
		CANOPEN_NMT_STATE_INITIALISATION,
		CANOPEN_NMT_STATE_RESET_APPLICATION,
		CANOPEN_NMT_STATE_RESET_COMMUNICATION,
		CANOPEN_NMT_STATE_PRE_OPERATIONAL,
	};

	verify_state_transitions(transitions, ARRAY_SIZE(transitions));
}

static void verify_reset_communication_transitions(void)
{
	enum canopen_nmt_state transitions[] = {
		CANOPEN_NMT_STATE_INITIALISATION,
		CANOPEN_NMT_STATE_RESET_COMMUNICATION,
		CANOPEN_NMT_STATE_PRE_OPERATIONAL,
	};

	verify_state_transitions(transitions, ARRAY_SIZE(transitions));
}

static void verify_no_state_transitions(void)
{
	verify_state_transitions(NULL, 0U);
}

static void node_control_local(enum node_control_cs cs)
{

	switch (cs) {
	case NODE_CONTROL_CS_START:
		zassert_ok(canopen_nmt_start(&dut));
		break;
	case NODE_CONTROL_CS_STOP:
		zassert_ok(canopen_nmt_stop(&dut));
		break;
	case NODE_CONTROL_CS_ENTER_PRE_OPERATIONAL:
		zassert_ok(canopen_nmt_enter_pre_operational(&dut));
		break;
	case NODE_CONTROL_CS_RESET_NODE:
		zassert_ok(canopen_nmt_reset_node(&dut));
		break;
	case NODE_CONTROL_CS_RESET_COMMUNICATION:
		zassert_ok(canopen_nmt_reset_communication(&dut));
		break;
	default:
		zassert_unreachable();
		break;
	}
}

static void node_control_remote(enum node_control_cs cs, bool broadcast)
{
	struct can_frame frame = {0};

	/* NMT node control CAN frame (CiA 301, section 7.2.8.3.1) */
	frame.dlc = 2U;
	frame.data[0] = cs;

	if (!broadcast) {
		frame.data[1] = NODE_ID;
	}

	frame_inject(fake_can_dev, &frame, frame_inject_user_data);
}

static void node_control_command(enum node_control_protocol protocol, enum node_control_cs cs)
{
	switch (protocol) {
	case NODE_CONTROL_PROTOCOL_LOCAL:
		node_control_local(cs);
		break;
	case NODE_CONTROL_PROTOCOL_REMOTE_NODE_ID:
		node_control_remote(cs, false);
		break;
	case NODE_CONTROL_PROTOCOL_REMOTE_BROADCAST:
		node_control_remote(cs, true);
		break;
	default:
		zassert_unreachable();
		break;
	}
}

static void test_node_control(enum node_control_protocol protocol)
{
	/* CiA 301, figure 48, transition (3) */
	node_control_command(protocol, NODE_CONTROL_CS_START);
	verify_state_transition(CANOPEN_NMT_STATE_OPERATIONAL);

	/* CiA 301, figure 48, transition (4) */
	node_control_command(protocol, NODE_CONTROL_CS_ENTER_PRE_OPERATIONAL);
	verify_state_transition(CANOPEN_NMT_STATE_PRE_OPERATIONAL);

	/* CiA 301, figure 48, transition (5) */
	node_control_command(protocol, NODE_CONTROL_CS_STOP);
	verify_state_transition(CANOPEN_NMT_STATE_STOPPED);

	/* CiA 301, figure 48, transition (6) */
	node_control_command(protocol, NODE_CONTROL_CS_START);
	verify_state_transition(CANOPEN_NMT_STATE_OPERATIONAL);

	/* CiA 301, figure 48, transition (7) */
	node_control_command(protocol, NODE_CONTROL_CS_STOP);
	verify_state_transition(CANOPEN_NMT_STATE_STOPPED);
	node_control_command(protocol, NODE_CONTROL_CS_ENTER_PRE_OPERATIONAL);
	verify_state_transition(CANOPEN_NMT_STATE_PRE_OPERATIONAL);

	/* CiA 301, figure 48, transition (8) */
	node_control_command(protocol, NODE_CONTROL_CS_STOP);
	verify_state_transition(CANOPEN_NMT_STATE_STOPPED);

	/* CiA 301, figure 48, transition (9) */
	node_control_command(protocol, NODE_CONTROL_CS_START);
	verify_state_transition(CANOPEN_NMT_STATE_OPERATIONAL);
	node_control_command(protocol, NODE_CONTROL_CS_RESET_NODE);
	verify_reset_node_transitions();

	/* CiA 301, figure 48, transition (10) */
	node_control_command(protocol, NODE_CONTROL_CS_STOP);
	verify_state_transition(CANOPEN_NMT_STATE_STOPPED);
	node_control_command(protocol, NODE_CONTROL_CS_RESET_NODE);
	verify_reset_node_transitions();

	/* CiA 301, figure 48, transition (11) */
	node_control_command(protocol, NODE_CONTROL_CS_RESET_NODE);
	verify_reset_node_transitions();

	/* CiA 301, figure 48, transition (12) */
	node_control_command(protocol, NODE_CONTROL_CS_START);
	verify_state_transition(CANOPEN_NMT_STATE_OPERATIONAL);
	node_control_command(protocol, NODE_CONTROL_CS_RESET_COMMUNICATION);
	verify_reset_communication_transitions();

	/* CiA 301, figure 48, transition (13) */
	node_control_command(protocol, NODE_CONTROL_CS_STOP);
	verify_state_transition(CANOPEN_NMT_STATE_STOPPED);
	node_control_command(protocol, NODE_CONTROL_CS_RESET_COMMUNICATION);
	verify_reset_communication_transitions();

	/* CiA 301, figure 48, transition (14) */
	node_control_command(protocol, NODE_CONTROL_CS_RESET_COMMUNICATION);
	verify_reset_communication_transitions();

	/* No state transistions from pre-operational to pre-operational */
	node_control_command(protocol, NODE_CONTROL_CS_ENTER_PRE_OPERATIONAL);
	verify_no_state_transitions();

	/* No state transistions from operational to operational */
	node_control_command(protocol, NODE_CONTROL_CS_START);
	verify_state_transition(CANOPEN_NMT_STATE_OPERATIONAL);
	node_control_command(protocol, NODE_CONTROL_CS_START);
	verify_no_state_transitions();

	/* No state transistions from stopped to stopped */
	node_control_command(protocol, NODE_CONTROL_CS_STOP);
	verify_state_transition(CANOPEN_NMT_STATE_STOPPED);
	node_control_command(protocol, NODE_CONTROL_CS_STOP);
	verify_no_state_transitions();
}

/**
 * @brief Verify NMT FSA transitions using local control.
 */
ZTEST(canopen_nmt_fsa, test_local_control)
{
	test_node_control(NODE_CONTROL_PROTOCOL_LOCAL);
}

/**
 * @brief Verify NMT FSA transitions using remote control by CANopen node-ID.
 */
ZTEST(canopen_nmt_fsa, test_remote_control_node_id)
{
	test_node_control(NODE_CONTROL_PROTOCOL_REMOTE_NODE_ID);
}

/**
 * @brief Verify NMT FSA transitions using remote control by CANopen NMT broadcast.
 */
ZTEST(canopen_nmt_fsa, test_remote_control_broadcast)
{
	test_node_control(NODE_CONTROL_PROTOCOL_REMOTE_BROADCAST);
}

/**
 * @brief Verify NMT FSA transitions when boot-up write receives a delayed CAN ACK.
 */
ZTEST(canopen_nmt_fsa, boot_up_write_delayed_ack)
{
	enum canopen_nmt_state transitions[] = {
		CANOPEN_NMT_STATE_INITIALISATION,
		CANOPEN_NMT_STATE_RESET_APPLICATION,
		CANOPEN_NMT_STATE_RESET_COMMUNICATION,
	};

	frame_capture_no_ack = true;

	zassert_ok(canopen_nmt_reset_node(&dut));
	verify_state_transitions(transitions, ARRAY_SIZE(transitions));

	/* Emulate boot-up write CAN frame ACK */
	zassert_not_null(frame_capture_callback);
	frame_capture_callback(fake_can_dev, 0, frame_capture_user_data);
	verify_state_transition(CANOPEN_NMT_STATE_PRE_OPERATIONAL);
}

/**
 * @brief Verify NMT FSA transitions when boot-up write receives no CAN ACK.
 */
ZTEST(canopen_nmt_fsa, boot_up_write_no_ack)
{
	enum canopen_nmt_state transitions1[] = {
		CANOPEN_NMT_STATE_INITIALISATION,
		CANOPEN_NMT_STATE_RESET_APPLICATION,
		CANOPEN_NMT_STATE_RESET_COMMUNICATION,
	};
	enum canopen_nmt_state transitions2[] = {
		CANOPEN_NMT_STATE_INITIALISATION,
		CANOPEN_NMT_STATE_RESET_COMMUNICATION,
	};

	frame_capture_no_ack = true;

	zassert_ok(canopen_nmt_reset_node(&dut));
	verify_state_transitions(transitions1, ARRAY_SIZE(transitions1));

	zassert_ok(canopen_nmt_reset_node(&dut));
	verify_state_transitions(transitions1, ARRAY_SIZE(transitions1));

	zassert_ok(canopen_nmt_reset_communication(&dut));
	verify_state_transitions(transitions2, ARRAY_SIZE(transitions2));

	/* Emulate boot-up write CAN frame ACK */
	zassert_not_null(frame_capture_callback);
	frame_capture_callback(fake_can_dev, 0, frame_capture_user_data);
	verify_state_transition(CANOPEN_NMT_STATE_PRE_OPERATIONAL);
}

/**
 * @brief Verify NMT FSA transitions when boot-up write returns error.
 */
ZTEST(canopen_nmt_fsa, boot_up_write_error)
{
	/* TODO: test boot-up write with error and retransmission attempt */
}

static void *canopen_nmt_fsa_setup(void)
{
	enum canopen_nmt_state transitions[] = {
		CANOPEN_NMT_STATE_INITIALISATION,
		CANOPEN_NMT_STATE_INITIALISING,
		CANOPEN_NMT_STATE_RESET_APPLICATION,
		CANOPEN_NMT_STATE_RESET_COMMUNICATION,
		CANOPEN_NMT_STATE_PRE_OPERATIONAL,
	};

	zassert_true(device_is_ready(fake_can_dev));

	install_fake_can_delegates();

	zassert_ok(canopen_nmt_init(&dut, &k_sys_work_q, fake_can_dev, NODE_ID));
	zassert_equal(fake_can_add_rx_filter_fake.call_count, 1);

	canopen_nmt_init_state_callback(&state_transition_cb, state_transition_capture);
	zassert_ok(canopen_nmt_add_state_callback(&dut, &state_transition_cb));

	/* CiA 301, figure 48, transitions (1) and (2) */
	zassert_ok(canopen_nmt_enable(&dut));
	verify_state_transitions(transitions, ARRAY_SIZE(transitions));

	return NULL;
}

static void canopen_nmt_fsa_before(void *fixture)
{
	k_msgq_purge(&state_transition_queue);

	memset(&frame_capture, 0, sizeof(frame_capture));
	frame_capture_callback = NULL;
	frame_capture_user_data = NULL;
	frame_capture_no_ack = false;

	install_fake_can_delegates();
}

ZTEST_SUITE(canopen_nmt_fsa, NULL, canopen_nmt_fsa_setup, canopen_nmt_fsa_before, NULL, NULL);
