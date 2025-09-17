/*
 * Copyright (c) 2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 * Copyright (c) 2018 Aurelien Jarno <aurelien@aurel32.net>
 * Copyright Google LLC.
 * Copyright Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "udc_common.h"

#include <string.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/clock_control/atmel_sam_pmc.h>
#include <zephyr/drivers/usb/udc.h>
#include <zephyr/sys/barrier.h>
#include <soc.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(udc_sam_usbhs, CONFIG_UDC_DRIVER_LOG_LEVEL);

/*
 * The new Atmel DFP headers provide mode-specific interrupt register field
 * definitions.  Map the existing generic definitions to these.
 */
#ifndef USBHS_DEVEPTISR_CTRL_RXSTPI_Msk
#define USBHS_DEVEPTISR_CTRL_RXSTPI_Msk USBHS_DEVEPTISR_RXSTPI_Msk
#endif
#ifndef USBHS_DEVEPTIER_CTRL_RXSTPES_Msk
#define USBHS_DEVEPTIER_CTRL_RXSTPES_Msk USBHS_DEVEPTIER_RXSTPES_Msk
#endif
#ifndef USBHS_DEVEPTIER_CTRL_STALLRQS_Msk
#define USBHS_DEVEPTIER_CTRL_STALLRQS_Msk USBHS_DEVEPTIER_STALLRQS_Msk
#endif
#ifndef USBHS_DEVEPTIDR_CTRL_STALLRQC_Msk
#define USBHS_DEVEPTIDR_CTRL_STALLRQC_Msk USBHS_DEVEPTIDR_STALLRQC_Msk
#endif

enum udc_sam_usbhs_event_type {
	/* Setup packet received. */
	UDC_SAM_USBHS_EVT_SETUP,
	/* Trigger new transfer (except control OUT). */
	UDC_SAM_USBHS_EVT_XFER_NEW,
	/* Transfer for specific endpoint is finished. */
	UDC_SAM_USBHS_EVT_XFER_FINISHED,
};

struct udc_sam_usbhs_config {
	Usbhs *base;
	mem_addr_t dpram;
	struct atmel_sam_pmc_config clock_cfg;
	size_t num_of_eps;
	struct udc_ep_config *ep_cfg_in;
	struct udc_ep_config *ep_cfg_out;
	int speed_idx;
	void (*make_thread)(const struct device *dev);
	void (*irq_enable_func)(void);
	void (*irq_disable_func)(void);
};

struct udc_sam_usbhs_data {
	struct k_thread thread_data;
	/*
	 * Events on which the driver thread waits. the xfer_new and xfer_finished fields contain
	 * information on which endpoints events UDC_SAM_USBHS_EVT_XFER_NEW or
	 * UDC_SAM_USBHS_EVT_XFER_FINISHED are triggered. The mapping is bits 31..16 for IN
	 * endpoints and bits 15..0 for OUT endpoints.
	 */
	struct k_event events;
	atomic_t xfer_new;
	atomic_t xfer_finished;

	uint8_t setup[8];
};

static inline int udc_sam_usbhs_ep_to_bnum(const uint8_t ep)
{
	if (USB_EP_DIR_IS_IN(ep)) {
		return 16UL + USB_EP_GET_IDX(ep);
	}

	return USB_EP_GET_IDX(ep);
}

static inline uint8_t udc_sam_usbhs_pull_ep_from_bmsk(uint32_t *const bitmap)
{
	unsigned int bit;

	__ASSERT_NO_MSG(bitmap != NULL && *bitmap != 0U);

	bit = find_lsb_set(*bitmap) - 1;
	*bitmap &= ~BIT(bit);

	if (bit >= 16U) {
		return USB_EP_DIR_IN | (bit - 16U);
	} else {
		return USB_EP_DIR_OUT | bit;
	}
}

static inline void udc_sam_usbhs_ep_reset(const struct device *dev, uint8_t ep_idx)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *base = config->base;

	base->USBHS_DEVEPT |= BIT(USBHS_DEVEPT_EPRST0_Pos + ep_idx);
	barrier_dsync_fence_full();

	base->USBHS_DEVEPT &= ~BIT(USBHS_DEVEPT_EPRST0_Pos + ep_idx);
	barrier_dsync_fence_full();
}

static inline void udc_sam_usbhs_ep_enable_interrupts(const struct device *dev, uint8_t ep)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	uint8_t ep_idx = USB_EP_GET_IDX(ep);
	Usbhs *base = config->base;
	uint32_t eptier;

	if (ep_idx == 0U) {
		eptier = USBHS_DEVEPTIER_CTRL_RXSTPES_Msk | USBHS_DEVEPTIER_RXOUTES_Msk;
	} else if (USB_EP_DIR_IS_OUT(ep)) {
		eptier = USBHS_DEVEPTIER_RXOUTES_Msk;
	} else {
		/* Acknowledge FIFO empty interrupt */
		base->USBHS_DEVEPTICR[ep_idx] = USBHS_DEVEPTICR_TXINIC;
		eptier = USBHS_DEVEPTIER_TXINES_Msk;
	}

	base->USBHS_DEVEPTIER[ep_idx] = eptier;
}

static void udc_sam_usbhs_fifo_data_read(const struct device *dev, uint8_t ep_idx, uint8_t *dest,
					 const size_t length)
{
	const struct udc_sam_usbhs_config *config = dev->config;

	memcpy(dest, (void *)(config->dpram + (0x8000 * ep_idx)), length);
}

static void udc_sam_usbhs_fifo_data_write(const struct device *dev, uint8_t ep_idx, uint8_t *src,
					 const size_t length)
{
	const struct udc_sam_usbhs_config *config = dev->config;

	memcpy((void *)(config->dpram + (0x8000 * ep_idx)), src, length);
}

static int udc_sam_usbhs_prep_out(const struct device *dev, struct net_buf *const buf,
				  struct udc_ep_config *const ep_cfg)
{
	/* TODO: implement this (if needed?) */
	LOG_DBG("");

	return 0;
}

static int udc_sam_usbhs_prep_in(const struct device *dev, struct net_buf *const buf,
				 struct udc_ep_config *const ep_cfg)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->addr);
	Usbhs *base = config->base;

	/* TODO: determine len, etc. */
	LOG_DBG("");

	/* TODO: write from buf into FIFO */
	udc_sam_usbhs_fifo_data_write(dev, ep_idx, buf->data, buf->len);

	if (ep_idx == 0U) {
		/* TODO: mark FIFO ready */
		base->USBHS_DEVEPTICR[ep_idx] |= USBHS_DEVEPTICR_TXINIC;
		base->USBHS_DEVEPTIER[ep_idx] |= USBHS_DEVEPTIER_TXINES;
	} else {
		/* TODO: mark FIFO ready */
		base->USBHS_DEVEPTIDR[ep_idx] |= USBHS_DEVEPTIDR_FIFOCONC;
	}

	return 0;
}

static int udc_sam_usbhs_ctrl_feed_dout(const struct device *dev, const size_t length)
{
	struct udc_ep_config *const ep_cfg = udc_get_ep_cfg(dev, USB_CONTROL_EP_OUT);
	struct net_buf *buf;

	buf = udc_ctrl_alloc(dev, USB_CONTROL_EP_OUT, length);
	if (buf == NULL) {
		return -ENOMEM;
	}

	udc_buf_put(ep_cfg, buf);

	return udc_sam_usbhs_prep_out(dev, buf, ep_cfg);
}

static void udc_sam_usbhs_drop_control_transfers(const struct device *dev)
{
	struct net_buf *buf;

	buf = udc_buf_get_all(udc_get_ep_cfg(dev, USB_CONTROL_EP_OUT));
	if (buf != NULL) {
		net_buf_unref(buf);
	}

	buf = udc_buf_get_all(udc_get_ep_cfg(dev, USB_CONTROL_EP_IN));
	if (buf != NULL) {
		net_buf_unref(buf);
	}
}

static int udc_sam_usbhs_handle_evt_setup(const struct device *dev)
{
	struct udc_sam_usbhs_data *const priv = udc_get_private(dev);
	struct net_buf *buf;
	int err;

	udc_sam_usbhs_drop_control_transfers(dev);

	buf = udc_ctrl_alloc(dev, USB_CONTROL_EP_OUT, sizeof(priv->setup));
	if (buf == NULL) {
		return -ENOMEM;
	}

	net_buf_add_mem(buf, priv->setup, sizeof(priv->setup));
	udc_ep_buf_set_setup(buf);

	/* Update to next stage of control transfer */
	udc_ctrl_update_stage(dev, buf);

	if (udc_ctrl_stage_is_data_out(dev)) {
		/*  Allocate and feed buffer for data OUT stage */
		LOG_DBG("s:%p|feed for -out-", (void *)buf);
		err = udc_sam_usbhs_ctrl_feed_dout(dev, udc_data_stage_length(buf));
		if (err == -ENOMEM) {
			udc_submit_ep_event(dev, buf, err);
		} else {
			return err;
		}
	} else if (udc_ctrl_stage_is_data_in(dev)) {
		LOG_DBG("s:%p|feed for -in-status", (void *)buf);
		err = udc_ctrl_submit_s_in_status(dev);
	} else {
		LOG_DBG("s:%p|no data", (void *)buf);
		err = udc_ctrl_submit_s_status(dev);
	}

	return err;
}

static int udc_sam_usbhs_handle_evt_din(const struct device *dev,
					struct udc_ep_config *const ep_cfg)
{
	struct net_buf *buf;
	int err;

	buf = udc_buf_get(ep_cfg);
	if (buf == NULL) {
		LOG_ERR("No buffer for ep 0x%02x", ep_cfg->addr);
		return -ENOBUFS;
	}

	udc_ep_set_busy(ep_cfg, false);

	if (ep_cfg->addr == USB_CONTROL_EP_IN) {
		if (udc_ctrl_stage_is_status_in(dev) || udc_ctrl_stage_is_no_data(dev)) {
			/* Status stage finished, notify upper layer */
			udc_ctrl_submit_status(dev, buf);
		}

		/* Update to next stage of control transfer */
		udc_ctrl_update_stage(dev, buf);

		if (udc_ctrl_stage_is_data_in(dev)) {
			/*
			 * s-in-[status] finished, release buffer.
			 * Since the controller supports auto-status we cannot use
			 * if (udc_ctrl_stage_is_status_out()) after state update.
			 */
			net_buf_unref(buf);
		}

		/* if (udc_ctrl_stage_is_status_out(dev)) { */
		/* 	/\* IN transfer finished, submit buffer for status stage *\/ */
		/* 	net_buf_unref(buf); */

		/* 	err = udc_sam_usbhs_ctrl_feed_dout(dev, 0); */
		/* 	if (err == -ENOMEM) { */
		/* 		udc_submit_ep_event(dev, buf, err); */
		/* 	} else { */
		/* 		return err; */
		/* 	} */
		/* } */

		return 0;
	}

	return udc_submit_ep_event(dev, buf, 0);
}

static int udc_sam_usbhs_handle_evt_dout(const struct device *dev,
					 struct udc_ep_config *const ep_cfg)
{
	struct net_buf *buf;
	int err = 0;

	buf = udc_buf_get(ep_cfg);
	if (buf == NULL) {
		LOG_ERR("No buffer for OUT ep 0x%02x", ep_cfg->addr);
		return -ENODATA;
	}

	/* TODO: read from FIFO into buf */
	/* TODO: release bank */

	udc_ep_set_busy(ep_cfg, false);

	if (ep_cfg->addr == USB_CONTROL_EP_OUT) {
		if (udc_ctrl_stage_is_status_out(dev)) {
			LOG_DBG("dout:%p|status, feed >s", (void *)buf);

			/* Status stage finished, notify upper layer */
			udc_ctrl_submit_status(dev, buf);
		}

		/* Update to next stage of control transfer */
		udc_ctrl_update_stage(dev, buf);

		if (udc_ctrl_stage_is_status_in(dev)) {
			err = udc_ctrl_submit_s_out_status(dev, buf);
		}
	} else {
		err = udc_submit_ep_event(dev, buf, 0);
	}

	return err;
}

static void udc_sam_usbhs_handle_xfer_next(const struct device *dev,
					   struct udc_ep_config *const ep_cfg)
{
	struct net_buf *buf;
	int err;

	buf = udc_buf_peek(ep_cfg);
	if (buf == NULL) {
		return;
	}

	if (USB_EP_DIR_IS_OUT(ep_cfg->addr)) {
		err = udc_sam_usbhs_prep_out(dev, buf, ep_cfg);
	} else {
		err = udc_sam_usbhs_prep_in(dev, buf, ep_cfg);
	}

	if (err != 0) {
		buf = udc_buf_get(ep_cfg);
		udc_submit_ep_event(dev, buf, -ECONNREFUSED);
	} else {
		udc_ep_set_busy(ep_cfg, true);
	}
}

static ALWAYS_INLINE void udc_sam_usbhs_thread_handler(const struct device *dev)
{
	struct udc_sam_usbhs_data *const priv = udc_get_private(dev);
	struct udc_ep_config *ep_cfg;
	uint32_t evt;
	uint32_t eps;
	uint8_t ep;
	int err;

	evt = k_event_wait(&priv->events, UINT32_MAX, false, K_FOREVER);
	udc_lock_internal(dev, K_FOREVER);

	if (evt & BIT(UDC_SAM_USBHS_EVT_XFER_FINISHED)) {
		k_event_clear(&priv->events, BIT(UDC_SAM_USBHS_EVT_XFER_FINISHED));

		eps = atomic_clear(&priv->xfer_finished);

		while (eps) {
			ep = udc_sam_usbhs_pull_ep_from_bmsk(&eps);
			ep_cfg = udc_get_ep_cfg(dev, ep);
			LOG_DBG("Finished event ep 0x%02x", ep);

			if (USB_EP_DIR_IS_IN(ep)) {
				err = udc_sam_usbhs_handle_evt_din(dev, ep_cfg);
			} else {
				err = udc_sam_usbhs_handle_evt_dout(dev, ep_cfg);
			}

			if (err != 0) {
				udc_submit_event(dev, UDC_EVT_ERROR, err);
			}

			if (!udc_ep_is_busy(ep_cfg)) {
				udc_sam_usbhs_handle_xfer_next(dev, ep_cfg);
			} else {
				LOG_ERR("Endpoint 0x%02x busy", ep);
			}
		}
	}

	if (evt & BIT(UDC_SAM_USBHS_EVT_XFER_NEW)) {
		k_event_clear(&priv->events, BIT(UDC_SAM_USBHS_EVT_XFER_NEW));

		eps = atomic_clear(&priv->xfer_new);

		while (eps != 0U) {
			ep = udc_sam_usbhs_pull_ep_from_bmsk(&eps);
			ep_cfg = udc_get_ep_cfg(dev, ep);
			LOG_INF("New transfer ep 0x%02x in the queue", ep);

			if (!udc_ep_is_busy(ep_cfg)) {
				udc_sam_usbhs_handle_xfer_next(dev, ep_cfg);
			} else {
				LOG_ERR("Endpoint 0x%02x busy", ep);
			}
		}
	}

	if (evt & BIT(UDC_SAM_USBHS_EVT_SETUP)) {
		k_event_clear(&priv->events, BIT(UDC_SAM_USBHS_EVT_SETUP));
		err = udc_sam_usbhs_handle_evt_setup(dev);
		if (err != 0) {
			udc_submit_event(dev, UDC_EVT_ERROR, err);
		}
	}

	udc_unlock_internal(dev);
}

static void udc_sam_usbhs_handle_setup_irq(const struct device *dev, uint32_t deveptisr)
{
	struct udc_sam_usbhs_data *const priv = udc_get_private(dev);
	uint16_t byct;

	LOG_DBG("");

	byct = FIELD_GET(USBHS_DEVEPTISR_BYCT_Msk, deveptisr);
	if (byct != sizeof(priv->setup)) {
		LOG_ERR("Wrong byte count %u for setup packet", byct);
	}

	udc_sam_usbhs_fifo_data_read(dev, 0U, priv->setup, sizeof(priv->setup));

	k_event_post(&priv->events, BIT(UDC_SAM_USBHS_EVT_SETUP));
}

static void udc_sam_usbhs_handle_out_irq(const struct device *dev, const uint8_t ep,
					 uint32_t deveptisr)
{
	struct udc_ep_config *ep_cfg = udc_get_ep_cfg(dev, ep);
	struct net_buf *buf;
	uint16_t byct;

	buf = udc_buf_peek(ep_cfg);
	if (buf == NULL) {
		LOG_ERR("No OUT buffer for ep 0x%02x", ep);
		udc_submit_event(dev, UDC_EVT_ERROR, -ENOBUFS);
		return;
	}

	byct = FIELD_GET(USBHS_DEVEPTISR_BYCT_Msk, deveptisr);

	LOG_DBG("ISR ep 0x%02x byct %u room %u mps %u", ep, byct, net_buf_tailroom(buf),
		udc_mps_ep_size(ep_cfg));

	/* TODO: implement this */

	/* TODO: if EP0, read FIFO into buf here */
	/* TODO: else, ... */
}

static void udc_sam_usbhs_handle_in_irq(const struct device *dev, const uint8_t ep,
					uint32_t deveptisr)
{
	struct udc_sam_usbhs_data *const priv = udc_get_private(dev);
	const struct udc_sam_usbhs_config *config = dev->config;
	struct udc_ep_config *ep_cfg = udc_get_ep_cfg(dev, ep);
	Usbhs *base = config->base;
	struct net_buf *buf;
	uint32_t devctrl;
	uint16_t byct;
	int err;

	buf = udc_buf_peek(ep_cfg);
	if (buf == NULL) {
		LOG_ERR("No IN buffer for ep 0x%02x", ep);
		udc_submit_event(dev, UDC_EVT_ERROR, -ENOBUFS);
		return;
	}

	devctrl = base->USBHS_DEVCTRL;
	if ((devctrl & USBHS_DEVCTRL_UADD_Msk) != 0U && (devctrl & USBHS_DEVCTRL_ADDEN_Msk) == 0U) {
		LOG_ERR("setting ADDEN, DEVCTRL = 0x%08x", devctrl);
		base->USBHS_DEVCTRL |= USBHS_DEVCTRL_ADDEN;
	}

	byct = FIELD_GET(USBHS_DEVEPTISR_BYCT_Msk, deveptisr);

	LOG_DBG("ISR ep 0x%02x byct %u", ep, byct);

	net_buf_pull(buf, byct);

	if (buf->len != 0U) {
		err = udc_sam_usbhs_prep_in(dev, buf, ep_cfg);
		__ASSERT(err == 0, "Failed to start new IN transaction");
	} else {
		if (udc_ep_buf_has_zlp(buf)) {
			err = udc_sam_usbhs_prep_in(dev, buf, ep_cfg);
			__ASSERT(err == 0, "Failed to start new IN transaction");
			udc_ep_buf_clear_zlp(buf);
			return;
		}

		atomic_set_bit(&priv->xfer_finished, udc_sam_usbhs_ep_to_bnum(ep));
		k_event_post(&priv->events, BIT(UDC_SAM_USBHS_EVT_XFER_FINISHED));
	}
}

static void ALWAYS_INLINE udc_sam_usbhs_ep_isr_handler(const struct device *dev,
						       const uint8_t ep_idx)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *base = config->base;
	uint32_t sr;

	sr = base->USBHS_DEVEPTISR[ep_idx];

	LOG_INF("ep%d deveptisr: 0x%08x", ep_idx, sr);

	if ((sr & USBHS_DEVEPTISR_TXINI_Msk) != 0U) {
		/* TODO: where to disable this? */
		base->USBHS_DEVEPTIDR[ep_idx] = USBHS_DEVEPTIDR_TXINEC;
		udc_sam_usbhs_handle_in_irq(dev, ep_idx | USB_EP_DIR_IN, sr);
	}

	if ((sr & USBHS_DEVEPTISR_RXOUTI_Msk) != 0U) {
		udc_sam_usbhs_handle_out_irq(dev, ep_idx | USB_EP_DIR_IN, sr);
	}

	if ((sr & USBHS_DEVEPTISR_CTRL_RXSTPI_Msk) != 0U) {
		udc_sam_usbhs_handle_setup_irq(dev, sr);
	}

	base->USBHS_DEVEPTICR[ep_idx] = sr & base->USBHS_DEVEPTIMR[ep_idx];
}

static void udc_sam_usbhs_isr_handler(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *base = config->base;
	uint32_t sr;

	/* Read and clear global IRQs */
	LOG_INF("devisr: 0x%08x, deimr: 0x%08x", base->USBHS_DEVISR, base->USBHS_DEVIMR);
	sr = base->USBHS_DEVISR & base->USBHS_DEVIMR;
	base->USBHS_DEVICR = sr;

	LOG_INF("isr: 0x%08x", sr);

	for (int ep_idx = 0; ep_idx < config->num_of_eps; ep_idx++) {
		if ((sr & BIT(USBHS_DEVISR_PEP_0_Pos + ep_idx)) != 0U) {
			udc_sam_usbhs_ep_isr_handler(dev, ep_idx);
		}
	}

	if (IS_ENABLED(CONFIG_UDC_ENABLE_SOF) && ((sr & USBHS_DEVISR_SOF_Msk) != 0U)) {
		udc_submit_sof_event(dev);
	}

	if ((sr & USBHS_DEVISR_EORST_Msk) != 0U) {
		if ((base->USBHS_DEVEPT & USBHS_DEVEPT_EPEN0_Msk) != 0U) {
			/* Re-enable endpoint 0 interrupts, cleared by USB reset */
			base->USBHS_DEVEPTIER[0] =
				USBHS_DEVEPTIER_CTRL_RXSTPES_Msk | USBHS_DEVEPTIER_RXOUTES_Msk;
		}

		udc_submit_event(dev, UDC_EVT_RESET, 0);
	}

	if ((sr & USBHS_DEVISR_SUSP_Msk) != 0U) {
		if (!udc_is_suspended(dev)) {
			udc_set_suspended(dev, true);
			udc_submit_event(dev, UDC_EVT_SUSPEND, 0);
		}
	}

	if ((sr & USBHS_DEVISR_EORSM_Msk) != 0U) {
		if (udc_is_suspended(dev)) {
			udc_set_suspended(dev, false);
			udc_submit_event(dev, UDC_EVT_RESUME, 0);
		}
	}
}

static int udc_sam_usbhs_ep_enqueue(const struct device *dev, struct udc_ep_config *const ep_cfg,
				    struct net_buf *buf)
{
	struct udc_sam_usbhs_data *const priv = udc_get_private(dev);

	LOG_DBG("%s enqueue 0x%02x %p len %u", dev->name, ep_cfg->addr, (void *)buf, buf->len);
	udc_buf_put(ep_cfg, buf);

	if (!ep_cfg->stat.halted) {
		atomic_set_bit(&priv->xfer_new, udc_sam_usbhs_ep_to_bnum(ep_cfg->addr));
		k_event_post(&priv->events, BIT(UDC_SAM_USBHS_EVT_XFER_NEW));
	}

	return 0;
}

static int udc_sam_usbhs_ep_dequeue(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->addr);
	Usbhs *base = config->base;
	struct net_buf *buf;

	/*
	 * Abort algorithm according to SAM E70/S70/V70/V71 family datasheet (DS60001527H), figure
	 * 38-13.
	 */
	base->USBHS_DEVEPTIDR[ep_idx] = USBHS_DEVEPTIDR_TXINEC;

	while ((base->USBHS_DEVEPTISR[ep_idx] & USBHS_DEVEPTISR_NBUSYBK_Msk) != 0U) {
		base->USBHS_DEVEPTIER[ep_idx] = USBHS_DEVEPTIER_KILLBKS;

		while ((base->USBHS_DEVEPTIMR[ep_idx] & USBHS_DEVEPTIMR_KILLBK_Msk) != 0U) {
			/* Wait for bank to be killed */
		}
	}

	udc_sam_usbhs_ep_reset(dev, ep_idx);

	buf = udc_buf_get_all(ep_cfg);
	if (buf) {
		udc_submit_ep_event(dev, buf, -ECONNABORTED);
		udc_ep_set_busy(ep_cfg, false);
	}

	udc_sam_usbhs_ep_enable_interrupts(dev, ep_cfg->addr);

	return 0;
}

static int udc_sam_usbhs_ep_enable(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->addr);
	uint16_t mps = udc_mps_ep_size(ep_cfg);
	Usbhs *base = config->base;
	uint32_t eptcfg = 0U;

	LOG_DBG("Enable ep%d 0x%02x", ep_idx, ep_cfg->addr);

	udc_sam_usbhs_ep_reset(dev, ep_idx);

	if (ep_idx == 0U) {
		eptcfg |= USBHS_DEVEPTCFG_EPDIR_OUT;
	} else if (USB_EP_DIR_IS_OUT(ep_cfg->addr)) {
		eptcfg |= USBHS_DEVEPTCFG_EPDIR_OUT;
	} else {
		eptcfg |= USBHS_DEVEPTCFG_EPDIR_IN;
	}

	if (mps <= 8) {
		eptcfg |= USBHS_DEVEPTCFG_EPSIZE_8_BYTE;
	} else if (mps <= 16) {
		eptcfg |= USBHS_DEVEPTCFG_EPSIZE_16_BYTE;
	} else if (mps <= 32) {
		eptcfg |= USBHS_DEVEPTCFG_EPSIZE_32_BYTE;
	} else if (mps <= 64) {
		eptcfg |= USBHS_DEVEPTCFG_EPSIZE_64_BYTE;
	} else if (mps <= 128) {
		eptcfg |= USBHS_DEVEPTCFG_EPSIZE_128_BYTE;
	} else if (mps <= 256) {
		eptcfg |= USBHS_DEVEPTCFG_EPSIZE_256_BYTE;
	} else if (mps <= 512) {
		eptcfg |= USBHS_DEVEPTCFG_EPSIZE_512_BYTE;
	} else if (mps <= 1024) {
		eptcfg |= USBHS_DEVEPTCFG_EPSIZE_1024_BYTE;
	} else {
		return -EINVAL;
	}

	switch (ep_cfg->attributes & USB_EP_TRANSFER_TYPE_MASK) {
	case USB_EP_TYPE_CONTROL:
		eptcfg |= USBHS_DEVEPTCFG_EPTYPE_CTRL;
		break;
	case USB_EP_TYPE_ISO:
		/* Use double bank buffering for isochronous endpoints */
		eptcfg |= USBHS_DEVEPTCFG_EPTYPE_ISO | USBHS_DEVEPTCFG_EPBK_2_BANK;
		break;
	case USB_EP_TYPE_BULK:
		eptcfg |= USBHS_DEVEPTCFG_EPTYPE_BLK;
		break;
	case USB_EP_TYPE_INTERRUPT:
		eptcfg |= USBHS_DEVEPTCFG_EPTYPE_INTRPT;
		break;
	default:
		return -EINVAL;
	}

	/* TODO: de-alloc and alloc all EPs */
	eptcfg |= USBHS_DEVEPTCFG_ALLOC_Msk;

	base->USBHS_DEVEPTCFG[ep_idx] = eptcfg;

	if ((base->USBHS_DEVEPTISR[ep_idx] & USBHS_DEVEPTISR_CFGOK_Msk) == 0U) {
		LOG_ERR("Invalid ep%d 0x%02x configuration", ep_idx, ep_cfg->addr);
		return -EINVAL;
	}

	/* Enable endpoint */
	base->USBHS_DEVEPT |= BIT(USBHS_DEVEPT_EPEN0_Pos + ep_idx);

	/* Enable endpoint interrupts */
	udc_sam_usbhs_ep_enable_interrupts(dev, ep_cfg->addr);

	/* Enable global endpoint interrupt */
	base->USBHS_DEVIER = BIT(USBHS_DEVIER_PEP_0_Pos + ep_idx);

	return 0;
}

static int udc_sam_usbhs_ep_disable(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->addr);
	Usbhs *base = config->base;

	/* Disable global endpoint interrupt */
	base->USBHS_DEVIDR = BIT(USBHS_DEVIDR_PEP_0_Pos + ep_idx);

	/* Disable endpoint */
	base->USBHS_DEVEPT &= ~BIT(USBHS_DEVEPT_EPEN0_Pos + ep_idx);

	LOG_DBG("Disable ep%d 0x%02x", ep_idx, ep_cfg->addr);

	return 0;
}

static int udc_sam_usbhs_ep_set_halt(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->addr);
	Usbhs *base = config->base;

	base->USBHS_DEVEPTIER[ep_idx] = USBHS_DEVEPTIER_CTRL_STALLRQS_Msk;

	LOG_DBG("Set halt ep 0x%02x", ep_cfg->addr);
	if (ep_idx != 0U) {
		ep_cfg->stat.halted = true;
	}

	return 0;
}

static int udc_sam_usbhs_ep_clear_halt(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	struct udc_sam_usbhs_data *const priv = udc_get_private(dev);
	const struct udc_sam_usbhs_config *config = dev->config;
	uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->addr);
	Usbhs *base = config->base;

	if (ep_idx == 0) {
		return 0;
	}

	base->USBHS_DEVEPTIDR[ep_idx] = USBHS_DEVEPTIDR_CTRL_STALLRQC_Msk;

	if (USB_EP_GET_IDX(ep_cfg->addr) != 0 && !udc_ep_is_busy(ep_cfg)) {
		if (udc_buf_peek(ep_cfg)) {
			atomic_set_bit(&priv->xfer_new, udc_sam_usbhs_ep_to_bnum(ep_cfg->addr));
			k_event_post(&priv->events, BIT(UDC_SAM_USBHS_EVT_XFER_NEW));
		}
	}

	LOG_DBG("Clear halt ep 0x%02x", ep_cfg->addr);
	ep_cfg->stat.halted = false;

	return 0;
}

static int udc_sam_usbhs_set_address(const struct device *dev, const uint8_t addr)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *base = config->base;

	LOG_DBG("Set new address %u for %p", addr, dev);

	base->USBHS_DEVCTRL &= ~(USBHS_DEVCTRL_UADD_Msk | USBHS_DEVCTRL_ADDEN_Msk);
	base->USBHS_DEVCTRL |= USBHS_DEVCTRL_UADD(addr);

	return 0;
}

static int udc_sam_usbhs_host_wakeup(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *base = config->base;

	LOG_DBG("Remote wakeup from %p", dev);
	base->USBHS_DEVCTRL |= USBHS_DEVCTRL_RMWKUP_Msk;

	return 0;
}

static enum udc_bus_speed udc_sam_usbhs_device_speed(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *base = config->base;

	switch (base->USBHS_SR & USBHS_SR_SPEED_Msk) {
	case USBHS_SR_SPEED_FULL_SPEED:
		return UDC_BUS_SPEED_FS;
	case USBHS_SR_SPEED_HIGH_SPEED:
		return UDC_BUS_SPEED_HS;
	case USBHS_SR_SPEED_LOW_SPEED:
		__ASSERT(false, "Low speed mode not supported");
		__fallthrough;
	default:
		return UDC_BUS_UNKNOWN;
	}
}

static int udc_sam_usbhs_test_mode(const struct device *dev, const uint8_t mode, const bool dryrun)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *base = config->base;
	uint32_t devctrl;

	switch (mode) {
	case USB_SFS_TEST_MODE_J:
		devctrl = USBHS_DEVCTRL_TSTJ_Msk;
		break;
	case USB_SFS_TEST_MODE_K:
		devctrl = USBHS_DEVCTRL_TSTK_Msk;
		break;
	case USB_SFS_TEST_MODE_PACKET:
		devctrl = USBHS_DEVCTRL_TSTPCKT_Msk;
		break;
	default:
		return -EINVAL;
	}

	if (dryrun) {
		LOG_DBG("Test Mode %u supported", mode);
		return 0;
	}

	base->USBHS_DEVCTRL |= devctrl;

	return 0;
}

static void udc_sam_enable_upll(void)
{
	/* Enable the UPLL @ 480 MHz and wait for it to be considered locked */
	PMC->CKGR_UCKR |= CKGR_UCKR_UPLLEN;

	while (!(PMC->PMC_SR & PMC_SR_LOCKU)) {
		k_yield();
	}
}

static int udc_sam_usbhs_enable(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *base = config->base;
	int err;

	/*
	 * Initialization according to SAM E70/S70/V70/V71 family datasheet (DS60001527H), paragraph
	 * 38.5.2.
	 */

	if (config->speed_idx == 1) {
		/* High-speed not enabled, configure the low-power mode clock @ 48 MHz */
		udc_sam_enable_upll();

		/* Configure the USB_48M clock to be UPLLCK/10 */
		PMC->PMC_MCKR &= ~PMC_MCKR_UPLLDIV2;
		PMC->PMC_USB = PMC_USB_USBDIV(9) | PMC_USB_USBS;
	}

	/* Enable the USBHS peripheral clock via PMC_PCER. */
	err = clock_control_on(SAM_DT_PMC_CONTROLLER, (clock_control_subsys_t)&config->clock_cfg);
	if (err != 0) {
		return err;
	}

	/* Ensure the USBHS is in reset state */
	base->USBHS_CTRL = USBHS_CTRL_UIMOD_Msk | USBHS_CTRL_FRZCLK_Msk | USBHS_CTRL_VBUSHWC_Msk;
	barrier_dsync_fence_full();

	if (config->speed_idx == 1) {
		/* High-speed not enabled, use low-power mode */
		base->USBHS_DEVCTRL |= USBHS_DEVCTRL_SPDCONF_LOW_POWER;
	}

	/* Enable the USBHS and unfreeze the clock */
	base->USBHS_CTRL = USBHS_CTRL_UIMOD_Msk | USBHS_CTRL_USBE_Msk | USBHS_CTRL_VBUSHWC_Msk;

	if (config->speed_idx == 2) {
		/* Enable the UPLL @ 480 MHz and wait for it to be considered locked */
		udc_sam_enable_upll();
	} else {
		/* Enable USB_48M clock */
		PMC->PMC_SCER |= PMC_SCER_USBCLK_Msk;
	}

	if (udc_ep_enable_internal(dev, USB_CONTROL_EP_OUT, USB_EP_TYPE_CONTROL, 64, 0)) {
		LOG_ERR("Failed to enable control endpoint");
		return -EIO;
	}

	if (udc_ep_enable_internal(dev, USB_CONTROL_EP_IN, USB_EP_TYPE_CONTROL, 64, 0)) {
		LOG_ERR("Failed to enable control endpoint");
		return -EIO;
	}

	/* Enable device interrupts */
	base->USBHS_DEVIER =
		USBHS_DEVIER_EORSMES_Msk | USBHS_DEVIER_EORSTES_Msk | USBHS_DEVIER_SUSPES_Msk;

	if (IS_ENABLED(CONFIG_UDC_ENABLE_SOF)) {
		base->USBHS_DEVIER = USBHS_DEVIER_SOFES_Msk;
	}

	/* Attach the device */
	base->USBHS_DEVCTRL &= ~USBHS_DEVCTRL_DETACH_Msk;

	config->irq_enable_func();
	LOG_DBG("Enable device %p", dev);

	return 0;
}

static int udc_sam_usbhs_disable(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *base = config->base;
	int err;

	config->irq_disable_func();

	/* Detach the device */
	base->USBHS_DEVCTRL |= USBHS_DEVCTRL_DETACH_Msk;

	if (udc_ep_disable_internal(dev, USB_CONTROL_EP_OUT)) {
		LOG_ERR("Failed to disable control endpoint");
		return -EIO;
	}

	if (udc_ep_disable_internal(dev, USB_CONTROL_EP_IN)) {
		LOG_ERR("Failed to disable control endpoint");
		return -EIO;
	}

	if (config->speed_idx == 1) {
		/* Disable USB_48M clock */
		PMC->PMC_SCER &= ~PMC_SCER_USBCLK_Msk;
	}

	/* Disable the UPLL */
	PMC->CKGR_UCKR &= ~CKGR_UCKR_UPLLEN_Msk;

	/* Disable the USBHS and freeze the clock */
	base->USBHS_CTRL = USBHS_CTRL_UIMOD_Msk | USBHS_CTRL_FRZCLK_Msk | USBHS_CTRL_VBUSHWC_Msk;

	/* Disable the USBHS peripheral clock via PMC_PCER. */
	err = clock_control_off(SAM_DT_PMC_CONTROLLER, (clock_control_subsys_t)&config->clock_cfg);
	if (err != 0) {
		return err;
	}

	LOG_DBG("Disable device %p", dev);

	return 0;
}

/*
 * Nothing to do here as the controller does not support VBUS state change
 * detection and there is nothing to initialize in the controller to do this.
 */
static int udc_sam_usbhs_init(const struct device *dev)
{
	LOG_DBG("Init device %s", dev->name);

	return 0;
}

static int udc_sam_usbhs_shutdown(const struct device *dev)
{
	LOG_DBG("Shutdown device %s", dev->name);

	return 0;
}

static int udc_sam_usbhs_driver_preinit(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	struct udc_sam_usbhs_data *priv = udc_get_private(dev);
	struct udc_data *data = dev->data;
	uint16_t mps = 1023;
	int err;

	k_mutex_init(&data->mutex);
	k_event_init(&priv->events);
	atomic_clear(&priv->xfer_new);
	atomic_clear(&priv->xfer_finished);

	data->caps.rwup = true;
	data->caps.out_ack = true;
	data->caps.addr_before_status = true;
	data->caps.mps0 = UDC_MPS0_64;

	if (config->speed_idx == 2) {
		data->caps.hs = true;
		mps = 1024;
	}

	for (int i = 0; i < config->num_of_eps; i++) {
		config->ep_cfg_out[i].caps.out = 1;
		if (i == 0) {
			config->ep_cfg_out[i].caps.control = 1;
			config->ep_cfg_out[i].caps.mps = 64;
		} else {
			config->ep_cfg_out[i].caps.bulk = 1;
			config->ep_cfg_out[i].caps.interrupt = 1;
			config->ep_cfg_out[i].caps.iso = 1;
			config->ep_cfg_out[i].caps.mps = mps;
			config->ep_cfg_out[i].caps.high_bandwidth = 1;
		}

		config->ep_cfg_out[i].addr = USB_EP_DIR_OUT | i;
		err = udc_register_ep(dev, &config->ep_cfg_out[i]);
		if (err != 0) {
			LOG_ERR("Failed to register endpoint");
			return err;
		}
	}

	for (int i = 0; i < config->num_of_eps; i++) {
		config->ep_cfg_in[i].caps.in = 1;
		if (i == 0) {
			config->ep_cfg_in[i].caps.control = 1;
			config->ep_cfg_in[i].caps.mps = 64;
		} else {
			config->ep_cfg_in[i].caps.bulk = 1;
			config->ep_cfg_in[i].caps.interrupt = 1;
			config->ep_cfg_in[i].caps.iso = 1;
			config->ep_cfg_in[i].caps.mps = mps;
			config->ep_cfg_in[i].caps.high_bandwidth = 1;
		}

		config->ep_cfg_in[i].addr = USB_EP_DIR_IN | i;
		err = udc_register_ep(dev, &config->ep_cfg_in[i]);
		if (err != 0) {
			LOG_ERR("Failed to register endpoint");
			return err;
		}
	}

	config->make_thread(dev);
	LOG_INF("Device %p (max. speed %d)", dev, config->speed_idx);

	return 0;
}

static void udc_sam_usbhs_lock(const struct device *dev)
{
	k_sched_lock();
	udc_lock_internal(dev, K_FOREVER);
}

static void udc_sam_usbhs_unlock(const struct device *dev)
{
	udc_unlock_internal(dev);
	k_sched_unlock();
}

static const struct udc_api udc_sam_usbhs_api = {
	.lock = udc_sam_usbhs_lock,
	.unlock = udc_sam_usbhs_unlock,
	.device_speed = udc_sam_usbhs_device_speed,
	.init = udc_sam_usbhs_init,
	.enable = udc_sam_usbhs_enable,
	.disable = udc_sam_usbhs_disable,
	.shutdown = udc_sam_usbhs_shutdown,
	.set_address = udc_sam_usbhs_set_address,
	.test_mode = udc_sam_usbhs_test_mode,
	.host_wakeup = udc_sam_usbhs_host_wakeup,
	.ep_enable = udc_sam_usbhs_ep_enable,
	.ep_disable = udc_sam_usbhs_ep_disable,
	.ep_set_halt = udc_sam_usbhs_ep_set_halt,
	.ep_clear_halt = udc_sam_usbhs_ep_clear_halt,
	.ep_enqueue = udc_sam_usbhs_ep_enqueue,
	.ep_dequeue = udc_sam_usbhs_ep_dequeue,
};

#define DT_DRV_COMPAT atmel_sam_usbhs

#define UDC_SAM_USBHS_DEVICE_DEFINE(n)                                                             \
	K_THREAD_STACK_DEFINE(udc_sam_usbhs_stack_##n, CONFIG_UDC_SAM_USBHS_STACK_SIZE);           \
                                                                                                   \
	static void udc_sam_usbhs_irq_enable_##n(void)                                             \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), udc_sam_usbhs_isr_handler,  \
			    DEVICE_DT_INST_GET(n), 0);                                             \
		irq_enable(DT_INST_IRQN(n));                                                       \
	}                                                                                          \
                                                                                                   \
	static void udc_sam_usbhs_irq_disable_##n(void)                                            \
	{                                                                                          \
		irq_disable(DT_INST_IRQN(n));                                                      \
	}                                                                                          \
                                                                                                   \
	static void udc_sam_usbhs_thread_##n(void *dev, void *arg1, void *arg2)                    \
	{                                                                                          \
		while (true) {                                                                     \
			udc_sam_usbhs_thread_handler(dev);                                         \
		}                                                                                  \
	}                                                                                          \
                                                                                                   \
	static void udc_sam_usbhs_make_thread_##n(const struct device *dev)                        \
	{                                                                                          \
		struct udc_sam_usbhs_data *priv = udc_get_private(dev);                            \
                                                                                                   \
		k_thread_create(&priv->thread_data, udc_sam_usbhs_stack_##n,                       \
				K_THREAD_STACK_SIZEOF(udc_sam_usbhs_stack_##n),                    \
				udc_sam_usbhs_thread_##n, (void *)dev, NULL, NULL,                 \
				K_PRIO_COOP(CONFIG_UDC_SAM_USBHS_THREAD_PRIORITY), K_ESSENTIAL,    \
				K_NO_WAIT);                                                        \
		k_thread_name_set(&priv->thread_data, dev->name);                                  \
	}                                                                                          \
                                                                                                   \
	static struct udc_ep_config ep_cfg_out[DT_INST_PROP(n, num_bidir_endpoints)];              \
	static struct udc_ep_config ep_cfg_in[DT_INST_PROP(n, num_bidir_endpoints)];               \
                                                                                                   \
	static const struct udc_sam_usbhs_config udc_sam_usbhs_config_##n = {                      \
		.base = (Usbhs *)DT_INST_REG_ADDR_BY_NAME(n, usbhs),                               \
		.dpram = DT_INST_REG_ADDR_BY_NAME(n, usbhs_ram),                                   \
		.clock_cfg = SAM_DT_INST_CLOCK_PMC_CFG(n),                                         \
		.num_of_eps = DT_INST_PROP(n, num_bidir_endpoints),                                \
		.ep_cfg_in = ep_cfg_out,                                                           \
		.ep_cfg_out = ep_cfg_in,                                                           \
		.speed_idx = COND_CODE_1(CONFIG_UDC_DRIVER_HIGH_SPEED_SUPPORT_ENABLED,             \
					 (DT_ENUM_IDX_OR(DT_DRV_INST(n), maximum_speed,            \
							 UDC_BUS_SPEED_HS)), (UDC_BUS_SPEED_FS)),  \
		.make_thread = udc_sam_usbhs_make_thread_##n,                                      \
		.irq_enable_func = udc_sam_usbhs_irq_enable_##n,                                   \
		.irq_disable_func = udc_sam_usbhs_irq_disable_##n,                                 \
	};                                                                                         \
                                                                                                   \
	static struct udc_sam_usbhs_data udc_priv_##n;                                             \
                                                                                                   \
	static struct udc_data udc_data_##n = {                                                    \
		.mutex = Z_MUTEX_INITIALIZER(udc_data_##n.mutex),                                  \
		.priv = &udc_priv_##n,                                                             \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, udc_sam_usbhs_driver_preinit, NULL, &udc_data_##n,                \
			      &udc_sam_usbhs_config_##n, POST_KERNEL,                              \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &udc_sam_usbhs_api);

DT_INST_FOREACH_STATUS_OKAY(UDC_SAM_USBHS_DEVICE_DEFINE)
