/*
 * drivers/usb/gadget/s3c-udc-otg-hs.c
 * Samsung S3C on-chip full/high speed USB OTG 2.0 device controllers
 *
 * Copyright (C) 2008 Samsung Electronics, Kyu-Hyeok Jang, Seung-Soo Yang
 * Copyright (C) 2009 Samsung Electronics, Seung-Soo Yang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 *	The changes of udc_state of struct s3c_udc in functions
 *	
 *	  probe					s3c_udc->udc_state = USB_STATE_NOTATTACHED;
 *	  udc_enable			s3c_udc->udc_state = USB_STATE_POWERED;
 *	  reset irq				s3c_udc->udc_state = USB_STATE_DEFAULT;
 *	  ude_set_address		s3c_udc->udc_state = USB_STATE_ADDRESS;
 *	  ep0_setup				s3c_udc->udc_state = USB_STATE_CONFIGURED;
 *	  process_suspend_intr	s3c_udc->udc_state = USB_STATE_SUSPENDED;
 *	  udc_disable			s3c_udc->udc_state = USB_STATE_NOTATTACHED;
 *
 */
  
#include "s3c-udc.h"
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <mach/map.h>
#include <plat/regs-usb-otg-hs.h>
#include <plat/regs-clock.h> 

#include <plat/gpio-cfg.h>
#include <plat/regs-gpio.h>
#include <mach/hardware.h>
#include <linux/i2c.h>

#include <plat/regs-otg.h>
#include <linux/usb/ch9.h>

#if defined (CONFIG_MACH_SMDK6410)

extern void s3c_cable_check_status(int flag);

void s3c_udc_cable_connect(struct s3c_udc *dev)
{
	if(dev->is_pm_lock != false)
		return;

	printk("[%s] \n",__FUNCTION__);

	s3c_cable_check_status(1);

	dev->is_pm_lock = true;
}

void s3c_udc_cable_disconnect(struct s3c_udc *dev)
{
	if(dev->is_pm_lock != true)
		return;

	printk("[%s] \n",__FUNCTION__);
	
	s3c_cable_check_status(0);
	
	dev->is_pm_lock = false;
}
#endif /* CONFIG_MACH_SMDK6410 */


#define OTG_DBG_ENABLE	0

#if OTG_DBG_ENABLE
#define DEBUG_S3C_UDC_SETUP
#define DEBUG_S3C_UDC_EP0
#define DEBUG_S3C_UDC_ISR
#define DEBUG_S3C_UDC_OUT_EP
#define DEBUG_S3C_UDC_IN_EP
#define DEBUG_S3C_UDC
#define DEBUG_S3C_UDC_PM

#else
#undef DEBUG_S3C_UDC_SETUP
#undef DEBUG_S3C_UDC_EP0
#undef DEBUG_S3C_UDC_ISR
#undef DEBUG_S3C_UDC_OUT_EP
#undef DEBUG_S3C_UDC_IN_EP
#undef DEBUG_S3C_UDC
#undef DEBUG_S3C_UDC_PM
#endif

//temp
//#define DEBUG_S3C_UDC_SETUP
#define DEBUG_S3C_UDC_PM

#if defined(DEBUG_S3C_UDC_ISR) || defined(DEBUG_S3C_UDC_EP0)
static char *state_names[] = {
	"WAIT_FOR_SETUP",
	"DATA_STATE_XMIT",
	"DATA_STATE_NEED_ZLP",
	"WAIT_FOR_OUT_STATUS",
	"DATA_STATE_RECV",
	"RegReadErr"
	};
#endif

#ifdef DEBUG_S3C_UDC_SETUP
#define DEBUG_SETUP(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG_SETUP(fmt,args...) do {} while(0)
#endif

#ifdef DEBUG_S3C_UDC_PM
#define DEBUG_PM(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG_PM(fmt,args...) do {} while(0)
#endif

#ifdef DEBUG_S3C_UDC_EP0
#define DEBUG_EP0(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG_EP0(fmt,args...) do {} while(0)
#endif

#ifdef DEBUG_S3C_UDC
#define DEBUG(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG(fmt,args...) do {} while(0)
#endif

#ifdef DEBUG_S3C_UDC_ISR
#define DEBUG_ISR(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG_ISR(fmt,args...) do {} while(0)
#endif

#ifdef DEBUG_S3C_UDC_OUT_EP
#define DEBUG_OUT_EP(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG_OUT_EP(fmt,args...) do {} while(0)
#endif

#ifdef DEBUG_S3C_UDC_IN_EP
#define DEBUG_IN_EP(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG_IN_EP(fmt,args...) do {} while(0)
#endif
//---------------------------------------------------------------------------------------

#define	DRIVER_DESC		"Samsung Dual-speed USB 2.0 OTG Device Controller"
#define	DRIVER_VERSION		__DATE__

static	struct s3c_udc	*the_controller;
static struct clk	*otg_clock = NULL;

static const char driver_name[] = "s3c-udc";
static const char driver_desc[] = DRIVER_DESC;
static const char ep0name[] = "ep0-control";
//---------------------------------------------------------------------------------------

/*
 *	Samsung S3C on-chip full/high speed USB OTG 2.0 device operates
 *	both internal DMA mode and slave mode.
 *	The implementation of the slave mode is not stable at the time of writing(Feb. 18 2009)
 *  Make sure CONFIG_USB_GADGET_S3C_OTGD_HS_DMA_MODE is defineds
 */

#if defined(CONFIG_USB_GADGET_S3C_OTGD_HS_DMA_MODE) 
#define GINTMSK_INIT	(INT_OUT_EP|INT_IN_EP|INT_RESUME|INT_ENUMDONE|INT_RESET\
						|INT_SUSPEND|INT_DISCONN)
#define DOEPMSK_INIT	(CTRL_OUT_EP_SETUP_PHASE_DONE|AHB_ERROR|TRANSFER_DONE)
#define DIEPMSK_INIT	(NON_ISO_IN_EP_TIMEOUT|AHB_ERROR|TRANSFER_DONE)
#define GAHBCFG_INIT	(PTXFE_HALF|NPTXFE_HALF|MODE_DMA|BURST_SINGLE|GBL_INT_UNMASK)
#else
#define GINTMSK_INIT	(INT_RESUME|INT_ENUMDONE|INT_RESET|INT_SUSPEND|	INT_RX_FIFO_NOT_EMPTY\
						|INT_GOUTNakEff|INT_GINNakEff|INT_DISCONN)//|INT_IN_EP)
#define DOEPMSK_INIT	(CTRL_OUT_EP_SETUP_PHASE_DONE|AHB_ERROR|TRANSFER_DONE)
#define DIEPMSK_INIT	(INTKN_TXFEMP|NON_ISO_IN_EP_TIMEOUT|TRANSFER_DONE|AHB_ERROR)
#define GAHBCFG_INIT	(PTXFE_HALF|NPTXFE_HALF|MODE_SLAVE|BURST_INCR16|GBL_INT_UNMASK)

#endif
//---------------------------------------------------------------------------------------

/*
 * Local declarations.
*/
static int s3c_ep_enable(struct usb_ep *ep, const struct usb_endpoint_descriptor *);
static int s3c_ep_disable(struct usb_ep *ep);
static struct usb_request *s3c_alloc_request(struct usb_ep *ep, gfp_t gfp_flags);
static void s3c_free_request(struct usb_ep *ep, struct usb_request *);
static int s3c_queue(struct usb_ep *ep, struct usb_request *, gfp_t gfp_flags);
static int s3c_dequeue(struct usb_ep *ep, struct usb_request *);
static int s3c_set_halt(struct usb_ep *ep, int);
static int s3c_fifo_status(struct usb_ep *ep);
static void s3c_fifo_flush(struct usb_ep *ep);
static void s3c_ep0_read(struct s3c_udc *dev);
static void s3c_handle_ep0(struct s3c_udc *dev);
static int s3c_ep0_write(struct s3c_udc *dev);
static int write_fifo_ep0(struct s3c_ep *ep, struct s3c_request *req);
static void done(struct s3c_ep *ep, struct s3c_request *req, int status);
static void stop_activity(struct s3c_udc *dev, struct usb_gadget_driver *driver);
static int udc_enable(struct s3c_udc *dev);
static void udc_set_address(struct s3c_udc *dev, unsigned char address);
static void reconfig_usbd(struct s3c_udc *dev);

static inline void s3c_ep_send_zlp(u32 ep_num);
//---------------------------------------------------------------------------------------

/**
 * Max packet size
 * 
 */
static u32 ep0_fifo_size = 64;
static u32 ep_fifo_size =  512;
static u32 ep_fifo_size2 = 1024;
//---------------------------------------------------------------------------------------

struct usb_ctrlrequest ctrl;
//---------------------------------------------------------------------------------------

/**
 * usb_ep_ops s3c_ep_ops
 * 
 */
static struct usb_ep_ops s3c_ep_ops = {
	.enable = s3c_ep_enable,
	.disable = s3c_ep_disable,

	.alloc_request = s3c_alloc_request,
	.free_request = s3c_free_request,

	.queue = s3c_queue,
	.dequeue = s3c_dequeue,

	.set_halt = s3c_set_halt,
	.fifo_status = s3c_fifo_status,
	.fifo_flush = s3c_fifo_flush,
};
//---------------------------------------------------------------------------------------

/**
 * Proc related
 * 
 */
#ifdef CONFIG_USB_GADGET_DEBUG_FILES

static const char proc_node_name[] = "driver/udc";

static int
udc_proc_read(char *page, char **start, off_t off, int count, int *eof, void *_dev)
{
	char *buf = page;
	struct s3c_udc *dev = _dev;
	char *next = buf;
	unsigned size = count;
	unsigned long flags;
	int t;

	if (off != 0)
		return 0;

	local_irq_save(flags);

	/* basic device status */
	t = scnprintf(next, size,
		      DRIVER_DESC "\n"
		      "%s version: %s\n"
		      "Gadget driver: %s\n"
		      "\n",
		      driver_name, DRIVER_VERSION,
		      dev->driver ? dev->driver->driver.name : "(none)");
	size -= t;
	next += t;

	local_irq_restore(flags);
	*eof = 1;
	return count - size;
}

#define create_proc_files() \
	create_proc_read_entry(proc_node_name, 0, NULL, udc_proc_read, dev)
#define remove_proc_files() \
	remove_proc_entry(proc_node_name, NULL)

#else	/* !CONFIG_USB_GADGET_DEBUG_FILES */

#define create_proc_files() do {} while (0)
#define remove_proc_files() do {} while (0)

#endif	/* CONFIG_USB_GADGET_DEBUG_FILES */
//---------------------------------------------------------------------------------------

/* 
 * until it becomes enabled, this UDC should be completely invisible
 * to any USB host.
 */
static int udc_enable(struct s3c_udc *dev)
{
	u32 uTemp;
	DEBUG_SETUP("%s: %p\n", __FUNCTION__, dev);

	uTemp = readl(S3C_OTHERS);
	uTemp |= (1<<16);   // USB_SIG_MASK
	writel(uTemp, S3C_OTHERS);
	
	// 1. Initializes OTG Phy.
	writel(0x0, S3C_USBOTG_PHYPWR);
//	writel(0x20, S3C_USBOTG_PHYCLK);
	writel(0x02, S3C_USBOTG_PHYCLK); /* 2010-0208, modified by CVKK(JC) */
	writel(0x1, S3C_USBOTG_RSTCON);
	udelay(50);
	writel(0x0, S3C_USBOTG_RSTCON);
	udelay(50);
	
	dev->udc_state = USB_STATE_POWERED;
	dev->gadget.speed = USB_SPEED_UNKNOWN;

	reconfig_usbd(dev);

	DEBUG_SETUP("S3C USB 2.0 OTG Controller Core Initialized : 0x%x\n",
			readl(S3C_UDC_OTG_GINTMSK));
	
	return 0;
}
//---------------------------------------------------------------------------------------

extern void otg_phy_init(u32);
/*
 * 	udc_disable - disable USB device controller
 */
static void udc_disable(struct s3c_udc *dev)
{
	DEBUG_SETUP("%s: %p\n", __FUNCTION__, dev);

	udc_set_address(dev, 0);

	dev->ep0state = WAIT_FOR_SETUP;
	dev->gadget.speed = USB_SPEED_UNKNOWN;
	dev->usb_address = 0;
	//writel(readl(S3C_USBOTG_PHYPWR)|(0x7<<1), S3C_USBOTG_PHYPWR);
        otg_phy_init(0x42); /* 2010-0208, added by CVKK(JC) */
	dev->udc_state = USB_STATE_NOTATTACHED;
}
//---------------------------------------------------------------------------------------

/*
 * 	udc_ep_list_reinit - initialize software state
 */
static void udc_ep_list_reinit(struct s3c_udc *dev)
{
	u32 i;

	DEBUG_SETUP("%s: %p\n", __FUNCTION__, dev);

	/* device/ep0 records init */
	INIT_LIST_HEAD(&dev->gadget.ep_list);
	INIT_LIST_HEAD(&dev->gadget.ep0->ep_list);
	dev->ep0state = WAIT_FOR_SETUP;

	/* basic endpoint records init */
	for (i = 0; i < S3C_MAX_ENDPOINTS; i++) {
		struct s3c_ep *ep = &dev->ep[i];

		if (i != 0)
			list_add_tail(&ep->ep.ep_list, &dev->gadget.ep_list);

		ep->desc = 0;
		ep->stopped = 0;
		INIT_LIST_HEAD(&ep->queue);
	}

	/* the rest was statically initialized, and is read-only */
}
//---------------------------------------------------------------------------------------


/*
 * usb_gadget_register_driver
 * Register entry point for the peripheral controller driver.
*/
int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	struct s3c_udc *dev = the_controller;
	int retval;

	DEBUG_SETUP("%s: %s\n", __FUNCTION__, driver->driver.name);
#if 1
/*
	adb composite fail to !driver->unbind in composite.c as below
	static struct usb_gadget_driver composite_driver = {
		.speed		= USB_SPEED_HIGH,

		.bind		= composite_bind,
		.unbind		= __exit_p(composite_unbind),
*/
	if (!driver
	    || (driver->speed != USB_SPEED_FULL && driver->speed != USB_SPEED_HIGH)
	    || !driver->bind
	    || !driver->disconnect || !driver->setup)
		return -EINVAL;		
#else	
if (!driver
	|| (driver->speed != USB_SPEED_FULL && driver->speed != USB_SPEED_HIGH)
	|| !driver->bind
	|| !driver->unbind || !driver->disconnect || !driver->setup)
	return -EINVAL;
#endif
	
	if (!dev)
		return -ENODEV;

	if (dev->driver)
		return -EBUSY;

	/* first hook up the driver ... */
	dev->driver = driver;
	dev->gadget.dev.driver = &driver->driver;
	retval = device_add(&dev->gadget.dev);

	if(retval) { /* TODO */
		printk("target device_add failed, error %d\n", retval);
		return retval;
	}
	
	retval = driver->bind(&dev->gadget);
	if (retval) {
		printk("%s: bind to driver %s --> error %d\n", dev->gadget.name,
		       driver->driver.name, retval);
		device_del(&dev->gadget.dev);

		dev->driver = 0;
		dev->gadget.dev.driver = 0;
		return retval;
	}
	enable_irq(IRQ_OTG);
	
	printk("Registered gadget driver '%s'\n", driver->driver.name);
	udc_enable(dev);

	//in case of rndis, will be chaned at the time of SET_CONFIGURATION
	if (strcmp(driver->driver.name, "g_ether") == 0)
		dev->config_gadget_driver = ETHER_CDC;	
	
	else if (strcmp(driver->driver.name, "android_adb") == 0)
		dev->config_gadget_driver = ANDROID_ADB;
	
	else if (strcmp(driver->driver.name, "android_usb") == 0)
		dev->config_gadget_driver = ANDROID_ADB_UMS;
	
	else if (strcmp(driver->driver.name, "android_adb_ums_acm") == 0)
		dev->config_gadget_driver = ANDROID_ADB_UMS_ACM;
	
	else if (strcmp(driver->driver.name, "g_serial") == 0)
		dev->config_gadget_driver = SERIAL;
	
	else if (strcmp(driver->driver.name, "g_cdc") == 0)
		dev->config_gadget_driver = CDC2;
	
	else if (strcmp(driver->driver.name, "g_file_storage") == 0)
		dev->config_gadget_driver = FILE_STORAGE;

	else
		printk("Not recognized driver's name '%s'\n", driver->driver.name);
		
	return 0;
}

EXPORT_SYMBOL(usb_gadget_register_driver);
//---------------------------------------------------------------------------------------

/*
 * Unregister entry point for the peripheral controller driver.
*/
int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct s3c_udc *dev = the_controller;
	unsigned long flags;

	if (!dev)
		return -ENODEV;
	if (!driver || driver != dev->driver)
		return -EINVAL;
	
	disable_irq(IRQ_OTG);

	spin_lock_irqsave(&dev->lock, flags);
	stop_activity(dev, driver);
	spin_unlock_irqrestore(&dev->lock, flags);

	driver->unbind(&dev->gadget);
	device_del(&dev->gadget.dev);

	printk("Unregistered gadget driver '%s'\n", driver->driver.name);

	udc_disable(dev);

	dev->gadget.dev.driver = NULL;
	dev->driver = NULL;
	dev->config_gadget_driver = NO_GADGET_DRIVER;

	return 0;
}

EXPORT_SYMBOL(usb_gadget_unregister_driver);
//---------------------------------------------------------------------------------------

#if defined(CONFIG_USB_GADGET_S3C_OTGD_HS_DMA_MODE) 
	/* DMA Mode */
	#include "s3c-udc-otg-hs_dma.c"
#else 
	/* Slave Mode */
	#include "s3c-udc-otg-hs_slave.c"
#endif

/*
 * power up s3c-udc power & enable otg clock
 */ 
void s3c_udc_power_up(void)
{
	struct s3c_udc *dev = the_controller;

	printk("[%s] \n", __FUNCTION__);
	
	//fsa9480_s3c_udc_on();

	clk_enable(otg_clock);
	udc_enable(dev);
}
//---------------------------------------------------------------------------------------

/*
 * power down s3c-udc power & disable otg clock
 */ 
void s3c_udc_power_down(void)
{
	struct s3c_udc *dev = the_controller;
	printk("[%s] \n", __FUNCTION__);

	udc_disable(dev);
	clk_disable(otg_clock);

	//fsa9480_s3c_udc_off();
}
//---------------------------------------------------------------------------------------


/*
 * make s3c-udc being connected
 */ 
void s3c_udc_soft_connect(void)
{
	u32 uTemp;
	
	DEBUG("[%s]\n", __FUNCTION__);
	uTemp = readl(S3C_UDC_OTG_DCTL);
	uTemp = uTemp & ~SOFT_DISCONNECT;
	writel(uTemp, S3C_UDC_OTG_DCTL);
}
//---------------------------------------------------------------------------------------

/*
 * make s3c-udc being disconnected
 */ 
void s3c_udc_soft_disconnect(void)
{
	u32 uTemp;
	struct s3c_udc *dev = the_controller;

	DEBUG("[%s]\n", __FUNCTION__);	
	uTemp = readl(S3C_UDC_OTG_DCTL);
	uTemp |= SOFT_DISCONNECT;
	writel(uTemp, S3C_UDC_OTG_DCTL);

	set_disconnect_state(dev);
}
//---------------------------------------------------------------------------------------

/*
 * get frame number 
 */ 
static int s3c_udc_get_frame(struct usb_gadget *_gadget)
{
	/*fram count number [21:8]*/
	u32 frame = readl(S3C_UDC_OTG_DSTS);

	printk("[%s]: %s\n", __FUNCTION__, _gadget->name);
	return (frame & 0x3ff00);
}
//---------------------------------------------------------------------------------------

/*
 * usb_gadget_ops wake-up not implemented
 */ 
static int s3c_udc_wakeup(struct usb_gadget *_gadget)
{
	printk("[%s]: %s\n", __FUNCTION__, _gadget->name);
	return -ENOTSUPP;
}
//---------------------------------------------------------------------------------------

/*
 * usb_gadget_ops pullup
 */ 
static int s3c_udc_pullup(struct usb_gadget *gadget, int is_on)
{
	if (is_on)
		s3c_udc_soft_connect();
	else
		s3c_udc_soft_disconnect();
	return 0;
}
//---------------------------------------------------------------------------------------

/**
 * struct usb_gadget_ops s3c_udc_ops
 * 
 */
static const struct usb_gadget_ops s3c_udc_ops = {
	.get_frame = s3c_udc_get_frame,
	.wakeup = s3c_udc_wakeup,
	/* current versions must always be self-powered */
	//added by ss1	
	.pullup	= s3c_udc_pullup,
};
//---------------------------------------------------------------------------------------

/*
 * dev release
 */ 
static void nop_release(struct device *dev)
{
	DEBUG("%s %s\n", __FUNCTION__, dev->bus_id);
}
//---------------------------------------------------------------------------------------

/**
 * struct s3c_udc memory
 * 
 */
static struct s3c_udc memory = {
	.usb_address = 0,

	.gadget = {
		   .ops = &s3c_udc_ops,
		   .ep0 = &memory.ep[0].ep,
		   .name = driver_name,
		   .dev = {
			   .bus_id = "gadget",
			   .release = nop_release,
			   },
		   },

	/* control endpoint */
	.ep[0] = {
		  .ep = {
			 .name = ep0name,
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP0_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = 0,
		  .bmAttributes = 0,

		  .fifo = (u32) S3C_UDC_OTG_EP0_FIFO,
		  },

	/* first group of endpoints */
	.ep[1] = {
		  .ep = {
			 .name = "ep1-bulk",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = 1,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,

		  .fifo = (u32) S3C_UDC_OTG_EP1_FIFO,
		  },

	.ep[2] = {
		  .ep = {
			 .name = "ep2-bulk",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 2,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,

		  .fifo = (u32) S3C_UDC_OTG_EP2_FIFO,
		  },

	.ep[3] = {				// Though NOT USED XXX
		  .ep = {
			 .name = "ep3-int",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 3,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .fifo = (u32) S3C_UDC_OTG_EP3_FIFO,
		  },
		  
	.ep[4] = {
		  .ep = {
			 .name = "ep4-bulk",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = 4,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,

		  .fifo = (u32) S3C_UDC_OTG_EP4_FIFO,
		  },

	.ep[5] = {
		  .ep = {
			 .name = "ep5-bulk",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 5,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,

		  .fifo = (u32) S3C_UDC_OTG_EP5_FIFO,
		  },

	.ep[6] = {				 
		  .ep = {
			 .name = "ep6-int",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 6,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .fifo = (u32) S3C_UDC_OTG_EP6_FIFO,
		  },
		  
	.ep[7] = {				 
		  .ep = {
			 .name = "ep7-bulk",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = 7,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,

		  .fifo = (u32) S3C_UDC_OTG_EP7_FIFO,
		  },
		  
	.ep[8] = {				 
		  .ep = {
			 .name = "ep8-bulk",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 8,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,

		  .fifo = (u32) S3C_UDC_OTG_EP8_FIFO,
		  },
		  
	.ep[9] = {				 
		  .ep = {
			 .name = "ep9-int",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 8,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .fifo = (u32) S3C_UDC_OTG_EP8_FIFO,
		  },
//following eps are not used		  
	.ep[10] = {				// Though NOT USED XXX
		  .ep = {
			 .name = "ep10-int",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE2,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 8,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .fifo = (u32) S3C_UDC_OTG_EP8_FIFO,
		  },
	.ep[11] = {				// Though NOT USED XXX
		  .ep = {
			 .name = "ep11-int",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE2,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 8,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .fifo = (u32) S3C_UDC_OTG_EP8_FIFO,
		  },
	.ep[12] = {				// Though NOT USED XXX
		  .ep = {
			 .name = "ep12-int",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE2,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 8,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .fifo = (u32) S3C_UDC_OTG_EP8_FIFO,
		  },
	.ep[13] = {				// Though NOT USED XXX
		  .ep = {
			 .name = "ep13-int",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE2,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 8,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .fifo = (u32) S3C_UDC_OTG_EP8_FIFO,
		  },
	.ep[14] = {				// Though NOT USED XXX
		  .ep = {
			 .name = "ep14-int",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE2,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 8,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .fifo = (u32) S3C_UDC_OTG_EP8_FIFO,
		  },
	.ep[15] = {				// Though NOT USED XXX
		  .ep = {
			 .name = "ep15-int",
			 .ops = &s3c_ep_ops,
			 .maxpacket = EP_FIFO_SIZE2,
			 },
		  .dev = &memory,

		  .bEndpointAddress = USB_DIR_IN | 8,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,

		  .fifo = (u32) S3C_UDC_OTG_EP8_FIFO,
		  },
};
//---------------------------------------------------------------------------------------

/*
 * 	probe - binds to the platform device
 */
static int s3c_udc_probe(struct platform_device *pdev)
{
	struct s3c_udc *dev = &memory;
	int retval;

	DEBUG("%s: %p\n", __FUNCTION__, pdev);

	spin_lock_init(&dev->lock);
	dev->dev = pdev;

	device_initialize(&dev->gadget.dev);
	dev->gadget.dev.parent = &pdev->dev;

	dev->gadget.is_dualspeed = 1;	// Hack only
	dev->gadget.is_otg = 0;
	dev->gadget.is_a_peripheral = 0;
	dev->gadget.b_hnp_enable = 0;
	dev->gadget.a_hnp_support = 0;
	dev->gadget.a_alt_hnp_support = 0;
	dev->udc_state = USB_STATE_NOTATTACHED;
	dev->config_gadget_driver = NO_GADGET_DRIVER;
	
#if defined (CONFIG_MACH_SMDK6410)
	dev->is_pm_lock = false;
#endif /* CONFIG_MACH_SMDK6410 */

	the_controller = dev;
	platform_set_drvdata(pdev, dev);

	otg_clock = clk_get(&pdev->dev, "otg");

	if (IS_ERR(otg_clock)) {
		printk(KERN_INFO "failed to find otg clock source\n");
		return -ENOENT;
	}
	clk_enable(otg_clock);

	udc_ep_list_reinit(dev);

	local_irq_disable();

	/* irq setup after old hardware state is cleaned up */
	retval =
	    request_irq(IRQ_OTG, s3c_udc_irq, 0, driver_name, dev);

	if (retval != 0) {
		DEBUG(KERN_ERR "%s: can't get irq %i, err %d\n", driver_name,
		      IRQ_OTG, retval);
		return -EBUSY;
	}

	disable_irq(IRQ_OTG);
	local_irq_enable();
	create_proc_files();

	//it just prints which file included regarding whether DMA mode or SLAVE mode
	s3c_show_mode();
	
	return retval;
}
//---------------------------------------------------------------------------------------

/*
 * 	remove - unbinds to the platform device
 */
static int s3c_udc_remove(struct platform_device *pdev)
{
	struct s3c_udc *dev = platform_get_drvdata(pdev);

	DEBUG("%s: %p\n", __FUNCTION__, pdev);

	if (otg_clock != NULL) {
		clk_disable(otg_clock);
		clk_put(otg_clock);
		otg_clock = NULL;
	}

	remove_proc_files();
	usb_gadget_unregister_driver(dev->driver);

	free_irq(IRQ_OTG, dev);

	platform_set_drvdata(pdev, 0);

	the_controller = 0;

	return 0;
}
//---------------------------------------------------------------------------------------

/*
 * 	AHB clock gating for suspend
 */
int s3c_udc_suspend_clock_gating(void)
{
	u32	uReg;
	DEBUG_PM("[%s]\n", __FUNCTION__);
	uReg = readl(S3C_UDC_OTG_PCGCCTL);

	writel(uReg|1<<STOP_PCLK_BIT|1<<GATE_HCLK_BIT, S3C_UDC_OTG_PCGCCTL); 
	DEBUG_PM("[%s] : S3C_UDC_OTG_PCGCCTL 0x%x \n", __FUNCTION__, uReg);	
	return 0;
}
//---------------------------------------------------------------------------------------

/*
 * 	AHB clock gating for resume
 */
int s3c_udc_resume_clock_gating(void)
{
	u32	uReg;
	DEBUG_PM("[%s]\n", __FUNCTION__);
	
	uReg = readl(S3C_UDC_OTG_PCGCCTL);
	uReg &= ~(1<<STOP_PCLK_BIT|1<<GATE_HCLK_BIT);
	writel(uReg, S3C_UDC_OTG_PCGCCTL); 
	DEBUG_PM("[%s] : S3C_UDC_OTG_PCGCCTL 0x%x \n", __FUNCTION__, uReg);	
	return 0;
}
//---------------------------------------------------------------------------------------

#ifdef CONFIG_PM

/*
 *	Synopsys OTG PM supports Partial Power-Down and AHB Clock Gating.
 *	OTG PM just turns on or off OTG PHYPWR 
 *	because S3C6410 can only use the clock gating method.		 
*/
enum OTG_PM
{
	ALL_POWER_DOWN,    		//turn off OTG module

	//the followings are not tested yet
	CLOCK_GATING,			//using AHB clock gating,
	OPHYPWR_FORCE_SUSPEND,	//using OPHYPWR.force_suspend
}; 
//---------------------------------------------------------------------------------------

//specifying pm_policy
static enum	OTG_PM pm_policy = ALL_POWER_DOWN;

static int s3c_udc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct s3c_udc *dev = platform_get_drvdata(pdev);

	DEBUG_PM("[%s]: System Suspend \n", __FUNCTION__);

	//save the state of connection to udc_resume_state
	if (dev->gadget.speed != USB_SPEED_UNKNOWN)
	{
		dev->udc_resume_state = dev->udc_state;
		DEBUG_PM("[%s]: USB connected at suspend\n", __FUNCTION__);
	}
	else
	{
		dev->udc_resume_state = dev->udc_state;
		DEBUG_PM("[%s]: USB not connected at suspend\n", __FUNCTION__);
	}

	switch(pm_policy)
	{
		case ALL_POWER_DOWN:			
			s3c_udc_soft_disconnect();
			stop_activity(dev, dev->driver);
			//confirm clk disable
			s3c_udc_power_down();
			break;
		case CLOCK_GATING: 		
			s3c_udc_suspend_clock_gating();
			break;
		case OPHYPWR_FORCE_SUSPEND:			
			writel((readl(S3C_USBOTG_PHYPWR)|1), S3C_USBOTG_PHYPWR);
			break;
		default:
			printk("[%s]: not proper pm_policy\n", __FUNCTION__);
	}
	return 0;
}
//---------------------------------------------------------------------------------------

static int s3c_udc_resume(struct platform_device *pdev)
{
//	struct s3c_udc *dev = platform_get_drvdata(pdev);
	u32 tmp;
	
	DEBUG_PM("[%s]: System Resume \n", __FUNCTION__);

	//if not suspended as connected
#ifndef CONFIG_MACH_SMDK6410
	if (dev->udc_resume_state == USB_STATE_CONFIGURED)
	{
		DEBUG_PM("[%s]: USB connected before suspend \n", __FUNCTION__);
	}
	else
	{	
		DEBUG_PM("[%s]: USB not connected before suspend\n", __FUNCTION__);
		return 0;
	}
#endif /* CONFIG_MACH_SMDK6410 */
	switch(pm_policy)
	{
		case ALL_POWER_DOWN:			
			s3c_udc_power_up();
			break;
		case CLOCK_GATING:		
			s3c_udc_resume_clock_gating();
			break;
		case OPHYPWR_FORCE_SUSPEND: 		
			tmp = readl(S3C_USBOTG_PHYPWR);
			tmp &= ~(1);
			writel(tmp, S3C_USBOTG_PHYPWR);
			break;
		default:
			printk("[%s]: not proper pm_policy\n", __FUNCTION__);
	}
	return 0;
}

//---------------------------------------------------------------------------------------
#endif /* CONFIG_PM */

/**
 * struct platform_driver s3c_udc_driver
 */
static struct platform_driver s3c_udc_driver = {
	.probe		= s3c_udc_probe,
	.remove		= s3c_udc_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c6410-usbgadget",
	},
#ifdef CONFIG_PM
	.suspend	= s3c_udc_suspend,
	.resume 	= s3c_udc_resume,
#endif /* CONFIG_PM */
};
//---------------------------------------------------------------------------------------

static int __init udc_init(void)
{
	int ret;

	ret = platform_driver_register(&s3c_udc_driver);

	return ret;
}
//---------------------------------------------------------------------------------------

static void __exit udc_exit(void)
{
	platform_driver_unregister(&s3c_udc_driver);
	printk("Unloaded %s version %s\n", driver_name, DRIVER_VERSION);
}
//---------------------------------------------------------------------------------------

module_init(udc_init);
module_exit(udc_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("SeungSoo Yange");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

