/**
  * @file smartq-gpio.c
  * 
  * Project : SmartQ5 MID
  * 
  * Description : 
  *   S3C6410 GPIO programming for SmartQ5
  * 
  * ChangeLog:
  *   0.1 2010-0120, 
  *       - initial 
  * 
  * @author Jackal Chan(jackal.cvkk@gmail.com) , Copyright (C) 2010 Covia
  * @date   2010-0120
  */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <mach/hardware.h>
#include <mach/gpio.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>

#include <plat/regs-timer.h>
#include <mach/regs-irq.h>

#if 1 /* TERRY(2010-0317): GPIO relative PM support */
#include <plat/pm.h>
#endif

#define DEV_VERSION "0.1 2010-0120"

#define HEADPHONE_DEBOUCE_INTERVAL 100 /* ms */

static struct timer_list gpio_headp_sts_timer;
extern void otg_phy_init(u32);

static void gpio_headp_sts_check(unsigned long _data)
{
   int pin_level = (gpio_get_value(S3C64XX_GPL(12))) ? 0 : 1;
   gpio_set_value(S3C64XX_GPK(12), pin_level);
}
static irqreturn_t smartq_headphone_detect_isr(int irq, void *dev_id)
{
   mod_timer(&gpio_headp_sts_timer, 
	     jiffies + msecs_to_jiffies(HEADPHONE_DEBOUCE_INTERVAL));
   return IRQ_HANDLED;
}

#if 1 /* TERRY(2010-0317): GPIO relative PM support */
static int wifi_pwr_en = 0;

/*
 * WIFI Power control via GPIO
 */
int smartq_gpio_wifi_en(int power_state)
{
	int ret = 0;

	if (wifi_pwr_en == power_state)
		return 0;

	if ((ret = gpio_request(S3C64XX_GPK(1), "GPK")) < 0) {
		pr_err("%s: failed to request GPK1 for WiFi power control, errno %d.\n",
			__func__, ret);
		return ret;
	}

	if ((ret = gpio_request(S3C64XX_GPK(2), "GPK")) < 0) {
		pr_err("%s: failed to request GPK1 for WiFi reset, errno %d.\n",
			__func__, ret);
		goto release2;
	}

	if (power_state) {
		/* Power ON */
		if ((ret = gpio_direction_output(S3C64XX_GPK(1), 1)) < 0) {
			pr_err("%s: failed to configure output direction for GPK1\n", __func__);
			goto release1;
		}
		mdelay(100); // Warming up
		/* Reset */
		if ((ret = gpio_direction_output(S3C64XX_GPK(2), 0)) < 0) {
			pr_err("%s: failed to configure output direction for GPK2\n", __func__);
			goto release1;
		}
		mdelay(100);
		/* Recover Reset to high */
		gpio_set_value(S3C64XX_GPK(2), 1);
		wifi_pwr_en = 1;
	} else {
		/* Power OFF */
		if ((ret = gpio_direction_output(S3C64XX_GPK(1), 0)) < 0) {
			pr_err("%s: failed to configure output direction for GPK1\n", __func__);
			goto release1;
		}
		wifi_pwr_en = 0;
	}
release1:
	gpio_free(S3C64XX_GPK(1));

release2:
	gpio_free(S3C64XX_GPK(2));
	return ret;
}
EXPORT_SYMBOL(smartq_gpio_wifi_en);

int smartq_gpio_wifi_state(void)
{
	return wifi_pwr_en;
}
EXPORT_SYMBOL(smartq_gpio_wifi_state);

/*
 * LED control via GPIO
 * led_state:
 *   0 - OFF
 *   1 - GREEN
 *   2 - RED
 *   3 - ORANGE
 */
int smartq_gpio_led_ctl(int led_state)
{
	int ret = 0;

	if ((ret = gpio_request(S3C64XX_GPN(8), "led_red")) < 0) {
		pr_err("%s: failed to request GPN8 for LED control.\n", __func__);
		return ret;
	}
   
	if ((ret = gpio_request(S3C64XX_GPN(9), "led_green")) < 0) {
		pr_err("%s: failed to request GPN9 for LED control.\n", __func__);
		gpio_free(S3C64XX_GPN(8));
		goto out2;
	}
   
	/* Initialize */
	if ((ret = gpio_direction_output(S3C64XX_GPN(8), 0)) < 0) {
		pr_err("%s: failed to configure output direction for GPN8, errno %d\n", 
				__func__,ret);
		goto out1;
	}
	if ((ret = gpio_direction_output(S3C64XX_GPN(9), 0)) < 0) {
		pr_err("%s: failed to configure output direction for GPN9, errno %d\n", 
				__func__,ret);
		goto out1;
	}

	/* Set LED state */
	if (!(led_state & 1)) gpio_set_value(S3C64XX_GPN(8), 1);
	if (!(led_state & 2)) gpio_set_value(S3C64XX_GPN(9), 1);

out1:
	gpio_free(S3C64XX_GPN(8));
out2:
	gpio_free(S3C64XX_GPN(9));

	return ret;
}
EXPORT_SYMBOL(smartq_gpio_led_ctl);
#endif

static int smartq_gpio_audio_init(void)
{
   int ret = 0, irq;
   
   /* Speaker */
   ret = gpio_request(S3C64XX_GPK(12), "GPK");
   if (ret < 0) {
      pr_err("%s: failed to request GPK12 for audio speaker, errno %d.\n", 
	     __func__,ret);
      return ret;
   }
   
   ret = gpio_direction_output(S3C64XX_GPK(12), 1);
   if (ret < 0) {
      pr_err("%s: failed to configure output direction for GPK12, errno %d.\n",
	     __func__,ret);
      goto err;
   }
   
   /* Headphone status detection */   
#if 1 /* TERRY(2010-0318): Fix compile warning */
   setup_timer(&gpio_headp_sts_timer, gpio_headp_sts_check, 0);
#else
   setup_timer(&gpio_headp_sts_timer, gpio_headp_sts_check, NULL);
#endif

   ret = gpio_request(S3C64XX_GPL(12), "GPL");
   if (ret < 0) {
      pr_err("%s: failed to request GPL12 for audio headphone, errno %d.\n", 
	     __func__,ret);
      return ret;
   }
   ret = gpio_direction_input(S3C64XX_GPL(12));
   if (ret < 0) {
      pr_err("%s: failed to configure input direction for GPL12, errno %d.\n",
	     __func__,ret);
      goto err;
   }
   irq = gpio_to_irq(S3C64XX_GPL(12));
   if (irq < 0) {
      ret = irq;
      pr_err("%s: unable to get irq number for GPL12, errno %d.\n",
	     __func__,ret);
      goto err;
   }
   ret = request_irq(irq, 
		     smartq_headphone_detect_isr, 
		     IRQF_SAMPLE_RANDOM | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		     "Headphone detect", NULL);
   if (ret) {
      pr_err("%s: unable to claim irq %d, errno %d.\n",
	     __func__,irq,ret);
      goto err;
   }
   
   gpio_set_value(S3C64XX_GPK(12), gpio_get_value(S3C64XX_GPL(12)) ? 0 : 1);

   return ret;
   
err:
   gpio_free(S3C64XX_GPK(12));
   gpio_free(S3C64XX_GPL(12));
   
   return ret;
}

static int smartq_gpio_usb_init(void)
{
   int ret = 0;
   
   ret = gpio_request(S3C64XX_GPL(0), "GPL");
   if (ret < 0) {
      pr_err("%s: failed to request GPL0 for USB host/OTG power, errno %d.\n",
	     __func__,ret);
      return ret;
   }
   ret = gpio_request(S3C64XX_GPL(1), "GPL");
   if (ret < 0) {
      pr_err("%s: failed to request GPL1 for internal USB host port power , errno %d.\n", 
	     __func__,ret);
      gpio_free(S3C64XX_GPL(0));
      return ret;
   }
   ret = gpio_request(S3C64XX_GPL(8), "GPL");
   if (ret < 0) {
      pr_err("%s: failed to request GPL8 for external USB host port power , errno %d.\n",
	     __func__,ret);
      gpio_free(S3C64XX_GPL(0));
      gpio_free(S3C64XX_GPL(1));
      return ret;
   }
   ret = gpio_request(S3C64XX_GPL(11), "GPL");
   if (ret < 0) {
      pr_err("%s: failed to request GPL11 for external USB host port(over current detection) , errno %d.\n", 
	     __func__,ret);
      gpio_free(S3C64XX_GPL(0));
      gpio_free(S3C64XX_GPL(1));
      gpio_free(S3C64XX_GPL(8));
      return ret;
   }
   
   /* turn power on : usb host */
   ret = gpio_direction_output(S3C64XX_GPL(0), 1);
   if ( ret < 0) {
      pr_err("%s: failed to configure output direction for GPL0, errno %d\n", 
	     __func__,ret);
      goto err;
   }
   /* turn power on : external usb host port */
   ret = gpio_direction_output(S3C64XX_GPL(8), 1);
   if ( ret < 0) {
      pr_err("%s: failed to configure output direction for GPL8, errno %d\n",
	     __func__,ret);
      goto err;
   }
   /* turn power off : internal usb host port */
   ret = gpio_direction_output(S3C64XX_GPL(1), 0);
   if ( ret < 0) {
      pr_err("%s: failed to configure output direction for GPL1, errno %d\n", 
	     __func__,ret);
      goto err;
   }
   /* Over current detection (external USB port) */
   ret = gpio_direction_input(S3C64XX_GPL(11));
   if ( ret < 0){
      pr_err("%s: failed to configure input direction for GPL11, errno %d\n",
	     __func__,ret);
      goto err;
   }
   
err:
   
   return ret;
}

static void smartq_gpio_free(void)
{
        /* USB */
        gpio_free(S3C64XX_GPL(0));
        gpio_free(S3C64XX_GPL(1));
        gpio_free(S3C64XX_GPL(8));
        gpio_free(S3C64XX_GPL(11));
}

/***************************************************************
 * implement sysfs
 ***************************************************************/
static int smartq_sysfs_show_usbextpwr(struct device *dev, 
				       struct device_attribute *attr, 
				       char *buf)
{
   return snprintf(buf, PAGE_SIZE, "%d\n", gpio_get_value(S3C64XX_GPL(8)) );
}

static ssize_t smartq_sysfs_store_usbextpwr(struct device *dev,
					   struct device_attribute *attr, 
					   const char *buf, 
					   size_t len)
{
   if (len < 1) return -EINVAL;
   
   if (strnicmp(buf, "on", 2) == 0 || strnicmp(buf, "1", 1) == 0) {
      gpio_set_value(S3C64XX_GPL(8), 1);
      otg_phy_init(0x42);
   } else if (strnicmp(buf, "off", 3) == 0 || strnicmp(buf, "0", 1) == 0) {
      gpio_set_value(S3C64XX_GPL(8), 0);
      otg_phy_init(0x02); /* UMIT */
   } else {
     return -EINVAL;
   }
   
   return len;
}

static DEVICE_ATTR(usbextpwr_en, 0666,smartq_sysfs_show_usbextpwr,smartq_sysfs_store_usbextpwr);

#if 1 /* TERRY(2010-0421): Wifi sysfs for userspace and debug */
static int smartq_sysfs_show_wifi_en(struct device *dev, 
				       struct device_attribute *attr, 
				       char *buf)
{
   return snprintf(buf, PAGE_SIZE, "%d\n", wifi_pwr_en);
}

static ssize_t smartq_sysfs_store_wifi_en(struct device *dev,
					   struct device_attribute *attr, 
					   const char *buf, 
					   size_t len)
{
   if (len < 1) return -EINVAL;
   
   if (strnicmp(buf, "on", 2) == 0 || strnicmp(buf, "1", 1) == 0) {
      smartq_gpio_wifi_en(1);
   } else if (strnicmp(buf, "off", 3) == 0 || strnicmp(buf, "0", 1) == 0) {
      smartq_gpio_wifi_en(0);
   } else {
      return -EINVAL;
   }
   
   return len;
}

static DEVICE_ATTR(wifi_en, 0666,smartq_sysfs_show_wifi_en,smartq_sysfs_store_wifi_en);
#endif

static struct attribute *smartq_attrs[] ={
   &dev_attr_usbextpwr_en.attr,
#if 1 /* TERRY(2010-0421): Wifi sysfs for userspace and debug */
   &dev_attr_wifi_en.attr,
#endif
   NULL,   
};
static struct attribute_group smartq_attr_group ={
   .attrs = smartq_attrs,
};

/***************************************************************
 * implement platform device 
 ***************************************************************/
static int smartq_gpio_probe(struct platform_device *pdev)
{
   int ret;
   struct device *dev = &pdev->dev;
   
   printk(KERN_INFO"SmartQ GPIO initial, ver %s\n",DEV_VERSION);
   
   ret = sysfs_create_group(&dev->kobj, &smartq_attr_group);
   if (ret < 0) {
      pr_err("%s: failed to create sysfs.\n",__func__);
      return -1;
   }
   
   smartq_gpio_audio_init();
   smartq_gpio_usb_init();
   
   /* SD  : WP */
   gpio_direction_input(S3C64XX_GPK(0)); /* GPIO-104 */
   
   return 0;
}

static int smartq_gpio_remove(struct platform_device *dev)
{
   smartq_gpio_free();
   
   return 0;
}

#if 1 /* TERRY(2010-0317): GPIO relative PM support */
static struct sleep_save_phy usb_gpio_reg[] =  {
	SAVE_ITEM(S3C64XX_GPL(0)),    /* USB Host Power */
	SAVE_ITEM(S3C64XX_GPL(8)),    /* External USB */
	SAVE_ITEM(S3C64XX_GPL(1)),    /* Internal USB */
	SAVE_ITEM(S3C64XX_GPL(11)),   /* External Over Current Detect */
};

/*
 * USB power enable/disable via GPIO. When disabling, save GPIO state
 * and activate them when enabling.  This function is for PM.
 */
static void smartq_gpio_usb_en(int power_state)
{
	int i;

	if (power_state) {
		/* Restore USB registers */
		for (i = 0; i < ARRAY_SIZE(usb_gpio_reg); i++) {
			gpio_set_value(usb_gpio_reg[i].reg, usb_gpio_reg[i].val);
		}
	} else {
		/* Backup GPIO status */
		for (i = 0; i < ARRAY_SIZE(usb_gpio_reg); i++) {
			usb_gpio_reg[i].val = gpio_get_value(usb_gpio_reg[i].reg);
		}
		/* Power OFF */
		for (i = ARRAY_SIZE(usb_gpio_reg) - 1; i >= 0; i--) {
			gpio_set_value(usb_gpio_reg[i].reg, 0);
		}
	}
}

int smartq_gpio_suspend_late(struct platform_device *dev, pm_message_t state)
{
	/* USB power off */
	smartq_gpio_usb_en(0);

	/* Turn LED off */
	smartq_gpio_led_ctl(0);

	return 0;
}

int smartq_gpio_resume_early(struct platform_device *dev)
{
	/* No need to turn LED on because battery driver will do it for us */

	/* USB power on */
	smartq_gpio_usb_en(1);

	return 0;
}
#endif

static struct platform_driver smartq_gpio = {
     .probe   = smartq_gpio_probe,
     .remove  = smartq_gpio_remove,
     //.suspend = smartq_gpio_suspend,
     //.resume  = smartq_gpio_resume,
#if 1 /* TERRY(2010-0317): GPIO relative PM support */
     .suspend_late = smartq_gpio_suspend_late,
     .resume_early  = smartq_gpio_resume_early,
#endif
     .driver = {
	  .name = "smartq_gpio",
	  .owner = THIS_MODULE,
     },
};

static int __init smartq_gpio_init(void)
{
   return platform_driver_register(&smartq_gpio);
}
        
static void __exit smartq_gpio_exit(void)
{
   platform_driver_unregister(&smartq_gpio);
}

module_init(smartq_gpio_init);
module_exit(smartq_gpio_exit);
        
MODULE_AUTHOR("Jackal");
MODULE_DESCRIPTION("SmartQ5 GPIO driver");
MODULE_LICENSE("GPL");
