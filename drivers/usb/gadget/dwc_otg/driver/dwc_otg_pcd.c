/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/dwc_otg_pcd.c $
 * $Revision: #88 $
 * $Date: 2010/03/09 $
 * $Change: 1458980 $
 *
 * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================== */
#ifndef DWC_HOST_ONLY

/** @file
 * This file implements PCD Core. All code in this file is portable and doesn't
 * use any OS specific functions.
 * PCD Core provides Interface, defined in <code><dwc_otg_pcd_if.h></code>
 * header file, which can be used to implement OS specific PCD interface.
 *
 * An important function of the PCD is managing interrupts generated
 * by the DWC_otg controller. The implementation of the DWC_otg device
 * mode interrupt service routines is in dwc_otg_pcd_intr.c.
 *
 * @todo Add Device Mode test modes (Test J mode, Test K mode, etc).
 * @todo Does it work when the request size is greater than DEPTSIZ
 * transfer size
 *
 */

#include "dwc_otg_pcd.h"

#ifdef DWC_UTE_CFI
#include "dwc_otg_cfi.h"

extern int init_cfi(cfiobject_t *cfiobj);
#endif

/**
 * Choose endpoint from ep arrays using usb_ep structure.
 */
static dwc_otg_pcd_ep_t *get_ep_from_handle(dwc_otg_pcd_t *pcd, void *handle)
{
	int i;
	if (pcd->ep0.priv == handle)
		return &pcd->ep0;

	for (i = 0; i < MAX_EPS_CHANNELS - 1; i++) {
		if (pcd->in_ep[i].priv == handle)
			return &pcd->in_ep[i];
		if (pcd->out_ep[i].priv == handle)
			return &pcd->out_ep[i];
	}

	return NULL;
}

/**
 * This function completes a request.  It call's the request call back.
 */
void dwc_otg_request_done(dwc_otg_pcd_ep_t *ep, dwc_otg_pcd_request_t *req,
			  int32_t status)
{
	unsigned stopped = ep->stopped;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, ep);
	DWC_CIRCLEQ_REMOVE_INIT(&ep->queue, req, queue_entry);

	/* don't modify queue heads during completion callback */
	ep->stopped = 1;
	DWC_SPINUNLOCK(ep->pcd->lock);
	ep->pcd->fops->complete(ep->pcd, ep->priv, req->priv, status,
				req->actual);
	DWC_SPINLOCK(ep->pcd->lock);

	if (ep->pcd->request_pending > 0)
		--ep->pcd->request_pending;

	ep->stopped = stopped;
	dwc_free(req);
}

/**
 * This function terminates all the requsts in the EP request queue.
 */
void dwc_otg_request_nuke(dwc_otg_pcd_ep_t *ep)
{
	dwc_otg_pcd_request_t *req;

	ep->stopped = 1;

	/* called with irqs blocked?? */
	while (!DWC_CIRCLEQ_EMPTY(&ep->queue)) {
		req = DWC_CIRCLEQ_FIRST(&ep->queue);
		dwc_otg_request_done(ep, req, -DWC_E_SHUTDOWN);

		/* done would change the first in the queue but if it is same then avoid accessing freed element */
		if (req == DWC_CIRCLEQ_FIRST(&ep->queue))
			break;
	}
}

void dwc_otg_pcd_start(dwc_otg_pcd_t *pcd,
		       const struct dwc_otg_pcd_function_ops *fops)
{
	pcd->fops = fops;
}

/**
 * PCD Callback function for initializing the PCD when switching to
 * device mode.
 *
 * @param p void pointer to the <code>dwc_otg_pcd_t</code>
 */
static int32_t dwc_otg_pcd_start_cb(void *p)
{
	dwc_otg_pcd_t *pcd = (dwc_otg_pcd_t *)p;
	dwc_otg_core_if_t *core_if = GET_CORE_IF(pcd);

	/*
	 * Initialized the Core for Device mode.
	 */
	if (dwc_otg_is_device_mode(core_if))
		dwc_otg_core_dev_init(core_if);

	if (core_if->core_params->otg_supp_enable) {
		/* Now it is okay to connect */
		dwc_otg_pcd_disconnect(pcd, false);
	} else {
		dwc_otg_pcd_disconnect(pcd,
				       core_if->
				       gadget_pullup_on ? false : true);
	}

	return 1;
}

/** CFI-specific buffer allocation function for EP */
#ifdef DWC_UTE_CFI
uint8_t *cfiw_ep_alloc_buffer(dwc_otg_pcd_t *pcd, void *pep, dwc_dma_t *addr,
			      size_t buflen, int flags)
{
	dwc_otg_pcd_ep_t *ep;
	ep = get_ep_from_handle(pcd, pep);
	if (!ep) {
		DWC_WARN("bad ep\n");
		return -DWC_E_INVALID;
	}

	return pcd->cfi->ops.ep_alloc_buf(pcd->cfi, pcd, ep, addr, buflen,
					  flags);
}
#else
uint8_t *cfiw_ep_alloc_buffer(dwc_otg_pcd_t *pcd, void *pep, dwc_dma_t *addr,
			      size_t buflen, int flags);
#endif

/**
 * PCD Callback function for notifying the PCD when resuming from
 * suspend.
 *
 * @param p void pointer to the <code>dwc_otg_pcd_t</code>
 */
static int32_t dwc_otg_pcd_resume_cb(void *p)
{
	dwc_otg_pcd_t *pcd = (dwc_otg_pcd_t *)p;

	if (pcd->fops->resume)
		pcd->fops->resume(pcd);

	/* Stop the SRP timeout timer. */
	if ((GET_CORE_IF(pcd)->core_params->phy_type != DWC_PHY_TYPE_PARAM_FS)
	    || (!GET_CORE_IF(pcd)->core_params->i2c_enable)) {
		if (GET_CORE_IF(pcd)->srp_timer_started) {
			GET_CORE_IF(pcd)->srp_timer_started = 0;
			DWC_TIMER_CANCEL(GET_CORE_IF(pcd)->srp_timer);
		}
	}
	return 1;
}

/**
 * PCD Callback function for notifying the PCD device is suspended.
 *
 * @param p void pointer to the <code>dwc_otg_pcd_t</code>
 */
static int32_t dwc_otg_pcd_suspend_cb(void *p)
{
	dwc_otg_pcd_t *pcd = (dwc_otg_pcd_t *)p;
#ifdef CONFIG_USB_OTG
	gotgctl_data_t gotgctl = {.d32 = 0 };

	if (pcd->b_hnp_enable && (pcd->core_if->op_state == B_PERIPHERAL)) {
		/* Do HNP */
		DWC_DEBUGPL(DBG_PCD, "Request B HNP\n");
		gotgctl.d32 =
		    dwc_read_reg32(&pcd->core_if->core_global_regs->gotgctl);
		gotgctl.b.devhnpen = 1;
		gotgctl.b.hnpreq = 1;
		dwc_write_reg32(&pcd->core_if->core_global_regs->gotgctl,
				gotgctl.d32);
		/* Clear hnp test mode */
		pcd->otg_hnp_reqd = 0;
		/* Remain soft disconnected
		 * so we don't miss reset after reverting
		 * back to B_PERIPHERAL */
		dwc_otg_pcd_disconnect(pcd, true);
	}
#endif

	if (pcd->fops->suspend)
		pcd->fops->suspend(pcd);

	return 1;
}

/**
 * PCD Callback function for stopping the PCD when switching to Host
 * mode.
 *
 * @param p void pointer to the <code>dwc_otg_pcd_t</code>
 */
static int32_t dwc_otg_pcd_stop_cb(void *p)
{
	dwc_otg_pcd_t *pcd = (dwc_otg_pcd_t *)p;
	extern void dwc_otg_pcd_stop(dwc_otg_pcd_t *_pcd);

	dwc_otg_pcd_stop(pcd);
	return 1;
}

#ifdef CONFIG_USB_PCD_SETTINGS
/**
 * PCD Callback function for cleaning up the PCD when USB mode switching.
 *
 * @param p void pointer to the <code>dwc_otg_pcd_t</code>
 */
static int32_t dwc_otg_pcd_clean_cb(void *p)
{
	dwc_otg_pcd_t *pcd = (dwc_otg_pcd_t *) p;

	dwc_otg_pcd_clean(pcd);
	return 1;
}
#endif

/**
 * PCD Callback structure for handling mode switching.
 */
static dwc_otg_cil_callbacks_t pcd_callbacks = {
	.start = dwc_otg_pcd_start_cb,
	.stop = dwc_otg_pcd_stop_cb,
	.suspend = dwc_otg_pcd_suspend_cb,
	.resume_wakeup = dwc_otg_pcd_resume_cb,
#ifdef CONFIG_USB_PCD_SETTINGS
	.clean = dwc_otg_pcd_clean_cb,
#endif
	.p = 0,			/* Set at registration */
};

/**
 * This function allocates a DMA Descriptor chain for the Endpoint
 * buffer to be used for a transfer to/from the specified endpoint.
 */
dwc_otg_dev_dma_desc_t *dwc_otg_ep_alloc_desc_chain(uint32_t *dma_desc_addr,
						    uint32_t count,
						    int atomic_alloc)
{
	if (atomic_alloc)
		return dwc_dma_alloc_atomic(count *
					    sizeof(dwc_otg_dev_dma_desc_t),
					    dma_desc_addr);
	else
		return dwc_dma_alloc(count * sizeof(dwc_otg_dev_dma_desc_t),
				     dma_desc_addr);
}

/**
 * This function frees a DMA Descriptor chain that was allocated by ep_alloc_desc.
 */
void dwc_otg_ep_free_desc_chain(dwc_otg_dev_dma_desc_t *desc_addr,
				uint32_t dma_desc_addr, uint32_t count)
{
	dwc_dma_free(count * sizeof(dwc_otg_dev_dma_desc_t), desc_addr,
		     dma_desc_addr);
}

static void dwc_otg_pcd_init_ep(dwc_otg_pcd_t *pcd, dwc_otg_pcd_ep_t *pcd_ep,
				uint32_t is_in, uint32_t ep_num)
{
	/* Init EP structure */
	pcd_ep->desc = 0;
	pcd_ep->pcd = pcd;
	pcd_ep->stopped = 1;
	pcd_ep->queue_sof = 0;

	/* Init DWC ep structure */
	pcd_ep->dwc_ep.is_in = is_in;
	pcd_ep->dwc_ep.num = ep_num;
	pcd_ep->dwc_ep.active = 0;
	pcd_ep->dwc_ep.tx_fifo_num = 0;
	/* Control until ep is actvated */
	pcd_ep->dwc_ep.type = DWC_OTG_EP_TYPE_CONTROL;
	pcd_ep->dwc_ep.maxpacket = MAX_PACKET_SIZE;
	pcd_ep->dwc_ep.dma_addr = 0;
	pcd_ep->dwc_ep.start_xfer_buff = 0;
	pcd_ep->dwc_ep.xfer_buff = 0;
	pcd_ep->dwc_ep.xfer_len = 0;
	pcd_ep->dwc_ep.xfer_count = 0;
	pcd_ep->dwc_ep.sent_zlp = 0;
	pcd_ep->dwc_ep.total_len = 0;
	pcd_ep->dwc_ep.desc_addr = 0;
	pcd_ep->dwc_ep.dma_desc_addr = 0;
	DWC_CIRCLEQ_INIT(&pcd_ep->queue);
}

/**
 * Initialize ep's
 */
static void dwc_otg_pcd_reinit(dwc_otg_pcd_t *pcd)
{
	int i;
	uint32_t hwcfg1;
	dwc_otg_pcd_ep_t *ep;
	int in_ep_cntr, out_ep_cntr;
	uint32_t num_in_eps = (GET_CORE_IF(pcd))->dev_if->num_in_eps;
	uint32_t num_out_eps = (GET_CORE_IF(pcd))->dev_if->num_out_eps;

	/**
	 * Initialize the EP0 structure.
	 */
	ep = &pcd->ep0;
	dwc_otg_pcd_init_ep(pcd, ep, 0, 0);

	in_ep_cntr = 0;
	hwcfg1 = (GET_CORE_IF(pcd))->hwcfg1.d32 >> 3;
	for (i = 1; in_ep_cntr < num_in_eps; i++) {
		if ((hwcfg1 & 0x1) == 0) {
			dwc_otg_pcd_ep_t *ep = &pcd->in_ep[in_ep_cntr];
			in_ep_cntr++;
			/**
			 * @todo NGS: Add direction to EP, based on contents
			 * of HWCFG1.  Need a copy of HWCFG1 in pcd structure?
			 * sprintf(";r
			 */
			dwc_otg_pcd_init_ep(pcd, ep, 1 /* IN */ , i);

			DWC_CIRCLEQ_INIT(&ep->queue);
		}
		hwcfg1 >>= 2;
	}

	out_ep_cntr = 0;
	hwcfg1 = (GET_CORE_IF(pcd))->hwcfg1.d32 >> 2;
	for (i = 1; out_ep_cntr < num_out_eps; i++) {
		if ((hwcfg1 & 0x1) == 0) {
			dwc_otg_pcd_ep_t *ep = &pcd->out_ep[out_ep_cntr];
			out_ep_cntr++;
			/**
			 * @todo NGS: Add direction to EP, based on contents
			 * of HWCFG1.  Need a copy of HWCFG1 in pcd structure?
			 * sprintf(";r
			 */
			dwc_otg_pcd_init_ep(pcd, ep, 0 /* OUT */ , i);
			DWC_CIRCLEQ_INIT(&ep->queue);
		}
		hwcfg1 >>= 2;
	}

	pcd->ep0state = EP0_DISCONNECT;
	pcd->ep0.dwc_ep.maxpacket = MAX_EP0_SIZE;
	pcd->ep0.dwc_ep.type = DWC_OTG_EP_TYPE_CONTROL;
}

/**
 * This function is called when the SRP timer expires. The SRP should
 * complete within 6 seconds.
 */
static void srp_timeout(void *ptr)
{
	gotgctl_data_t gotgctl;
	dwc_otg_core_if_t *core_if = (dwc_otg_core_if_t *)ptr;
	volatile uint32_t *addr = &core_if->core_global_regs->gotgctl;

	gotgctl.d32 = dwc_read_reg32(addr);

	core_if->srp_timer_started = 0;

	if ((core_if->core_params->phy_type == DWC_PHY_TYPE_PARAM_FS) &&
	    (core_if->core_params->i2c_enable)) {
		DWC_PRINTF("SRP Timeout\n");

		if ((core_if->srp_success) && (gotgctl.b.bsesvld)) {
			if (core_if->pcd_cb && core_if->pcd_cb->resume_wakeup) {
				core_if->pcd_cb->resume_wakeup(core_if->pcd_cb->
							       p);
			}

			/* Clear Session Request */
			gotgctl.d32 = 0;
			gotgctl.b.sesreq = 1;
			dwc_modify_reg32(&core_if->core_global_regs->gotgctl,
					 gotgctl.d32, 0);

			core_if->srp_success = 0;
		} else {
			__DWC_ERROR("Device not connected/responding\n");
			gotgctl.b.sesreq = 0;
			dwc_write_reg32(addr, gotgctl.d32);
		}
	} else if (gotgctl.b.sesreq) {
		DWC_PRINTF("SRP Timeout\n");

		__DWC_ERROR("Device not connected/responding\n");
		gotgctl.b.sesreq = 0;
		dwc_write_reg32(addr, gotgctl.d32);
	} else {
		DWC_PRINTF(" SRP GOTGCTL=%0x\n", gotgctl.d32);
	}

	if (core_if->adp_enable) {
		if (gotgctl.b.bsesvld == 0) {
			gpwrdn_data_t gpwrdn = {.d32 = 0 };

			/* Power off the core */
			if (core_if->power_down == 2) {
				gpwrdn.b.pwrdnswtch = 1;
				dwc_modify_reg32(&core_if->core_global_regs->
						 gpwrdn, gpwrdn.d32, 0);
			}

			gpwrdn.d32 = 0;
			gpwrdn.b.pmuactv = 1;
			dwc_modify_reg32(&core_if->core_global_regs->gpwrdn, 0,
					 gpwrdn.d32);

			dwc_otg_adp_probe_start(core_if);
		} else {
			core_if->op_state = B_PERIPHERAL;
			dwc_otg_core_init(core_if);
			dwc_otg_enable_global_interrupts(core_if);
			cil_pcd_start(core_if);
		}
	}
}

/**
 * Tasklet
 *
 */
extern void start_next_request(dwc_otg_pcd_ep_t *ep);

static void start_xfer_tasklet_func(void *data)
{
	dwc_otg_pcd_t *pcd = (dwc_otg_pcd_t *)data;
	dwc_otg_core_if_t *core_if = GET_CORE_IF(pcd);

	int i;
	depctl_data_t diepctl;

	DWC_DEBUGPL(DBG_PCDV, "Start xfer tasklet\n");

	diepctl.d32 = dwc_read_reg32(&core_if->dev_if->in_ep_regs[0]->diepctl);

	if (pcd->ep0.queue_sof) {
		pcd->ep0.queue_sof = 0;
		start_next_request(&pcd->ep0);
		/* break; */
	}

	for (i = 0; i < core_if->dev_if->num_in_eps; i++) {
		depctl_data_t diepctl;
		diepctl.d32 =
		    dwc_read_reg32(&core_if->dev_if->in_ep_regs[i]->diepctl);

		if (pcd->in_ep[i].queue_sof) {
			pcd->in_ep[i].queue_sof = 0;
			start_next_request(&pcd->in_ep[i]);
			/* break; */
		}
	}

	return;
}

/**
 * This function initialized the PCD portion of the driver.
 *
 */
dwc_otg_pcd_t *dwc_otg_pcd_init(dwc_otg_core_if_t *core_if)
{
	dwc_otg_pcd_t *pcd = NULL;
	dwc_otg_dev_if_t *dev_if;

	/*
	 * Allocate PCD structure
	 */
	pcd = dwc_alloc(sizeof(dwc_otg_pcd_t));

	if (pcd == NULL)
		return NULL;

	pcd->lock = DWC_SPINLOCK_ALLOC();
	pcd->core_if = core_if;
	if (!pcd->lock) {
		DWC_ERROR("Could not allocate lock for pcd");
		dwc_free(pcd);
		return NULL;
	}
	dev_if = core_if->dev_if;

	if (core_if->hwcfg4.b.ded_fifo_en)
		DWC_DEBUGPL(DBG_CIL, "Dedicated Tx FIFOs mode\n");
	else
		DWC_DEBUGPL(DBG_CIL, "Shared Tx FIFO mode\n");

	/*
	 * Initialized the Core for Device mode.
	 */
	if (dwc_otg_is_device_mode(core_if))
		dwc_otg_core_dev_init(core_if);

	/*
	 * Register the PCD Callbacks.
	 */
	dwc_otg_cil_register_pcd_callbacks(core_if, &pcd_callbacks, pcd);

	/*
	 * Initialize the DMA buffer for SETUP packets
	 */
	if (GET_CORE_IF(pcd)->dma_enable) {
		pcd->setup_pkt =
		    dwc_dma_alloc(sizeof(*pcd->setup_pkt) * 5,
				  &pcd->setup_pkt_dma_handle);
		if (pcd->setup_pkt == 0) {
			dwc_free(pcd);
			return NULL;
		}

		pcd->status_buf =
		    dwc_dma_alloc(sizeof(uint16_t),
				  &pcd->status_buf_dma_handle);
		if (pcd->status_buf == NULL) {
			dwc_dma_free(sizeof(*pcd->setup_pkt) * 5,
				     pcd->setup_pkt, pcd->setup_pkt_dma_handle);
			dwc_free(pcd);
			return NULL;
		}

		if (GET_CORE_IF(pcd)->dma_desc_enable) {
			dev_if->setup_desc_addr[0] =
			    dwc_otg_ep_alloc_desc_chain(&dev_if->
							dma_setup_desc_addr[0],
							1, 0);
			dev_if->setup_desc_addr[1] =
			    dwc_otg_ep_alloc_desc_chain(&dev_if->
							dma_setup_desc_addr[1],
							1, 0);
			dev_if->in_desc_addr =
			    dwc_otg_ep_alloc_desc_chain(&dev_if->
							dma_in_desc_addr, 1, 0);
			dev_if->out_desc_addr =
			    dwc_otg_ep_alloc_desc_chain(&dev_if->
							dma_out_desc_addr, 1,
							0);

			if (dev_if->setup_desc_addr[0] == 0
			    || dev_if->setup_desc_addr[1] == 0
			    || dev_if->in_desc_addr == 0
			    || dev_if->out_desc_addr == 0) {

				if (dev_if->out_desc_addr)
					dwc_otg_ep_free_desc_chain(dev_if->
								   out_desc_addr,
								   dev_if->
								   dma_out_desc_addr,
								   1);
				if (dev_if->in_desc_addr)
					dwc_otg_ep_free_desc_chain(dev_if->
								   in_desc_addr,
								   dev_if->
								   dma_in_desc_addr,
								   1);
				if (dev_if->setup_desc_addr[1])
					dwc_otg_ep_free_desc_chain(dev_if->
								   setup_desc_addr
								   [1],
								   dev_if->
								   dma_setup_desc_addr
								   [1], 1);
				if (dev_if->setup_desc_addr[0])
					dwc_otg_ep_free_desc_chain(dev_if->
								   setup_desc_addr
								   [0],
								   dev_if->
								   dma_setup_desc_addr
								   [0], 1);

				dwc_dma_free(sizeof(*pcd->setup_pkt) * 5,
					     pcd->setup_pkt,
					     pcd->setup_pkt_dma_handle);
				dwc_dma_free(sizeof(*pcd->status_buf),
					     pcd->status_buf,
					     pcd->status_buf_dma_handle);

				dwc_free(pcd);

				return NULL;
			}
		}
	} else {
		pcd->setup_pkt = dwc_alloc(sizeof(*pcd->setup_pkt) * 5);
		if (pcd->setup_pkt == 0) {
			dwc_free(pcd);
			return NULL;
		}

		pcd->status_buf = dwc_alloc(sizeof(uint16_t));
		if (pcd->status_buf == 0) {
			dwc_free(pcd->setup_pkt);
			dwc_free(pcd);
			return NULL;
		}
	}

	dwc_otg_pcd_reinit(pcd);

	/* Allocate the cfi object for the PCD */
#ifdef DWC_UTE_CFI
	pcd->cfi = dwc_alloc(sizeof(cfiobject_t));
	if (NULL == pcd->cfi)
		return NULL;
	if (init_cfi(pcd->cfi)) {
		CFI_INFO("%s: Failed to init the CFI object\n", __func__);
		return NULL;
	}
#endif

	/* Initialize tasklets */
	pcd->start_xfer_tasklet = DWC_TASK_ALLOC(start_xfer_tasklet_func, pcd);
	pcd->test_mode_tasklet = DWC_TASK_ALLOC(do_test_mode, pcd);

	/* Initialize SRP timer */
	core_if->srp_timer = DWC_TIMER_ALLOC("SRP TIMER", srp_timeout, core_if);

	return pcd;
}

/**
 * Remove PCD specific data
 */
void dwc_otg_pcd_remove(dwc_otg_pcd_t *pcd)
{
	dwc_otg_dev_if_t *dev_if = GET_CORE_IF(pcd)->dev_if;

	if (GET_CORE_IF(pcd)->dma_enable) {
		dwc_dma_free(sizeof(*pcd->setup_pkt) * 5, pcd->setup_pkt,
			     pcd->setup_pkt_dma_handle);
		dwc_dma_free(sizeof(uint16_t), pcd->status_buf,
			     pcd->status_buf_dma_handle);
		if (GET_CORE_IF(pcd)->dma_desc_enable) {
			dwc_otg_ep_free_desc_chain(dev_if->setup_desc_addr[0],
						   dev_if->
						   dma_setup_desc_addr[0], 1);
			dwc_otg_ep_free_desc_chain(dev_if->setup_desc_addr[1],
						   dev_if->
						   dma_setup_desc_addr[1], 1);
			dwc_otg_ep_free_desc_chain(dev_if->in_desc_addr,
						   dev_if->dma_in_desc_addr, 1);
			dwc_otg_ep_free_desc_chain(dev_if->out_desc_addr,
						   dev_if->dma_out_desc_addr,
						   1);
		}
	} else {
		dwc_free(pcd->setup_pkt);
		dwc_free(pcd->status_buf);
	}
	DWC_SPINLOCK_FREE(pcd->lock);
	DWC_TASK_FREE(pcd->start_xfer_tasklet);
	DWC_TASK_FREE(pcd->test_mode_tasklet);

/* Release the CFI object's dynamic memory */
#ifdef DWC_UTE_CFI
	if (pcd->cfi->ops.release)
		pcd->cfi->ops.release(pcd->cfi);
#endif

	dwc_free(pcd);
}

/**
 * Returns whether registered pcd is dual speed or not
 */
uint32_t dwc_otg_pcd_is_dualspeed(dwc_otg_pcd_t *pcd)
{
	dwc_otg_core_if_t *core_if = GET_CORE_IF(pcd);

	if ((core_if->core_params->speed == DWC_SPEED_PARAM_FULL) ||
	    ((core_if->hwcfg2.b.hs_phy_type == 2) &&
	     (core_if->hwcfg2.b.fs_phy_type == 1) &&
	     (core_if->core_params->ulpi_fs_ls))) {
		return 0;
	}

	return 1;
}

/**
 * Returns whether registered pcd is OTG capable or not
 */
uint32_t dwc_otg_pcd_is_otg(dwc_otg_pcd_t *pcd)
{
	dwc_otg_core_if_t *core_if = GET_CORE_IF(pcd);
	gusbcfg_data_t usbcfg = {.d32 = 0 };

	usbcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->gusbcfg);
	if (!usbcfg.b.srpcap || !usbcfg.b.hnpcap
	    || !core_if->core_params->otg_supp_enable)
		return 0;

	return 1;
}

/**
 * Returns whether registered pcd is OTG2.0 capable or not
 */
uint32_t dwc_otg_pcd_is_otg20(dwc_otg_pcd_t *pcd)
{
	dwc_otg_core_if_t *core_if = GET_CORE_IF(pcd);
	gotgctl_data_t otgctl = {.d32 = 0 };

	otgctl.d32 = dwc_read_reg32(&core_if->core_global_regs->gotgctl);
	if (!otgctl.b.otgver)
		return 0;

	return 1;
}

/**
 * This function assigns periodic Tx FIFO to an periodic EP
 * in shared Tx FIFO mode
 */
static uint32_t assign_tx_fifo(dwc_otg_core_if_t *core_if)
{
	uint32_t TxMsk = 1;
	int i;

	for (i = 0; i < core_if->hwcfg4.b.num_in_eps; ++i) {
		if ((TxMsk & core_if->tx_msk) == 0) {
			core_if->tx_msk |= TxMsk;
			return i + 1;
		}
		TxMsk <<= 1;
	}
	return 0;
}

/**
 * This function assigns periodic Tx FIFO to an periodic EP
 * in shared Tx FIFO mode
 */
static uint32_t assign_perio_tx_fifo(dwc_otg_core_if_t *core_if)
{
	uint32_t PerTxMsk = 1;
	int i;
	for (i = 0; i < core_if->hwcfg4.b.num_dev_perio_in_ep; ++i) {
		if ((PerTxMsk & core_if->p_tx_msk) == 0) {
			core_if->p_tx_msk |= PerTxMsk;
			return i + 1;
		}
		PerTxMsk <<= 1;
	}
	return 0;
}

/**
 * This function releases periodic Tx FIFO
 * in shared Tx FIFO mode
 */
static void release_perio_tx_fifo(dwc_otg_core_if_t *core_if, uint32_t fifo_num)
{
	core_if->p_tx_msk =
	    (core_if->p_tx_msk & (1 << (fifo_num - 1))) ^ core_if->p_tx_msk;
}

/**
 * This function releases periodic Tx FIFO
 * in shared Tx FIFO mode
 */
static void release_tx_fifo(dwc_otg_core_if_t *core_if, uint32_t fifo_num)
{
	core_if->tx_msk =
	    (core_if->tx_msk & (1 << (fifo_num - 1))) ^ core_if->tx_msk;
}

/**
 * This function is being called from gadget
 * to enable PCD endpoint.
 */
int dwc_otg_pcd_ep_enable(dwc_otg_pcd_t *pcd,
			  const uint8_t *ep_desc, void *usb_ep)
{
	int num, dir;
	dwc_otg_pcd_ep_t *ep = 0;
	const usb_endpoint_descriptor_t *desc;
	uint64_t flags;
	int retval = 0;
	int i, epcount;

	desc = (const usb_endpoint_descriptor_t *)ep_desc;

	if (!desc) {
		pcd->ep0.priv = usb_ep;
		ep = &pcd->ep0;
		retval = -DWC_E_INVALID;
		goto out;
	}

	num = UE_GET_ADDR(desc->bEndpointAddress);
	dir = UE_GET_DIR(desc->bEndpointAddress);

	if (UGETW(desc->wMaxPacketSize) == 0) {
		DWC_WARN("bad maxpacketsize\n");
		retval = -DWC_E_INVALID;
		goto out;
	}

	if (dir == UE_DIR_IN) {
		epcount = pcd->core_if->dev_if->num_in_eps;
		for (i = 0; i < epcount; i++) {
			if (num == pcd->in_ep[i].dwc_ep.num) {
				ep = &pcd->in_ep[i];
				break;
			}
		}
	} else {
		epcount = pcd->core_if->dev_if->num_out_eps;
		for (i = 0; i < epcount; i++) {
			if (num == pcd->out_ep[i].dwc_ep.num) {
				ep = &pcd->out_ep[i];
				break;
			}
		}
	}

	if (!ep) {
		DWC_WARN("bad address\n");
		retval = -DWC_E_INVALID;
		goto out;
	}

	DWC_SPINLOCK_IRQSAVE(pcd->lock, &flags);

	ep->desc = desc;
	ep->priv = usb_ep;

	/*
	 * Activate the EP
	 */
	ep->stopped = 0;

	ep->dwc_ep.is_in = (dir == UE_DIR_IN);
	ep->dwc_ep.maxpacket = UGETW(desc->wMaxPacketSize);

	ep->dwc_ep.type = desc->bmAttributes & UE_XFERTYPE;

	if (ep->dwc_ep.is_in) {
		if (!GET_CORE_IF(pcd)->en_multiple_tx_fifo) {
			ep->dwc_ep.tx_fifo_num = 0;

			if (ep->dwc_ep.type == UE_ISOCHRONOUS) {
				/*
				 * if ISOC EP then assign a Periodic Tx FIFO.
				 */
				ep->dwc_ep.tx_fifo_num =
				    assign_perio_tx_fifo(GET_CORE_IF(pcd));
			}
		} else {
			/*
			 * if Dedicated FIFOs mode is on then assign a Tx FIFO.
			 */
			ep->dwc_ep.tx_fifo_num =
			    assign_tx_fifo(GET_CORE_IF(pcd));
		}
	}
	/* Set initial data PID. */
	if (ep->dwc_ep.type == UE_BULK)
		ep->dwc_ep.data_pid_start = 0;

	/* Alloc DMA Descriptors */
	if (GET_CORE_IF(pcd)->dma_desc_enable) {
		if (ep->dwc_ep.type != UE_ISOCHRONOUS) {
			ep->dwc_ep.desc_addr =
			    dwc_otg_ep_alloc_desc_chain(&ep->dwc_ep.
							dma_desc_addr,
							MAX_DMA_DESC_CNT, 1);
			if (!ep->dwc_ep.desc_addr) {
				DWC_WARN("%s, can't allocate DMA descriptor\n",
					 __func__);
				retval = -DWC_E_SHUTDOWN;
				DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);
				goto out;
			}
		}
	}

	DWC_DEBUGPL(DBG_PCD, "Activate %s: type=%d, mps=%d desc=%p\n",
		    (ep->dwc_ep.is_in ? "IN" : "OUT"),
		    ep->dwc_ep.type, ep->dwc_ep.maxpacket, ep->desc);
	if (ep->dwc_ep.type == DWC_OTG_EP_TYPE_ISOC) {
		ep->dwc_ep.bInterval = 1 << (ep->desc->bInterval - 1);
		ep->dwc_ep.frame_num = 0xFFFFFFFF;
	}

	dwc_otg_ep_activate(GET_CORE_IF(pcd), &ep->dwc_ep);

#ifdef DWC_UTE_CFI
	if (pcd->cfi->ops.ep_enable)
		pcd->cfi->ops.ep_enable(pcd->cfi, pcd, ep);
#endif

	DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);

out:
	return retval;
}

/**
 * This function is being called from gadget
 * to disable PCD endpoint.
 */
int dwc_otg_pcd_ep_disable(dwc_otg_pcd_t *pcd, void *ep_handle)
{
	dwc_otg_pcd_ep_t *ep;
	uint64_t flags;
	dwc_otg_dev_dma_desc_t *desc_addr;
	dwc_dma_t dma_desc_addr;

	ep = get_ep_from_handle(pcd, ep_handle);

	if (!ep || !ep->desc) {
		DWC_DEBUGPL(DBG_PCD, "bad ep address\n");
		return -DWC_E_INVALID;
	}

	DWC_SPINLOCK_IRQSAVE(pcd->lock, &flags);

	dwc_otg_request_nuke(ep);

	dwc_otg_ep_deactivate(GET_CORE_IF(pcd), &ep->dwc_ep);
	ep->desc = 0;
	ep->stopped = 1;

	if (ep->dwc_ep.is_in) {
		dwc_otg_flush_tx_fifo(GET_CORE_IF(pcd), ep->dwc_ep.tx_fifo_num);
		release_perio_tx_fifo(GET_CORE_IF(pcd), ep->dwc_ep.tx_fifo_num);
		release_tx_fifo(GET_CORE_IF(pcd), ep->dwc_ep.tx_fifo_num);
	}

	/* Free DMA Descriptors */
	if (GET_CORE_IF(pcd)->dma_desc_enable) {
		if (ep->dwc_ep.type != UE_ISOCHRONOUS) {
			desc_addr = ep->dwc_ep.desc_addr;
			dma_desc_addr = ep->dwc_ep.dma_desc_addr;

			/* Cannot call dma_free_coherent() with IRQs disabled */
			DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);
			dwc_otg_ep_free_desc_chain(desc_addr, dma_desc_addr,
						   MAX_DMA_DESC_CNT);

			goto out_unlocked;
		}
	}

	DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);

out_unlocked:
	DWC_DEBUGPL(DBG_PCD, "%d %s disabled\n", ep->dwc_ep.num,
		    ep->dwc_ep.is_in ? "IN" : "OUT");
	return 0;

}

int dwc_otg_pcd_ep_queue(dwc_otg_pcd_t *pcd, void *ep_handle,
			 uint8_t *buf, dwc_dma_t dma_buf, uint32_t buflen,
			 int zero, void *req_handle, int atomic_alloc)
{
	uint64_t flags;
	dwc_otg_pcd_request_t *req;
	dwc_otg_pcd_ep_t *ep;
	uint32_t max_transfer;

	ep = get_ep_from_handle(pcd, ep_handle);
	if (!ep || (!ep->desc && ep->dwc_ep.num != 0)) {
		DWC_WARN("bad ep\n");
		return -DWC_E_INVALID;
	}

	if (atomic_alloc)
		req = dwc_alloc_atomic(sizeof(*req));
	else
		req = dwc_alloc(sizeof(*req));

	if (!req)
		return -DWC_E_NO_MEMORY;

	DWC_CIRCLEQ_INIT_ENTRY(req, queue_entry);
	if (!GET_CORE_IF(pcd)->core_params->opt) {
		if (ep->dwc_ep.num != 0) {
			DWC_ERROR("queue req %p, len %d buf %p\n",
				  req_handle, buflen, buf);
		}
	}

	req->buf = buf;
	req->dma = dma_buf;
	req->length = buflen;
	req->sent_zlp = zero;
	req->priv = req_handle;

	DWC_SPINLOCK_IRQSAVE(pcd->lock, &flags);

	/*
	 * After adding request to the queue for IN ISOC wait for
	 * In Token Received
	 * when TX FIFO is empty interrupt and for OUT ISOC wait for OUT Token
	 * Received when EP is disabled interrupt to obtain starting microframe
	 * (odd/even) start transfer
	 */
	if (ep->dwc_ep.type == DWC_OTG_EP_TYPE_ISOC) {
		if (!DWC_CIRCLEQ_EMPTY(&ep->queue) || ep->stopped) {
			if (req != 0) {
				depctl_data_t depctl = {
					.d32 = DWC_READ_REG32(
					&pcd->core_if->dev_if->
					in_ep_regs[ep->dwc_ep.num]->
					diepctl)};
				++pcd->request_pending;

				DWC_CIRCLEQ_INSERT_TAIL(
					&ep->queue, req, queue_entry);
				if (ep->dwc_ep.is_in) {
					depctl.b.cnak = 1;
					DWC_WRITE_REG32(
						&pcd->core_if->dev_if->
						in_ep_regs[ep->dwc_ep.num]->
						diepctl, depctl.d32);
				}

				DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);
			}
			return 0;
		}
	}

	/*
	 * For EP0 IN without premature status, zlp is required?
	 */
	if (ep->dwc_ep.num == 0 && ep->dwc_ep.is_in) {
		DWC_DEBUGPL(DBG_PCDV, "%d-OUT ZLP\n", ep->dwc_ep.num);
		/*_req->zero = 1;*/
	}

	/* Start the transfer */
	if (DWC_CIRCLEQ_EMPTY(&ep->queue) && !ep->stopped) {
		/* EP0 Transfer? */
		if (ep->dwc_ep.num == 0) {
			switch (pcd->ep0state) {
			case EP0_IN_DATA_PHASE:
				DWC_DEBUGPL(DBG_PCD,
					    "%s ep0: EP0_IN_DATA_PHASE\n",
					    __func__);
				break;

			case EP0_OUT_DATA_PHASE:
				DWC_DEBUGPL(DBG_PCD,
					    "%s ep0: EP0_OUT_DATA_PHASE\n",
					    __func__);
				if (pcd->request_config) {
					/* Complete STATUS PHASE */
					ep->dwc_ep.is_in = 1;
					pcd->ep0state = EP0_IN_STATUS_PHASE;
				}
				break;

			case EP0_IN_STATUS_PHASE:
				DWC_DEBUGPL(DBG_PCD,
					    "%s ep0: EP0_IN_STATUS_PHASE\n",
					    __func__);
				break;

			default:
				DWC_DEBUGPL(DBG_ANY, "ep0: odd state %d\n",
					    pcd->ep0state);
				if (req)
					dwc_free(req);
				DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);
				return -DWC_E_SHUTDOWN;
			}

			ep->dwc_ep.dma_addr = dma_buf;
			ep->dwc_ep.start_xfer_buff = buf;
			ep->dwc_ep.xfer_buff = buf;
			ep->dwc_ep.xfer_len = buflen;
			ep->dwc_ep.xfer_count = 0;
			ep->dwc_ep.sent_zlp = 0;
			ep->dwc_ep.total_len = ep->dwc_ep.xfer_len;

			if (zero) {
				if ((ep->dwc_ep.xfer_len %
				     ep->dwc_ep.maxpacket == 0)
				    && (ep->dwc_ep.xfer_len != 0)) {
					ep->dwc_ep.sent_zlp = 1;
				}

			}

			dwc_otg_ep0_start_transfer(GET_CORE_IF(pcd),
						   &ep->dwc_ep);
		} /* non-ep0 endpoints */
		else {
#ifdef DWC_UTE_CFI
			if (ep->dwc_ep.buff_mode != BM_STANDARD) {
				/* store the request length */
				ep->dwc_ep.cfi_req_len = buflen;
				pcd->cfi->ops.build_descriptors(pcd->cfi, pcd,
								ep, req);
			} else {
#endif
				max_transfer =
				    GET_CORE_IF(ep->pcd)->core_params->
				    max_transfer_size;

				/* Setup and start the Transfer */
				ep->dwc_ep.dma_addr = dma_buf;
				ep->dwc_ep.start_xfer_buff = buf;
				ep->dwc_ep.xfer_buff = buf;
				ep->dwc_ep.xfer_len = 0;
				ep->dwc_ep.xfer_count = 0;
				ep->dwc_ep.sent_zlp = 0;
				ep->dwc_ep.total_len = buflen;

				ep->dwc_ep.maxxfer = max_transfer;
				if (GET_CORE_IF(pcd)->dma_desc_enable) {
					uint32_t out_max_xfer =
					    DDMA_MAX_TRANSFER_SIZE -
					    (DDMA_MAX_TRANSFER_SIZE % 4);
					if (ep->dwc_ep.is_in) {
						if (ep->dwc_ep.maxxfer >
						    DDMA_MAX_TRANSFER_SIZE) {
							ep->dwc_ep.maxxfer =
							    DDMA_MAX_TRANSFER_SIZE;
						}
					} else {
						if (ep->dwc_ep.maxxfer >
						    out_max_xfer) {
							ep->dwc_ep.maxxfer =
							    out_max_xfer;
						}
					}
				}
				if (ep->dwc_ep.maxxfer < ep->dwc_ep.total_len) {
					ep->dwc_ep.maxxfer -=
					    (ep->dwc_ep.maxxfer %
					     ep->dwc_ep.maxpacket);
				}

				if (zero) {
					if ((ep->dwc_ep.total_len %
					     ep->dwc_ep.maxpacket == 0)
					    && (ep->dwc_ep.total_len != 0)) {
						ep->dwc_ep.sent_zlp = 1;
					}
				}
#ifdef DWC_UTE_CFI
			}
#endif
			dwc_otg_ep_start_transfer(GET_CORE_IF(pcd),
						  &ep->dwc_ep);
		}
	}

	if (req != 0) {
		++pcd->request_pending;
		DWC_CIRCLEQ_INSERT_TAIL(&ep->queue, req, queue_entry);
		if (ep->dwc_ep.is_in && ep->stopped
		    && !(GET_CORE_IF(pcd)->dma_enable)) {
			/** @todo NGS Create a function for this. */
			diepmsk_data_t diepmsk = {.d32 = 0 };
			diepmsk.b.intktxfemp = 1;
			if (GET_CORE_IF(pcd)->multiproc_int_enable) {
				dwc_modify_reg32(&GET_CORE_IF(pcd)->dev_if->
						 dev_global_regs->
						 diepeachintmsk[ep->dwc_ep.num],
						 0, diepmsk.d32);
			} else {
				dwc_modify_reg32(&GET_CORE_IF(pcd)->dev_if->
						 dev_global_regs->diepmsk, 0,
						 diepmsk.d32);
			}

		}
	}

	DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);

	return 0;
}
int dwc_otg_pcd_ep_dequeue(dwc_otg_pcd_t *pcd, void *ep_handle,
			   void *req_handle)
{
	uint64_t flags;
	dwc_otg_pcd_request_t *req;
	dwc_otg_pcd_ep_t *ep;

	ep = get_ep_from_handle(pcd, ep_handle);
	if (!ep || (!ep->desc && ep->dwc_ep.num != 0)) {
		DWC_WARN("bad argument\n");
		return -DWC_E_INVALID;
	}

	DWC_SPINLOCK_IRQSAVE(pcd->lock, &flags);

	/* make sure it's actually queued on this endpoint */
	DWC_CIRCLEQ_FOREACH(req, &ep->queue, queue_entry) {
		if (req->priv == (void *)req_handle)
			break;
	}

	if ((req->priv != (void *)req_handle)
	    || (req == DWC_CIRCLEQ_END(&ep->queue))) {
		DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);
		return -DWC_E_INVALID;
	}

	if (!DWC_CIRCLEQ_EMPTY_ENTRY(req, queue_entry))
		dwc_otg_request_done(ep, req, -DWC_E_RESTART);
	else
		req = 0;

	DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);

	return req ? 0 : -DWC_E_SHUTDOWN;

}

int dwc_otg_pcd_ep_halt(dwc_otg_pcd_t *pcd, void *ep_handle, int value)
{
	dwc_otg_pcd_ep_t *ep;
	uint64_t flags;
	int retval = 0;

	ep = get_ep_from_handle(pcd, ep_handle);

	if (!ep || (!ep->desc && ep != &pcd->ep0) ||
	    (ep->desc && (ep->desc->bmAttributes == UE_ISOCHRONOUS))) {
		DWC_WARN("%s, bad ep\n", __func__);
		return -DWC_E_INVALID;
	}

	DWC_SPINLOCK_IRQSAVE(pcd->lock, &flags);
	if (!DWC_CIRCLEQ_EMPTY(&ep->queue)) {
		DWC_WARN("%d %s XFer In process\n", ep->dwc_ep.num,
			 ep->dwc_ep.is_in ? "IN" : "OUT");
		retval = -DWC_E_AGAIN;
	} else if (value == 0) {
		dwc_otg_ep_clear_stall(GET_CORE_IF(pcd), &ep->dwc_ep);
	} else if (value == 1) {
		if (ep->dwc_ep.is_in == 1 && GET_CORE_IF(pcd)->dma_desc_enable) {
			dtxfsts_data_t txstatus;
			fifosize_data_t txfifosize;

			txfifosize.d32 =
			    dwc_read_reg32(&GET_CORE_IF(pcd)->core_global_regs->
					   dtxfsiz[ep->dwc_ep.tx_fifo_num]);
			txstatus.d32 =
			    dwc_read_reg32(&GET_CORE_IF(pcd)->dev_if->
					   in_ep_regs[ep->dwc_ep.num]->dtxfsts);

			if (txstatus.b.txfspcavail < txfifosize.b.depth) {
				DWC_WARN("%s() Data In Tx Fifo\n", __func__);
				retval = -DWC_E_AGAIN;
			} else {
				if (ep->dwc_ep.num == 0)
					pcd->ep0state = EP0_STALL;

				ep->stopped = 1;
				dwc_otg_ep_set_stall(GET_CORE_IF(pcd),
						     &ep->dwc_ep);
			}
		} else {
			if (ep->dwc_ep.num == 0)
				pcd->ep0state = EP0_STALL;

			ep->stopped = 1;
			dwc_otg_ep_set_stall(GET_CORE_IF(pcd), &ep->dwc_ep);
		}
	} else if (value == 2)
		ep->dwc_ep.stall_clear_flag = 0;
	else if (value == 3)
		ep->dwc_ep.stall_clear_flag = 1;

	DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);

	return retval;
}

/**
 * This function initiates remote wakeup of the host from suspend state.
 */
void dwc_otg_pcd_rem_wkup_from_suspend(dwc_otg_pcd_t *pcd, int set)
{
	dctl_data_t dctl = { 0 };
	dwc_otg_core_if_t *core_if = GET_CORE_IF(pcd);
	dsts_data_t dsts;

	dsts.d32 = dwc_read_reg32(&core_if->dev_if->dev_global_regs->dsts);
	if (!dsts.b.suspsts)
		DWC_WARN("Remote wakeup while is not in suspend state\n");

	/* Check if DEVICE_REMOTE_WAKEUP feature enabled */
	if (pcd->remote_wakeup_enable) {
		if (set) {

			if (core_if->adp_enable) {
				gpwrdn_data_t gpwrdn;

				dwc_otg_adp_probe_stop(core_if);

				/* Mask SRP detected interrupt from Power Down Logic */
				gpwrdn.d32 = 0;
				gpwrdn.b.srp_det_msk = 1;
				dwc_modify_reg32(&core_if->core_global_regs->
						 gpwrdn, gpwrdn.d32, 0);

				/* Disable Power Down Logic */
				gpwrdn.d32 = 0;
				gpwrdn.b.pmuactv = 1;
				dwc_modify_reg32(&core_if->core_global_regs->
						 gpwrdn, gpwrdn.d32, 0);

				/*
				 * Initialize the Core for Device mode.
				 */
				core_if->op_state = B_PERIPHERAL;
				dwc_otg_core_init(core_if);
				dwc_otg_enable_global_interrupts(core_if);
				cil_pcd_start(core_if);

				dwc_otg_initiate_srp(core_if);
			}

			dctl.b.rmtwkupsig = 1;
			dwc_modify_reg32(&core_if->dev_if->dev_global_regs->
					 dctl, 0, dctl.d32);
			DWC_DEBUGPL(DBG_PCD, "Set Remote Wakeup\n");

			dwc_mdelay(2);
			dwc_modify_reg32(&core_if->dev_if->dev_global_regs->
					 dctl, dctl.d32, 0);
			DWC_DEBUGPL(DBG_PCD, "Clear Remote Wakeup\n");
		}
	} else {
		DWC_DEBUGPL(DBG_PCD, "Remote Wakeup is disabled\n");
	}
}

#ifdef CONFIG_USB_DWC_OTG_LPM
/**
 * This function initiates remote wakeup of the host from L1 sleep state.
 */
void dwc_otg_pcd_rem_wkup_from_sleep(dwc_otg_pcd_t *pcd, int set)
{
	glpmcfg_data_t lpmcfg;
	dwc_otg_core_if_t *core_if = GET_CORE_IF(pcd);

	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);

	/* Check if we are in L1 state */
	if (!lpmcfg.b.prt_sleep_sts) {
		DWC_DEBUGPL(DBG_PCD, "Device is not in sleep state\n");
		return;
	}

	/* Check if host allows remote wakeup */
	if (!lpmcfg.b.rem_wkup_en) {
		DWC_DEBUGPL(DBG_PCD, "Host does not allow remote wakeup\n");
		return;
	}

	/* Check if Resume OK */
	if (!lpmcfg.b.sleep_state_resumeok) {
		DWC_DEBUGPL(DBG_PCD, "Sleep state resume is not OK\n");
		return;
	}

	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);
	lpmcfg.b.en_utmi_sleep = 0;
	lpmcfg.b.hird_thres &= (~(1 << 4));
	dwc_write_reg32(&core_if->core_global_regs->glpmcfg, lpmcfg.d32);

	if (set) {
		dctl_data_t dctl = {.d32 = 0 };
		dctl.b.rmtwkupsig = 1;
		/* Set RmtWkUpSig bit to start remote wakup signaling.
		 * Hardware will automatically clear this bit.
		 */
		dwc_modify_reg32(&core_if->dev_if->dev_global_regs->dctl,
				 0, dctl.d32);
		DWC_DEBUGPL(DBG_PCD, "Set Remote Wakeup\n");
	}

}
#endif

/**
 * Performs remote wakeup.
 */
void dwc_otg_pcd_remote_wakeup(dwc_otg_pcd_t *pcd, int set)
{
	dwc_otg_core_if_t *core_if = GET_CORE_IF(pcd);
	if (dwc_otg_is_device_mode(core_if)) {
#ifdef CONFIG_USB_DWC_OTG_LPM
		if (core_if->lx_state == DWC_OTG_L1) {
			dwc_otg_pcd_rem_wkup_from_sleep(pcd, set);
		} else {
#endif
			dwc_otg_pcd_rem_wkup_from_suspend(pcd, set);
#ifdef CONFIG_USB_DWC_OTG_LPM
		}
#endif
	}
	return;
}

void dwc_otg_pcd_disconnect(dwc_otg_pcd_t *pcd, int enable)
{
	return dwc_otg_core_soft_disconnect(GET_CORE_IF(pcd), enable);
}

#ifdef CONFIG_USB_PCD_SETTINGS
void dwc_otg_pcd_start_clean(dwc_otg_pcd_t *pcd, int start)
{
	if (start)
		cil_pcd_start(GET_CORE_IF(pcd));
	else {
		dwc_otg_start_stop_phy_clk(GET_CORE_IF(pcd), true);
		cil_pcd_clean(GET_CORE_IF(pcd));
		dwc_otg_start_stop_phy_clk(GET_CORE_IF(pcd), false);
	}
	return;
}
#endif

void dwc_otg_pcd_disconnect_us(dwc_otg_pcd_t *pcd, int no_of_usecs)
{
	dwc_otg_core_soft_disconnect(GET_CORE_IF(pcd), true);
	dwc_udelay(no_of_usecs);
	dwc_otg_core_soft_disconnect(GET_CORE_IF(pcd), false);
	return;
}

int dwc_otg_pcd_wakeup(dwc_otg_pcd_t *pcd)
{
	dsts_data_t dsts;
	gotgctl_data_t gotgctl;
	uint64_t flags;

	DWC_SPINLOCK_IRQSAVE(pcd->lock, &flags);

	/*
	 * This function starts the Protocol if no session is in progress. If
	 * a session is already in progress, but the device is suspended,
	 * remote wakeup signaling is started.
	 */

	/* Check if valid session */
	gotgctl.d32 =
	    dwc_read_reg32(&(GET_CORE_IF(pcd)->core_global_regs->gotgctl));
	if (gotgctl.b.bsesvld) {
		/* Check if suspend state */
		dsts.d32 =
		    dwc_read_reg32(&
				   (GET_CORE_IF(pcd)->dev_if->dev_global_regs->
				    dsts));
		if (dsts.b.suspsts)
			dwc_otg_pcd_remote_wakeup(pcd, 1);
	} else {
		dwc_otg_pcd_initiate_srp(pcd);
	}

	DWC_SPINUNLOCK_IRQRESTORE(pcd->lock, flags);
	return 0;

}

/**
 * Start the SRP timer to detect when the SRP does not complete within
 * 6 seconds.
 *
 * @param pcd the pcd structure.
 */
void dwc_otg_pcd_initiate_srp(dwc_otg_pcd_t *pcd)
{
	dwc_otg_initiate_srp(GET_CORE_IF(pcd));
}

int dwc_otg_pcd_get_frame_number(dwc_otg_pcd_t *pcd)
{
	return dwc_otg_get_frame_number(GET_CORE_IF(pcd));
}

int dwc_otg_pcd_is_lpm_enabled(dwc_otg_pcd_t *pcd)
{
	return GET_CORE_IF(pcd)->core_params->lpm_enable;
}

uint32_t get_b_hnp_enable(dwc_otg_pcd_t *pcd)
{
	return pcd->b_hnp_enable;
}

uint32_t get_a_hnp_support(dwc_otg_pcd_t *pcd)
{
	return pcd->a_hnp_support;
}

uint32_t get_a_alt_hnp_support(dwc_otg_pcd_t *pcd)
{
	return pcd->a_alt_hnp_support;
}

int dwc_otg_pcd_get_rmwkup_enable(dwc_otg_pcd_t *pcd)
{
	return pcd->remote_wakeup_enable;
}

#endif /* DWC_HOST_ONLY */
