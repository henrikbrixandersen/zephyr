/*
 * Copyright (c) 2025 Henrik Brix Andersen <henrik@brixandersen.dk>
 * Copyright (c) 2018 Aurelien Jarno <aurelien@aurel32.net>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "udc_common.h"

#include <string.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/clock_control/atmel_sam_pmc.h>
#include <zephyr/drivers/usb/udc.h>
#include <soc.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(udc_sam_usbhs, CONFIG_UDC_DRIVER_LOG_LEVEL);

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

/*
 * Structure to hold driver private data.
 * Note that this is not accessible via dev->data, but as
 *   struct udc_sam_usbhs_data *priv = udc_get_private(dev);
 */
struct udc_sam_usbhs_data {
	struct k_thread thread_data;

	struct k_event events;
	atomic_t xfer_new;
	atomic_t xfer_finished;
};

/*
 * You can use one thread per driver instance model or UDC driver workqueue,
 * whichever model suits your needs best. If you decide to use the UDC workqueue,
 * enable Kconfig option UDC_WORKQUEUE and remove the handler below and
 * caller from the UDC_SAM_USBHS_DEVICE_DEFINE macro.
 */
static ALWAYS_INLINE void udc_sam_usbhs_thread_handler(void *const arg)
{
	/*const struct device *dev = (const struct device *)arg;*/

	/* TODO */
	k_msleep(1000);
}

static void udc_sam_usbhs_isr_handler(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *const base = config->base;
	uint32_t sr;

	sr = base->USBHS_DEVISR;
	base->USBHS_DEVICR = sr;

	/* TODO */
	LOG_INF("isr: 0x%08x", sr);
}

/*
 * This is called in the context of udc_ep_enqueue() and must
 * not block. The driver can immediately claim the buffer if the queue is empty,
 * but usually it is offloaded to a thread or workqueue to handle transfers
 * in a single location. Please refer to existing driver implementations
 * for examples.
 */
static int udc_sam_usbhs_ep_enqueue(const struct device *dev, struct udc_ep_config *const ep_cfg,
				    struct net_buf *buf)
{
	LOG_DBG("%p enqueue %p", dev, buf);
	udc_buf_put(ep_cfg, buf);

	/* TODO */

	if (ep_cfg->stat.halted) {
		/*
		 * It is fine to enqueue a transfer for a halted endpoint,
		 * you need to make sure that transfers are retriggered when
		 * the halt is cleared.
		 *
		 * Always use the abbreviation 'ep' for the endpoint address
		 * and 'ep_idx' or 'ep_num' for the endpoint number identifiers.
		 * Although struct udc_ep_config uses address to be unambiguous
		 * in its context.
		 */
		LOG_DBG("ep 0x%02x halted", ep_cfg->addr);
		return 0;
	}

	return 0;
}

/*
 * This is called in the context of udc_ep_dequeue()
 * and must remove all requests from an endpoint queue
 * Successful removal should be reported to the higher level with
 * ECONNABORTED as the request result.
 * It is up to the request owner to clean up or reuse the buffer.
 */
static int udc_sam_usbhs_ep_dequeue(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	unsigned int lock_key;
	struct net_buf *buf;

	lock_key = irq_lock();

	buf = udc_buf_get_all(ep_cfg);
	if (buf) {
		udc_submit_ep_event(dev, buf, -ECONNABORTED);
	}

	/* TODO */

	irq_unlock(lock_key);

	return 0;
}

/*
 * Configure and make an endpoint ready for use.
 * This is called in the context of udc_ep_enable() or udc_ep_enable_internal(),
 * the latter of which may be used by the driver to enable control endpoints.
 */
static int udc_sam_usbhs_ep_enable(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	LOG_DBG("Enable ep 0x%02x", ep_cfg->addr);

	/* TODO */

	return 0;
}

/*
 * Opposite function to udc_sam_usbhs_ep_enable(). udc_ep_disable_internal()
 * may be used by the driver to disable control endpoints.
 */
static int udc_sam_usbhs_ep_disable(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	LOG_DBG("Disable ep 0x%02x", ep_cfg->addr);

	/* TODO */

	return 0;
}

static int udc_sam_usbhs_ep_set_halt(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->addr);
	Usbhs *const base = config->base;

	base->USBHS_DEVEPTIER[ep_idx] = USBHS_DEVEPTIER_CTRL_STALLRQS;

	LOG_DBG("Set halt ep 0x%02x", ep_cfg->addr);
	if (ep_idx != 0U) {
		ep_cfg->stat.halted = true;
	}

	return 0;
}

static int udc_sam_usbhs_ep_clear_halt(const struct device *dev, struct udc_ep_config *const ep_cfg)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->addr);
	Usbhs *const base = config->base;

	if (ep_idx == 0) {
		return 0;
	}

	base->USBHS_DEVEPTIDR[ep_idx] = USBHS_DEVEPTIDR_CTRL_STALLRQC;

	/* TODO: check endpoint queue for pending request, post event */

	LOG_DBG("Clear halt ep 0x%02x", ep_cfg->addr);
	ep_cfg->stat.halted = false;

	/* TODO */

	return 0;
}

static int udc_sam_usbhs_set_address(const struct device *dev, const uint8_t addr)
{
	LOG_DBG("Set new address %u for %p", addr, dev);

	/* TODO */

	return 0;
}

static int udc_sam_usbhs_host_wakeup(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *const base = config->base;

	LOG_DBG("Remote wakeup from %p", dev);
	base->USBHS_DEVCTRL |= USBHS_DEVCTRL_RMWKUP;

	return 0;
}

static enum udc_bus_speed udc_sam_usbhs_device_speed(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *const base = config->base;

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

static int udc_sam_usbhs_enable(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *const base = config->base;
	int err;

	/* TODO: follow paragraph 38.5.2 */

	/* Enable USBHS clock in PMC */
	err = clock_control_on(SAM_DT_PMC_CONTROLLER, (clock_control_subsys_t)&config->clock_cfg);
	if (err != 0) {
		return err;
	}

	/* Reset the controller */
	base->USBHS_CTRL = USBHS_CTRL_UIMOD | USBHS_CTRL_FRZCLK | USBHS_CTRL_VBUSHWC;

	/* Start the USB PLL */
	PMC->CKGR_UCKR |= CKGR_UCKR_UPLLEN;

	/* Wait for it to be ready */
	while (!(PMC->PMC_SR & PMC_SR_LOCKU)) {
		k_yield();
	}

	/* TODO: use max speed property here instead */
	/* In low power mode, provide a 48MHz clock instead of the 480MHz one */
	if ((base->USBHS_DEVCTRL & USBHS_DEVCTRL_SPDCONF_Msk) == USBHS_DEVCTRL_SPDCONF_LOW_POWER) {
		/* Configure the USB_48M clock to be UPLLCK/10 */
		PMC->PMC_MCKR &= ~PMC_MCKR_UPLLDIV2;
		PMC->PMC_USB = PMC_USB_USBDIV(9) | PMC_USB_USBS;

		/* Enable USB_48M clock */
		PMC->PMC_SCER |= PMC_SCER_USBCLK;
	}

	/* Unfreeze the clock */
	base->USBHS_CTRL = USBHS_CTRL_UIMOD | USBHS_CTRL_USBE | USBHS_CTRL_VBUSHWC;

	if (udc_ep_enable_internal(dev, USB_CONTROL_EP_OUT, USB_EP_TYPE_CONTROL, 64, 0)) {
		LOG_ERR("Failed to enable control endpoint");
		return -EIO;
	}

	if (udc_ep_enable_internal(dev, USB_CONTROL_EP_IN, USB_EP_TYPE_CONTROL, 64, 0)) {
		LOG_ERR("Failed to enable control endpoint");
		return -EIO;
	}

	/* Enable device interrupts */
	base->USBHS_DEVIER = USBHS_DEVIER_EORSMES | USBHS_DEVIER_EORSTES | USBHS_DEVIER_SUSPES;

	if (IS_ENABLED(CONFIG_UDC_ENABLE_SOF)) {
		base->USBHS_DEVIER = USBHS_DEVIER_SOFES;
	}

	/* Attach the device */
	base->USBHS_DEVCTRL &= ~USBHS_DEVCTRL_DETACH;

	config->irq_enable_func();
	LOG_DBG("Enable device %p", dev);

	return 0;
}

static int udc_sam_usbhs_disable(const struct device *dev)
{
	const struct udc_sam_usbhs_config *config = dev->config;
	Usbhs *const base = config->base;
	int err;

	/* TODO: follow paragraph 38.5.2 */

	config->irq_disable_func();

	/* Detach the device */
	base->USBHS_DEVCTRL |= USBHS_DEVCTRL_DETACH;

	if (udc_ep_disable_internal(dev, USB_CONTROL_EP_OUT)) {
		LOG_ERR("Failed to disable control endpoint");
		return -EIO;
	}

	if (udc_ep_disable_internal(dev, USB_CONTROL_EP_IN)) {
		LOG_ERR("Failed to disable control endpoint");
		return -EIO;
	}

	/* Disable USB_48M clock */
	PMC->PMC_SCER &= ~PMC_SCER_USBCLK;

	/* Disable the USB PLL */
	PMC->CKGR_UCKR &= ~CKGR_UCKR_UPLLEN;

	/* Disable the USB controller and freeze the clock */
	base->USBHS_CTRL = USBHS_CTRL_UIMOD | USBHS_CTRL_FRZCLK | USBHS_CTRL_VBUSHWC;

	/* Disable USBHS clock in PMC */
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
		.speed_idx = DT_ENUM_IDX(DT_DRV_INST(n), maximum_speed),                           \
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
