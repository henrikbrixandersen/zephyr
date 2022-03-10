/*
 * Copyright (c) 2022-2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/buf.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/gs_usb.h>

#include <usb_descriptor.h>

LOG_MODULE_REGISTER(gs_usb, CONFIG_USB_DEVICE_GS_USB_LOG_LEVEL);

#define DT_DRV_COMPAT zephyr_gs_usb

/* Software/hardware versions */
#define GS_USB_SW_VERSION 2U
#define GS_USB_HW_VERSION 1U

/* USB requests */
enum {
	GS_USB_REQUEST_HOST_FORMAT = 0,
	GS_USB_REQUEST_BITTIMING,
	GS_USB_REQUEST_MODE,
	GS_USB_REQUEST_BERR,            /* Not supported */
	GS_USB_REQUEST_BT_CONST,
	GS_USB_REQUEST_DEVICE_CONFIG,
	GS_USB_REQUEST_TIMESTAMP,
	GS_USB_REQUEST_IDENTIFY,
	GS_USB_REQUEST_GET_USER_ID,     /* Not supported */
	GS_USB_REQUEST_SET_USER_ID,     /* Not supported */
	GS_USB_REQUEST_DATA_BITTIMING,
	GS_USB_REQUEST_BT_CONST_EXT,
	GS_USB_REQUEST_SET_TERMINATION,
	GS_USB_REQUEST_GET_TERMINATION,
	GS_USB_REQUEST_GET_STATE,
};

/* CAN channel modes */
enum {
	GS_USB_CHANNEL_MODE_RESET = 0,
	GS_USB_CHANNEL_MODE_START,
};

/* CAN channel states */
enum {
	GS_USB_CHANNEL_STATE_ERROR_ACTIVE = 0,
	GS_USB_CHANNEL_STATE_ERROR_WARNING,
	GS_USB_CHANNEL_STATE_ERROR_PASSIVE,
	GS_USB_CHANNEL_STATE_BUS_OFF,
	GS_USB_CHANNEL_STATE_STOPPED,
	GS_USB_CHANNEL_STATE_SLEEPING,
};

/* CAN channel identify modes */
enum {
	GS_USB_CHANNEL_IDENTIFY_MODE_OFF = 0,
	GS_USB_CHANNEL_IDENTIFY_MODE_ON,
};

/* CAN channel termination states */
enum {
	GS_USB_CHANNEL_TERMINATION_STATE_OFF = 0,
	GS_USB_CHANNEL_TERMINATION_STATE_ON,
};

/* CAN channel features */
#define GS_US_CAN_FEATURE_LISTEN_ONLY              BIT(0)
#define GS_US_CAN_FEATURE_LOOP_BACK                BIT(1)
#define GS_US_CAN_FEATURE_TRIPLE_SAMPLE            BIT(2)
#define GS_US_CAN_FEATURE_ONE_SHOT                 BIT(3)
#define GS_US_CAN_FEATURE_HW_TIMESTAMP             BIT(4)
#define GS_US_CAN_FEATURE_IDENTIFY                 BIT(5)
#define GS_US_CAN_FEATURE_USER_ID                  BIT(6)
#define GS_US_CAN_FEATURE_PAD_PKTS_TO_MAX_PKT_SIZE BIT(7) /* Unsupported */
#define GS_US_CAN_FEATURE_FD                       BIT(8)
#define GS_US_CAN_FEATURE_REQ_USB_QUIRK_LPC546XX   BIT(9) /* Unused */
#define GS_US_CAN_FEATURE_BT_CONST_EXT             BIT(10)
#define GS_US_CAN_FEATURE_TERMINATION              BIT(11)
#define GS_US_CAN_FEATURE_BERR_REPORTING           BIT(12)
#define GS_US_CAN_FEATURE_GET_STATE                BIT(13)

/* CAN channel flags (bit positions match corresponding channel feature bits) */
#define GS_USB_CAN_MODE_NORMAL                   0U
#define GS_USB_CAN_MODE_LISTEN_ONLY              BIT(0)
#define GS_USB_CAN_MODE_LOOP_BACK                BIT(1)
#define GS_USB_CAN_MODE_TRIPLE_SAMPLE            BIT(2)
#define GS_USB_CAN_MODE_ONE_SHOT                 BIT(3)
#define GS_USB_CAN_MODE_HW_TIMESTAMP             BIT(4)
#define GS_USB_CAN_MODE_PAD_PKTS_TO_MAX_PKT_SIZE BIT(7)
#define GS_USB_CAN_MODE_FD                       BIT(8)
#define GS_USB_CAN_MODE_BERR_REPORTING           BIT(12)

/* Host frame CAN flags */
#define GS_USB_CAN_FLAG_OVERFLOW BIT(0)
#define GS_USB_CAN_FLAG_FD       BIT(1)
#define GS_USB_CAN_FLAG_BRS      BIT(2)
#define GS_USB_CAN_FLAG_ESI      BIT(3)

/* CAN ID flags */
#define GS_USB_CAN_ID_FLAG_ERR BIT(29)
#define GS_USB_CAN_ID_FLAG_RTR BIT(30)
#define GS_USB_CAN_ID_FLAG_IDE BIT(31)

/* Supported host byte order formats */
#define GS_USB_HOST_FORMAT_LITTLE_ENDIAN 0x0000beef

/* Host frame echo ID for RX frames */
#define GS_USB_HOST_FRAME_ECHO_ID_RX_FRAME UINT32_MAX

struct gs_usb_host_config {
	uint32_t byte_order;
} __packed;

struct gs_usb_device_config {
	uint8_t reserved1;
	uint8_t reserved2;
	uint8_t reserved3;
	uint8_t nchannels;
	uint32_t sw_version;
	uint32_t hw_version;
} __packed;

struct gs_usb_device_mode {
	uint32_t mode;
	uint32_t flags;
} __packed;

struct gs_usb_device_state {
	uint32_t state;
	uint32_t rxerr;
	uint32_t txerr;
} __packed;

struct gs_usb_device_bittiming {
	uint32_t prop_seg;
	uint32_t phase_seg1;
	uint32_t phase_seg2;
	uint32_t sjw;
	uint32_t brp;
} __packed;

struct gs_usb_identify_mode {
	uint32_t mode;
} __packed;

struct gs_usb_device_termination_state {
	uint32_t state;
} __packed;

struct gs_usb_device_bt_const {
	uint32_t feature;
	uint32_t fclk_can;
	uint32_t tseg1_min;
	uint32_t tseg1_max;
	uint32_t tseg2_min;
	uint32_t tseg2_max;
	uint32_t sjw_max;
	uint32_t brp_min;
	uint32_t brp_max;
	uint32_t brp_inc;
} __packed;

struct gs_usb_device_bt_const_ext {
	uint32_t feature;
	uint32_t fclk_can;
	uint32_t tseg1_min;
	uint32_t tseg1_max;
	uint32_t tseg2_min;
	uint32_t tseg2_max;
	uint32_t sjw_max;
	uint32_t brp_min;
	uint32_t brp_max;
	uint32_t brp_inc;
	uint32_t dtseg1_min;
	uint32_t dtseg1_max;
	uint32_t dtseg2_min;
	uint32_t dtseg2_max;
	uint32_t dsjw_max;
	uint32_t dbrp_min;
	uint32_t dbrp_max;
	uint32_t dbrp_inc;
} __packed;

struct gs_usb_can_frame {
	uint8_t data[8];
} __packed __aligned(4);

struct gs_usb_canfd_frame {
	uint8_t data[64];
} __packed __aligned(4);

struct gs_usb_host_frame_hdr {
	uint32_t echo_id;
	uint32_t can_id;
	uint8_t can_dlc;
	uint8_t channel;
	uint8_t flags;
	uint8_t reserved;
} __packed __aligned(4);

/* USB endpoint addresses
 *
 * Existing drivers expect endpoints 0x81 and 0x02. Include a dummy endpoint
 * 0x01 to work-around the endpoint address fixup.
 */
#define GS_USB_IN_EP_ADDR    0x81
#define GS_USB_DUMMY_EP_ADDR 0x01
#define GS_USB_OUT_EP_ADDR   0x02

/* USB endpoint indexes */
#define GS_USB_IN_EP_IDX  0U
#define GS_USB_DUMMY_EP_IDX  1U
#define GS_USB_OUT_EP_IDX 2U

/* Host frame sizes */
#define GS_USB_HOST_FRAME_CAN_FRAME_SIZE \
	(sizeof(struct gs_usb_host_frame_hdr) + sizeof(struct gs_usb_can_frame))
#define GS_USB_HOST_FRAME_CANFD_FRAME_SIZE \
	(sizeof(struct gs_usb_host_frame_hdr) + sizeof(struct gs_usb_canfd_frame))
#define GS_USB_HOST_FRAME_ECHO_FRAME_SIZE sizeof(struct gs_usb_host_frame_hdr)

/* TODO: increase max size for HW timestamp support */
#ifdef CONFIG_CAN_FD_MODE
#define GS_USB_HOST_FRAME_MAX_SIZE GS_USB_HOST_FRAME_CANFD_FRAME_SIZE
#else /* CONFIG_CAN_FD_MODE */
#define GS_USB_HOST_FRAME_MAX_SIZE GS_USB_HOST_FRAME_CAN_FRAME_SIZE
#endif /* !CONFIG_CAN_FD_MODE */

struct gs_usb_config {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_in_ep;
	struct usb_ep_descriptor if0_dummy_ep;
	struct usb_ep_descriptor if0_out_ep;
} __packed;

struct gs_usb_channel_data {
	const struct device *dev;
	uint32_t supported_features;
	uint32_t enabled_features;
	uint32_t mode_base;
	uint16_t ch;
};

struct gs_usb_data {
	struct usb_dev_data common;
	struct gs_usb_channel_data channels[CONFIG_USB_DEVICE_GS_USB_MAX_CHANNELS];
	size_t nchannels;
	struct gs_usb_ops ops;
	void *user_data;
	struct net_buf_pool *pool;

	struct k_fifo rx_fifo;
	struct k_thread rx_thread;

	uint8_t tx_buffer[GS_USB_HOST_FRAME_MAX_SIZE];
	struct k_fifo tx_fifo;
	struct k_thread tx_thread;

	K_KERNEL_STACK_MEMBER(rx_stack, CONFIG_USB_DEVICE_GS_USB_RX_THREAD_STACK_SIZE);
	K_KERNEL_STACK_MEMBER(tx_stack, CONFIG_USB_DEVICE_GS_USB_TX_THREAD_STACK_SIZE);
};

static sys_slist_t gs_usb_data_devlist;

static void gs_usb_transfer_tx_prepare(const struct device *dev);

static void gs_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
	struct usb_if_descriptor *if_desc = (struct usb_if_descriptor *)head;

	LOG_DBG("bInterfaceNumber = %u", bInterfaceNumber);
	if_desc->bInterfaceNumber = bInterfaceNumber;
}

static void gs_usb_status_callback(struct usb_cfg_data *cfg, enum usb_dc_status_code status,
				   const uint8_t *param)
{
	struct usb_dev_data *common;
	struct gs_usb_data *data;
	int ch;

	common = usb_get_dev_data_by_cfg(&gs_usb_data_devlist, cfg);
	if (common == NULL) {
		LOG_ERR("device data not found for cfg %p", cfg);
		return;
	}

	data = CONTAINER_OF(common, struct gs_usb_data, common);

	switch (status) {
	case USB_DC_ERROR:
		LOG_DBG("USB device error");
		break;
	case USB_DC_RESET:
		LOG_DBG("USB device reset");
		break;
	case USB_DC_CONNECTED:
		LOG_DBG("USB device connected");
		break;
	case USB_DC_CONFIGURED:
		LOG_DBG("USB device configured");
		LOG_DBG("EP IN addr = 0x%02x", cfg->endpoint[GS_USB_IN_EP_IDX].ep_addr);
		LOG_DBG("EP DUMMY addr = 0x%02x", cfg->endpoint[GS_USB_DUMMY_EP_IDX].ep_addr);
		LOG_DBG("EP OUT addr = 0x%02x", cfg->endpoint[GS_USB_OUT_EP_IDX].ep_addr);
		gs_usb_transfer_tx_prepare(common->dev);
		break;
	case USB_DC_DISCONNECTED:
		LOG_DBG("USB device disconnected");
		for (ch = 0; ch < data->nchannels; ch++) {
			(void)can_stop(data->channels[ch].dev);
		}

		usb_cancel_transfer(cfg->endpoint[GS_USB_IN_EP_IDX].ep_addr);
		usb_cancel_transfer(cfg->endpoint[GS_USB_OUT_EP_IDX].ep_addr);
		break;
	case USB_DC_SUSPEND:
		LOG_DBG("USB device suspend");
		break;
	case USB_DC_RESUME:
		LOG_DBG("USB device resume");
		break;
	case USB_DC_INTERFACE:
		LOG_DBG("USB device interface selected");
		break;
	case USB_DC_SET_HALT:
		LOG_DBG("USB device set halt");
		break;
	case USB_DC_CLEAR_HALT:
		LOG_DBG("USB device clear halt");
		break;
	case USB_DC_SOF:
		LOG_DBG("USB device start-of-frame");
		/* TODO: not called by all UDCs? select CONFIG_USB_DEVICE_SOF */
		/* TODO: capture HW timestamp */
		break;
	case USB_DC_UNKNOWN:
		__fallthrough;
	default:
		LOG_ERR("USB device unknown state");
		break;
	}
}

static int gs_usb_request_host_format(int32_t tlen, const uint8_t *tdata)
{
	struct gs_usb_host_config *payload = (struct gs_usb_host_config *)tdata;
	uint32_t byte_order;

	if (tlen != sizeof(*payload)) {
		LOG_ERR("invalid length for host format request (%d)", tlen);
		return -EINVAL;
	}

	byte_order = sys_le32_to_cpu(payload->byte_order);

	if (byte_order != GS_USB_HOST_FORMAT_LITTLE_ENDIAN) {
		LOG_ERR("unsupported host byte order (0x%08x)", byte_order);
		return -ENOTSUP;
	}

	return 0;
}

static int gs_usb_request_bt_const(const struct device *dev, uint16_t ch, int32_t *tlen,
				   uint8_t **tdata)
{
	struct gs_usb_device_bt_const *payload = (struct gs_usb_device_bt_const *)*tdata;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	const struct can_timing *min;
	const struct can_timing *max;
	uint32_t rate;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("bt_const request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	err = can_get_core_clock(channel->dev, &rate);
	if (err != 0U) {
		LOG_ERR("failed to get core clock for channel %u (err %d)", ch, err);
		return err;
	}

	min = can_get_timing_min(channel->dev);
	max = can_get_timing_max(channel->dev);

	payload->feature = sys_cpu_to_le32(channel->supported_features);
	payload->fclk_can = sys_cpu_to_le32(rate);
	payload->tseg1_min = sys_cpu_to_le32(min->prop_seg + min->phase_seg1);
	payload->tseg1_max = sys_cpu_to_le32(max->prop_seg + max->phase_seg1);
	payload->tseg2_min = sys_cpu_to_le32(min->phase_seg2);
	payload->tseg2_max = sys_cpu_to_le32(max->phase_seg1);
	payload->sjw_max = sys_cpu_to_le32(max->sjw);
	payload->brp_min = sys_cpu_to_le32(min->prescaler);
	payload->brp_max = sys_cpu_to_le32(max->prescaler);
	payload->brp_inc = 1U;

	*tlen = sizeof(*payload);

	return 0;
}

#ifdef CONFIG_CAN_FD_MODE
static int gs_usb_request_bt_const_ext(const struct device *dev, uint16_t ch, int32_t *tlen,
				       uint8_t **tdata)
{
	struct gs_usb_device_bt_const_ext *payload = (struct gs_usb_device_bt_const_ext *)*tdata;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	const struct can_timing *min;
	const struct can_timing *max;
	uint32_t rate;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("bt_const_ext request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	err = can_get_core_clock(channel->dev, &rate);
	if (err != 0U) {
		LOG_ERR("failed to get core clock for channel %u (err %d)", ch, err);
		return err;
	}

	min = can_get_timing_min(channel->dev);
	max = can_get_timing_max(channel->dev);

	payload->feature = sys_cpu_to_le32(channel->supported_features);
	payload->fclk_can = sys_cpu_to_le32(rate);

	payload->tseg1_min = sys_cpu_to_le32(min->prop_seg + min->phase_seg1);
	payload->tseg1_max = sys_cpu_to_le32(max->prop_seg + max->phase_seg1);
	payload->tseg2_min = sys_cpu_to_le32(min->phase_seg2);
	payload->tseg2_max = sys_cpu_to_le32(max->phase_seg1);
	payload->sjw_max = sys_cpu_to_le32(max->sjw);
	payload->brp_min = sys_cpu_to_le32(min->prescaler);
	payload->brp_max = sys_cpu_to_le32(max->prescaler);
	payload->brp_inc = 1U;

	min = can_get_timing_data_min(channel->dev);
	max = can_get_timing_data_max(channel->dev);

	if (min == NULL || max == NULL) {
		LOG_ERR("failed to get min/max data phase timing for channel %u", ch);
		return -ENOTSUP;
	};

	payload->dtseg1_min = sys_cpu_to_le32(min->prop_seg + min->phase_seg1);
	payload->dtseg1_max = sys_cpu_to_le32(max->prop_seg + max->phase_seg1);
	payload->dtseg2_min = sys_cpu_to_le32(min->phase_seg2);
	payload->dtseg2_max = sys_cpu_to_le32(max->phase_seg1);
	payload->dsjw_max = sys_cpu_to_le32(max->sjw);
	payload->dbrp_min = sys_cpu_to_le32(min->prescaler);
	payload->dbrp_max = sys_cpu_to_le32(max->prescaler);
	payload->dbrp_inc = 1U;

	*tlen = sizeof(*payload);

	return 0;
}
#endif /* CONFIG_CAN_FD_MODE */

static int gs_usb_request_get_termination(const struct device *dev, uint16_t ch, int32_t *tlen,
					  uint8_t **tdata)
{
	struct gs_usb_device_termination_state *payload =
		(struct gs_usb_device_termination_state *)*tdata;
	struct gs_usb_data *data = dev->data;
	bool terminated;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("get_termination request for non-existing channel %u", ch);
		return -EINVAL;
	}

	if (data->ops.get_termination == NULL) {
		LOG_ERR("get termination not supported");
		return -ENOTSUP;
	}

	err = data->ops.get_termination(dev, ch, &terminated, data->user_data);
	if (err != 0U) {
		LOG_ERR("failed to get termination state for channel %u (err %d)", ch, err);
		return err;
	}

	if (terminated) {
		payload->state = sys_cpu_to_le32(GS_USB_CHANNEL_TERMINATION_STATE_ON);
	} else {
		payload->state = sys_cpu_to_le32(GS_USB_CHANNEL_TERMINATION_STATE_OFF);
	}

	*tlen = sizeof(*payload);

	return 0;
}

static int gs_usb_request_set_termination(const struct device *dev, uint16_t ch, int32_t tlen,
					  uint8_t *tdata)
{
	struct gs_usb_device_termination_state *payload =
		(struct gs_usb_device_termination_state *)tdata;
	struct gs_usb_data *data = dev->data;
	uint32_t state;
	bool terminate;

	if (ch >= data->nchannels) {
		LOG_ERR("set termination request for non-existing channel %u", ch);
		return -EINVAL;
	}

	if (data->ops.set_termination == NULL) {
		LOG_ERR("set termination not supported");
		return -ENOTSUP;
	}

	if (tlen != sizeof(*payload)) {
		LOG_ERR("invalid length for set termination request (%d)", tlen);
		return -EINVAL;
	}

	state = sys_le32_to_cpu(payload->state);

	switch (state) {
	case GS_USB_CHANNEL_TERMINATION_STATE_OFF:
		terminate = false;
		break;
	case GS_USB_CHANNEL_TERMINATION_STATE_ON:
		terminate = true;
		break;
	default:
		LOG_ERR("unsupported set termination state %d for channel %u", state, ch);
		return -ENOTSUP;
	}

	return data->ops.set_termination(dev, ch, terminate, data->user_data);
}

static int gs_usb_request_get_state(const struct device *dev, uint16_t ch, int32_t *tlen,
				    uint8_t **tdata)
{
	struct gs_usb_device_state *payload = (struct gs_usb_device_state *)*tdata;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	struct can_bus_err_cnt err_cnt;
	enum can_state state;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("get_state request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	err = can_get_state(channel->dev, &state, &err_cnt);
	if (err != 0U) {
		LOG_ERR("failed to get state for channel %u (err %d)", ch, err);
		return err;
	}

	payload->rxerr = sys_cpu_to_le32(err_cnt.rx_err_cnt);
	payload->txerr = sys_cpu_to_le32(err_cnt.tx_err_cnt);

	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		payload->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_ERROR_ACTIVE);
		break;
	case CAN_STATE_ERROR_WARNING:
		payload->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_ERROR_WARNING);
		break;
	case CAN_STATE_ERROR_PASSIVE:
		payload->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_ERROR_PASSIVE);
		break;
	case CAN_STATE_BUS_OFF:
		payload->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_BUS_OFF);
		break;
	case CAN_STATE_STOPPED:
		payload->state = sys_cpu_to_le32(GS_USB_CHANNEL_STATE_STOPPED);
		break;
	default:
		LOG_ERR("unsupported state %d for channel %u", state, ch);
		return -ENOTSUP;
	}

	*tlen = sizeof(*payload);

	return 0;
}

static void gs_usb_bittiming_to_can_timing(const struct gs_usb_device_bittiming *dbt,
					   const struct can_timing *min,
					   const struct can_timing *max, struct can_timing *result)
{
	memset(result, 0U, sizeof(*result));

	result->sjw = dbt->sjw;
	result->prop_seg = dbt->prop_seg;
	result->phase_seg1 = dbt->phase_seg1;
	result->phase_seg2 = dbt->phase_seg2;
	result->prescaler = dbt->brp;

	if (result->prop_seg < min->prop_seg) {
		/* Move TQs from phase segment 1 to propagation segment */
		result->phase_seg1 -= (min->prop_seg - result->prop_seg);
		result->prop_seg = min->prop_seg;
	} else if (result->prop_seg > max->prop_seg) {
		/* Move TQs from propagation segment to phase segment 1 */
		result->phase_seg1 += result->prop_seg - max->prop_seg;
		result->prop_seg = max->prop_seg;
	}

	if (result->phase_seg1 < min->phase_seg1) {
		/* Move TQs from propagation segment to phase segment 1 */
		result->prop_seg -= (min->phase_seg1 - result->phase_seg1);
		result->phase_seg1 = min->phase_seg1;
	} else if (result->phase_seg1 > max->phase_seg1) {
		/* Move TQs from phase segment 1 to propagation segment */
		result->prop_seg += result->phase_seg1 - max->phase_seg1;
		result->phase_seg1 = max->phase_seg1;
	}

	LOG_DBG("request: prop_seg %u, phase_seg1 %u, phase_seq2 %u, sjw %u, brp %u", dbt->prop_seg,
		dbt->phase_seg1, dbt->phase_seg2, dbt->sjw, dbt->brp);
	LOG_DBG("result: prop_seg %u, phase_seg1 %u, phase_seq2 %u, sjw %u, brp %u",
		result->prop_seg, result->phase_seg1, result->phase_seg2, result->sjw,
		result->prescaler);
};

static int gs_usb_request_bittiming(const struct device *dev, uint16_t ch, int32_t tlen,
				    uint8_t *tdata)
{
	struct gs_usb_device_bittiming *payload = (struct gs_usb_device_bittiming *)tdata;
	struct gs_usb_device_bittiming dbt;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	const struct can_timing *min;
	const struct can_timing *max;
	struct can_timing timing;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("bittiming request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	if (tlen != sizeof(*payload)) {
		LOG_ERR("invalid length for bittiming request (%d)", tlen);
		return -EINVAL;
	}

	dbt.prop_seg = sys_le32_to_cpu(payload->prop_seg);
	dbt.phase_seg1 = sys_le32_to_cpu(payload->phase_seg1);
	dbt.phase_seg2 = sys_le32_to_cpu(payload->phase_seg2);
	dbt.sjw = sys_le32_to_cpu(payload->sjw);
	dbt.brp = sys_le32_to_cpu(payload->brp);

	min = can_get_timing_min(channel->dev);
	max = can_get_timing_max(channel->dev);

	gs_usb_bittiming_to_can_timing(payload, min, max, &timing);

	err = can_set_timing(channel->dev, &timing);
	if (err != 0U) {
		LOG_ERR("failed to set timing for channel %u (err %d)", ch, err);
		return err;
	}

	return 0;
}

#ifdef CONFIG_CAN_FD_MODE
static int gs_usb_request_data_bittiming(const struct device *dev, uint16_t ch, int32_t tlen,
					 uint8_t *tdata)
{
	struct gs_usb_device_bittiming *payload = (struct gs_usb_device_bittiming *)tdata;
	struct gs_usb_device_bittiming dbt;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	const struct can_timing *min;
	const struct can_timing *max;
	struct can_timing timing_data;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("data_bittiming request for non-existing channel %u", ch);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	if (tlen != sizeof(*payload)) {
		LOG_ERR("invalid length for data_bittiming request (%d)", tlen);
		return -EINVAL;
	}

	dbt.prop_seg = sys_le32_to_cpu(payload->prop_seg);
	dbt.phase_seg1 = sys_le32_to_cpu(payload->phase_seg1);
	dbt.phase_seg2 = sys_le32_to_cpu(payload->phase_seg2);
	dbt.sjw = sys_le32_to_cpu(payload->sjw);
	dbt.brp = sys_le32_to_cpu(payload->brp);

	min = can_get_timing_data_min(channel->dev);
	max = can_get_timing_data_max(channel->dev);

	if (min == NULL || max == NULL) {
		LOG_ERR("failed to get min/max data phase timing for channel %u", ch);
		return -ENOTSUP;
	};

	gs_usb_bittiming_to_can_timing(&dbt, min, max, &timing_data);

	err = can_set_timing_data(channel->dev, &timing_data);
	if (err != 0U) {
		LOG_ERR("failed to set data phase timing for channel %u (err %d)", ch, err);
		return err;
	}

	return 0;
}
#endif /* CONFIG_CAN_FD_MODE */

static int gs_usb_request_mode(const struct device *dev, uint16_t ch, int32_t tlen, uint8_t *tdata)
{
	struct gs_usb_device_mode *payload = (struct gs_usb_device_mode *)tdata;
	struct gs_usb_data *data = dev->data;
	struct gs_usb_channel_data *channel;
	can_mode_t mode = CAN_MODE_NORMAL;
	uint32_t flags;
	int err;

	if (ch >= data->nchannels) {
		LOG_ERR("mode request for non-existing channel %u", ch);
		return -EINVAL;
	}

	if (tlen != sizeof(*payload)) {
		LOG_ERR("invalid length for mode request (%d)", tlen);
		return -EINVAL;
	}

	channel = &data->channels[ch];

	switch (sys_le32_to_cpu(payload->mode)) {
	case GS_USB_CHANNEL_MODE_RESET:
		/* TODO: flush TX FIFO, unref net_bufs */
		err = can_stop(channel->dev);
		if (err != 0U && err != -EALREADY) {
			LOG_ERR("failed to stop channel %u (err %d)", ch, err);
			return err;
		}
		channel->enabled_features = 0U;
		break;
	case GS_USB_CHANNEL_MODE_START:
		flags = sys_le32_to_cpu(payload->flags);
		mode |= channel->mode_base;

		if ((flags & ~(channel->supported_features)) != 0U) {
			LOG_ERR("unsupported flags 0x%08x for channel %u", flags, ch);
			return -ENOTSUP;
		}

		if ((flags & GS_USB_CAN_MODE_LISTEN_ONLY) != 0U) {
			mode |= CAN_MODE_LISTENONLY;
		}

		if ((flags & GS_USB_CAN_MODE_LOOP_BACK) != 0U) {
			mode |= CAN_MODE_LOOPBACK;
		}

		if ((flags & GS_USB_CAN_MODE_TRIPLE_SAMPLE) != 0U) {
			mode |= CAN_MODE_3_SAMPLES;
		}

		if ((flags & GS_USB_CAN_MODE_ONE_SHOT) != 0U) {
			mode |= CAN_MODE_ONE_SHOT;
		}

		if ((flags & GS_USB_CAN_MODE_HW_TIMESTAMP) != 0U) {
			/* TODO: add HW timestamp support */
		}

		if ((flags & GS_USB_CAN_MODE_FD) != 0U) {
			mode |= CAN_MODE_FD;
		}

		if ((flags & GS_USB_CAN_MODE_BERR_REPORTING) != 0U) {
			/* TODO: Add bus error reporting support */
		}

		err = can_set_mode(channel->dev, mode);
		if (err != 0U) {
			LOG_ERR("failed to set channel %u mode 0x%08x (err %d)", ch, mode, err);
			return err;
		}

		err = can_start(channel->dev);
		if (err != 0U) {
			LOG_ERR("failed to start channel %u (err %d)", ch, err);
			return err;
		}

		channel->enabled_features = flags;
		break;
	default:
		LOG_ERR("unsupported mode %d requested for channel %u channel",
			sys_le32_to_cpu(payload->mode), ch);
		return -ENOTSUP;
	}

	return 0;
}

static int gs_usb_request_identify(const struct device *dev, uint16_t ch, int32_t tlen,
				   uint8_t *tdata)
{
	struct gs_usb_identify_mode *payload = (struct gs_usb_identify_mode *)tdata;
	struct gs_usb_data *data = dev->data;
	uint32_t mode;
	bool identify;

	if (data->ops.identify == NULL) {
		LOG_ERR("identify not supported");
		return -ENOTSUP;
	}

	if (ch >= data->nchannels) {
		LOG_ERR("identify request for non-existing channel %u", ch);
		return -EINVAL;
	}

	if (tlen != sizeof(*payload)) {
		LOG_ERR("invalid length for identify request (%d)", tlen);
		return -EINVAL;
	}

	mode = sys_le32_to_cpu(payload->mode);

	switch (mode) {
	case GS_USB_CHANNEL_IDENTIFY_MODE_OFF:
		identify = false;
		break;
	case GS_USB_CHANNEL_IDENTIFY_MODE_ON:
		identify = true;
		break;
	default:
		LOG_ERR("unsupported identify mode %d for channel %u", mode, ch);
		return -ENOTSUP;
	}

	return data->ops.identify(dev, ch, identify, data->user_data);
}

static int gs_usb_request_device_config(const struct device *dev, int32_t *tlen, uint8_t **tdata)
{
	struct gs_usb_device_config *payload = (struct gs_usb_device_config *)*tdata;
	struct gs_usb_data *data = dev->data;

	memset(payload, 0, sizeof(*payload));
	payload->nchannels = data->nchannels - 1U; /* 8 bit representing 1 to 256 */
	payload->sw_version = sys_cpu_to_le32(GS_USB_SW_VERSION);
	payload->hw_version = sys_cpu_to_le32(GS_USB_HW_VERSION);

	*tlen = sizeof(*payload);

	return 0;
}

static int gs_usb_vendor_request_handler(struct usb_setup_packet *setup, int32_t *tlen,
					 uint8_t **tdata)
{
	struct usb_dev_data *common;
	const struct device *dev;
	uint16_t ch;

	common = usb_get_dev_data_by_iface(&gs_usb_data_devlist, (uint8_t)setup->wIndex);
	if (common == NULL) {
		LOG_ERR("device data not found for interface %u", setup->wIndex);
		return -ENODEV;
	}

	dev = common->dev;

	switch (setup->RequestType.recipient) {
	case USB_REQTYPE_RECIPIENT_INTERFACE:
		ch = setup->wValue;

		if (usb_reqtype_is_to_host(setup)) {
			/* Interface to host */
			switch (setup->bRequest) {
			case GS_USB_REQUEST_BERR:
				/* Not supported */
				break;
			case GS_USB_REQUEST_BT_CONST:
				return gs_usb_request_bt_const(dev, ch, tlen, tdata);
			case GS_USB_REQUEST_DEVICE_CONFIG:
				return gs_usb_request_device_config(dev, tlen, tdata);
			case GS_USB_REQUEST_TIMESTAMP:
				/* TODO: handle timestamp request */
				break;
			case GS_USB_REQUEST_GET_USER_ID:
				/* Not supported */
				break;
			case GS_USB_REQUEST_BT_CONST_EXT:
#ifdef CONFIG_CAN_FD_MODE
				return gs_usb_request_bt_const_ext(dev, ch, tlen, tdata);
#else  /* CONFIG_CAN_FD_MODE */
				/* Not supported */
				break;
#endif /* !CONFIG_CAN_FD_MODE */
			case GS_USB_REQUEST_GET_TERMINATION:
				return gs_usb_request_get_termination(dev, ch, tlen, tdata);
			case GS_USB_REQUEST_GET_STATE:
				return gs_usb_request_get_state(dev, ch, tlen, tdata);
			default:
				break;
			}
		} else {
			/* Host to interface */
			switch (setup->bRequest) {
			case GS_USB_REQUEST_HOST_FORMAT:
				return gs_usb_request_host_format(*tlen, *tdata);
			case GS_USB_REQUEST_BITTIMING:
				return gs_usb_request_bittiming(dev, ch, *tlen, *tdata);
			case GS_USB_REQUEST_MODE:
				return gs_usb_request_mode(dev, ch, *tlen, *tdata);
			case GS_USB_REQUEST_IDENTIFY:
				return gs_usb_request_identify(dev, ch, *tlen, *tdata);
			case GS_USB_REQUEST_DATA_BITTIMING:
#ifdef CONFIG_CAN_FD_MODE
				return gs_usb_request_data_bittiming(dev, ch, *tlen, *tdata);
#else  /* CONFIG_CAN_FD_MODE */
				/* Not supported */
				break;
#endif /* !CONFIG_CAN_FD_MODE */
			case GS_USB_REQUEST_SET_USER_ID:
				/* Not supported */
				break;
			case GS_USB_REQUEST_SET_TERMINATION:
				return gs_usb_request_set_termination(dev, ch, *tlen, *tdata);
			default:
				break;
			};
		}
		break;
	default:
		break;
	}

	LOG_ERR("bmRequestType 0x%02x bRequest 0x%02x not supported", setup->bmRequestType,
		setup->bRequest);

	return -ENOTSUP;
}

static void gs_usb_can_rx_callback(const struct device *can_dev, struct can_frame *frame,
				   void *user_data)
{
	struct gs_usb_channel_data *channel = user_data;
	struct gs_usb_data *data = CONTAINER_OF(channel, struct gs_usb_data, channels[channel->ch]);
	struct gs_usb_host_frame_hdr hdr = { 0 };
	struct net_buf *buf;
	size_t buf_size = GS_USB_HOST_FRAME_CAN_FRAME_SIZE;
	size_t data_size = sizeof(struct gs_usb_can_frame);

	__ASSERT_NO_MSG(can_dev == channel->dev);

	if (IS_ENABLED(CONFIG_CAN_FD_MODE) && ((frame->flags & CAN_FRAME_FDF) != 0U)) {
		buf_size = GS_USB_HOST_FRAME_CANFD_FRAME_SIZE;
		data_size = sizeof(struct gs_usb_canfd_frame);
	}

	buf = net_buf_alloc_len(data->pool, buf_size, K_NO_WAIT);
	if (buf == NULL) {
		LOG_ERR("failed to allocate RX host frame for channel %u", channel->ch);
		/* TODO: report overrun */
		return;
	}

	hdr.echo_id = GS_USB_HOST_FRAME_ECHO_ID_RX_FRAME;
	hdr.can_id = sys_cpu_to_le32(frame->id);
	hdr.can_dlc = frame->dlc;
	hdr.channel = channel->ch;

	if ((frame->flags & CAN_FRAME_IDE) != 0U) {
		hdr.can_id |= sys_cpu_to_le32(GS_USB_CAN_ID_FLAG_IDE);
	}

	if ((frame->flags & CAN_FRAME_RTR) != 0U) {
		hdr.can_id |= sys_cpu_to_le32(GS_USB_CAN_ID_FLAG_RTR);
	}

	if (IS_ENABLED(CONFIG_CAN_FD_MODE)) {
		if ((frame->flags & CAN_FRAME_FDF) != 0U) {
			hdr.flags |= GS_USB_CAN_FLAG_FD;

			if ((frame->flags & CAN_FRAME_BRS) != 0U) {
				hdr.flags |= GS_USB_CAN_FLAG_BRS;
			}

			if ((frame->flags & CAN_FRAME_ESI) != 0U) {
				hdr.flags |= GS_USB_CAN_FLAG_ESI;
			}
		}
	}

	net_buf_add_mem(buf, &hdr, sizeof(hdr));
	net_buf_add_mem(buf, &frame->data, data_size);
	/* TODO: add HW timestamp if feature enabled */

	LOG_HEXDUMP_DBG(buf->data, buf->len, "RX host frame");

	net_buf_put(&data->rx_fifo, buf);
}

static void gs_usb_rx_thread(void *p1, void *p2, void *p3)
{
	const struct device *dev = p1;
	struct usb_cfg_data *cfg = (void *)dev->config;
	struct gs_usb_data *data = dev->data;
	struct net_buf *buf;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		buf = net_buf_get(&data->rx_fifo, K_FOREVER);

		/* TODO: check returned length? */
		usb_transfer_sync(cfg->endpoint[GS_USB_IN_EP_IDX].ep_addr,
				  buf->data, buf->len, USB_TRANS_WRITE);

		net_buf_unref(buf);
	}
}

static void gs_usb_can_tx_callback(const struct device *can_dev, int error, void *user_data)
{
	/* TODO: allocate echo host frame, put echo frame on rx_queue, unref original net_buf */
	/* TODO: how to propagate error != 0? */
}

static void gs_usb_transfer_tx_callback(uint8_t ep, int tsize, void *priv)
{
	const struct device *dev = priv;
	struct gs_usb_data *data = dev->data;
	struct net_buf *buf;

	if (tsize > 0) {
		buf = net_buf_alloc_len(data->pool, tsize, K_NO_WAIT);
		if (buf == NULL) {
			LOG_ERR("failed to allocated TX buffer");
			return;
		}

		net_buf_add_mem(buf, data->tx_buffer, tsize);
		net_buf_put(&data->tx_fifo, buf);
	}

	gs_usb_transfer_tx_prepare(dev);
}

static void gs_usb_transfer_tx_prepare(const struct device *dev)
{
	struct usb_cfg_data *cfg = (void *)dev->config;
	struct gs_usb_data *data = dev->data;

	usb_transfer(cfg->endpoint[GS_USB_OUT_EP_IDX].ep_addr,
		     data->tx_buffer, sizeof(data->tx_buffer), USB_TRANS_READ,
		     gs_usb_transfer_tx_callback, (void *)dev);
}

static void gs_usb_tx_thread(void *p1, void *p2, void *p3)
{
	const struct device *dev = p1;
	struct gs_usb_data *data = dev->data;
	struct net_buf *buf;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		buf = net_buf_get(&data->tx_fifo, K_FOREVER);

		/* TODO: blocking read from tx_fifo, call can_send(channel->dev, frame, K_FOREVER)? */
		/* TODO: how to propagate failure from can_send()? */
		LOG_HEXDUMP_DBG(buf->data, buf->len, "TX host frame");

		net_buf_unref(buf);
	}
}

static uint32_t gs_usb_features_from_ops(struct gs_usb_ops *ops)
{
	uint32_t features = 0U;

	if (ops->identify != NULL) {
		features |= GS_US_CAN_FEATURE_IDENTIFY;
	}

	if (ops->set_termination != NULL && ops->get_termination != NULL) {
		features |= GS_US_CAN_FEATURE_TERMINATION;
	}

	return features;
}

static uint32_t gs_usb_features_from_capabilities(can_mode_t caps)
{
	uint32_t features = 0U;

	if ((caps & CAN_MODE_LOOPBACK) != 0U) {
		features |= GS_US_CAN_FEATURE_LOOP_BACK;
	}

	if ((caps & CAN_MODE_LISTENONLY) != 0U) {
		features |= GS_US_CAN_FEATURE_LISTEN_ONLY;
	}

	if ((caps & CAN_MODE_FD) != 0U) {
		features |= GS_US_CAN_FEATURE_FD;
		features |= GS_US_CAN_FEATURE_BT_CONST_EXT;
	}

	if ((caps & CAN_MODE_ONE_SHOT) != 0U) {
		features |= GS_US_CAN_FEATURE_ONE_SHOT;
	}

	if ((caps & CAN_MODE_3_SAMPLES) != 0U) {
		features |= GS_US_CAN_FEATURE_TRIPLE_SAMPLE;
	}

	return features;
}

static int gs_usb_register_channel(struct gs_usb_channel_data *channel, uint16_t ch,
				   const struct device *can_dev, uint32_t common_features)
{
	const struct can_filter filters[] = {
		{
			.id = 0U,
			.mask = 0U,
			.flags = 0U,
		},
		{
			.id = 0U,
			.mask = 0U,
			.flags = CAN_FILTER_IDE,
		}
	};
	can_mode_t caps;
	int err;
	int i;

	if (!device_is_ready(can_dev)) {
		LOG_ERR("channel %d CAN device not ready", ch);
		return -ENODEV;
	}

	err = can_get_capabilities(can_dev, &caps);
	if (err != 0U) {
		LOG_ERR("failed to get capabilities for channel %u (err %d)", ch, err);
		return err;
	}

	for (i = 0; i < ARRAY_SIZE(filters); i++) {
		err = can_add_rx_filter(can_dev, gs_usb_can_rx_callback, channel, &filters[i]);
		if (err < 0U) {
			LOG_ERR("failed to add filter %d to channel %d (err %d)", i, ch, err);
			return err;
		}
	}

	channel->ch = ch;
	channel->dev = can_dev;
	channel->supported_features = common_features;
	channel->supported_features |= gs_usb_features_from_capabilities(caps);
	channel->mode_base = CAN_MODE_NORMAL;

	if ((caps & CAN_MODE_MANUAL_RECOVERY) != 0U) {
		channel->mode_base |= CAN_MODE_MANUAL_RECOVERY;
	} else {
		LOG_WRN("channel %d does not support manual recovery mode", ch);
	}

	LOG_DBG("channel %d features = 0x%08x", ch, channel->supported_features);

	return 0;
}

int gs_usb_register(const struct device *dev, const struct device **channels, size_t nchannels,
		    const struct gs_usb_ops *ops, void *user_data)
{
	struct gs_usb_data *data = dev->data;
	uint32_t common_features = GS_US_CAN_FEATURE_GET_STATE;
	int err;
	int ch;

	if (nchannels < 1U || nchannels > ARRAY_SIZE(data->channels)) {
		LOG_ERR("unsupported number of CAN channels %u", nchannels);
		return -ENOTSUP;
	}

	if (ops != NULL) {
		data->ops = *ops;
	}

	data->nchannels = nchannels;
	data->user_data = user_data;

	/* TODO: add GS_US_CAN_FEATURE_HW_TIMESTAMP feature support */
	/* TODO: add GS_US_CAN_FEATURE_BERR_REPORTING feature support */
	common_features = gs_usb_features_from_ops(&data->ops);

	for (ch = 0; ch < nchannels; ch++) {
		err = gs_usb_register_channel(&data->channels[ch], ch, channels[ch],
					      common_features);
		if (err != 0U) {
			LOG_ERR("failed to register channel %d (err %d)", ch, err);
			return err;
		}
	}

	sys_slist_append(&gs_usb_data_devlist, &data->common.node);

	return 0;
}

static int gs_usb_init(const struct device *dev)
{
	struct gs_usb_data *data = dev->data;

	data->common.dev = dev;

	k_fifo_init(&data->rx_fifo);
	k_fifo_init(&data->tx_fifo);

	k_thread_create(&data->rx_thread, data->rx_stack,
			K_KERNEL_STACK_SIZEOF(data->rx_stack),
			gs_usb_rx_thread,
			(void *)dev, NULL, NULL,
			CONFIG_USB_DEVICE_GS_USB_RX_THREAD_PRIO,
			0, K_NO_WAIT);
	k_thread_name_set(&data->rx_thread, "gs_usb_rx");

	k_thread_create(&data->tx_thread, data->tx_stack,
			K_KERNEL_STACK_SIZEOF(data->tx_stack),
			gs_usb_tx_thread,
			(void *)dev, NULL, NULL,
			CONFIG_USB_DEVICE_GS_USB_TX_THREAD_PRIO,
			0, K_NO_WAIT);
	k_thread_name_set(&data->tx_thread, "gs_usb_tx");

	return 0;
}

#define INITIALIZER_IF                                                                             \
	{                                                                                          \
		.bLength = sizeof(struct usb_if_descriptor),                                       \
		.bDescriptorType = USB_DESC_INTERFACE,                                             \
		.bInterfaceNumber = 0,                                                             \
		.bAlternateSetting = 0,                                                            \
		.bNumEndpoints = 3,                                                                \
		.bInterfaceClass = USB_BCC_VENDOR,                                                 \
		.bInterfaceSubClass = 0,                                                           \
		.bInterfaceProtocol = 0,                                                           \
		.iInterface = 0,                                                                   \
	}

#define INITIALIZER_IF_EP(addr)                                                                    \
	{                                                                                          \
		.bLength = sizeof(struct usb_ep_descriptor),                                       \
		.bDescriptorType = USB_DESC_ENDPOINT,                                              \
		.bEndpointAddress = addr,                                                          \
		.bmAttributes = USB_DC_EP_BULK,                                                    \
		.wMaxPacketSize = sys_cpu_to_le16(CONFIG_USB_DEVICE_GS_USB_MAX_PACKET_SIZE),       \
		.bInterval = 0x01,                                                                 \
	}

#define GS_USB_DEVICE_DEFINE(inst)                                                                 \
	BUILD_ASSERT(DT_INST_ON_BUS(inst, usb),                                                    \
		     "node " DT_NODE_PATH(                                                         \
			     DT_DRV_INST(inst)) " is not assigned to a USB device controller");    \
                                                                                                   \
	NET_BUF_POOL_FIXED_DEFINE(gs_usb_pool_##inst, CONFIG_USB_DEVICE_GS_USB_POOL_SIZE,          \
				  GS_USB_HOST_FRAME_MAX_SIZE, 0, NULL);                            \
                                                                                                   \
	USBD_CLASS_DESCR_DEFINE(primary, 0)                                                        \
	struct gs_usb_config gs_usb_config_##inst = {                                              \
		.if0 = INITIALIZER_IF,                                                             \
		.if0_in_ep = INITIALIZER_IF_EP(GS_USB_IN_EP_ADDR),                                 \
		.if0_dummy_ep = INITIALIZER_IF_EP(GS_USB_DUMMY_EP_ADDR),                           \
		.if0_out_ep = INITIALIZER_IF_EP(GS_USB_OUT_EP_ADDR),                               \
	};                                                                                         \
                                                                                                   \
	static struct usb_ep_cfg_data gs_usb_ep_cfg_data_##inst[] = {                              \
		{                                                                                  \
			.ep_cb = usb_transfer_ep_callback,                                         \
			.ep_addr = GS_USB_IN_EP_ADDR,                                              \
		},                                                                                 \
		{                                                                                  \
			.ep_cb = usb_transfer_ep_callback,                                         \
			.ep_addr = GS_USB_DUMMY_EP_ADDR,                                           \
		},                                                                                 \
		{                                                                                  \
			.ep_cb = usb_transfer_ep_callback,                                         \
			.ep_addr = GS_USB_OUT_EP_ADDR,                                             \
		},                                                                                 \
	};                                                                                         \
                                                                                                   \
	USBD_DEFINE_CFG_DATA(gs_usb_cfg_##inst) = {                                                \
		.usb_device_description = NULL,                                                    \
		.interface_config = gs_usb_interface_config,                                       \
		.interface_descriptor = &gs_usb_config_##inst.if0,                                 \
		.cb_usb_status = gs_usb_status_callback,                                           \
		.interface = {                                                                     \
			.class_handler = NULL,                                                     \
			.custom_handler = NULL,                                                    \
			.vendor_handler = gs_usb_vendor_request_handler,                           \
		},                                                                                 \
		.num_endpoints = ARRAY_SIZE(gs_usb_ep_cfg_data_##inst),                            \
		.endpoint = gs_usb_ep_cfg_data_##inst,                                             \
	};                                                                                         \
                                                                                                   \
	static struct gs_usb_data gs_usb_data_##inst = {                                           \
		.pool = &gs_usb_pool_##inst,                                                       \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, gs_usb_init, NULL, &gs_usb_data_##inst,                        \
			      &gs_usb_cfg_##inst,                                                  \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, NULL);

DT_INST_FOREACH_STATUS_OKAY(GS_USB_DEVICE_DEFINE);
