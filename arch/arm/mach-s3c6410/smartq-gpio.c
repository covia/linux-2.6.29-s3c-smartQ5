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

#define DEV_VERSION "0.1 2010-0120"

#define HEADPHONE_DEBOUCE_INTERVAL 100 /* ms */

static struct timer_list gpio_headp_sts_timer;
extern otg_phy_init(u32);

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

static int smartq_gpio_wifi_init(void)
{
   int ret = 0;
   
   ret = gpio_request(S3C64XX_GPK(1), "GPK");
   if (ret < 0) {
      pr_err("%s: failed to request GPK1 for WiFi power control, errno %d.\n",
	     __func__,ret);
      return ret;
   }
   
   ret = gpio_request(S3C64XX_GPK(2), "GPK");
   if (ret < 0) {
      pr_err("%s: failed to request GPK2 for WiFi reset, errno %d.\n", 
	     __func__,ret);
      goto err2;
   }
   
   /* turn power on */
   ret = gpio_direction_output(S3C64XX_GPK(1), 1);
   if ( ret < 0) {
      pr_err("%s: failed to configure output direction for GPK1, errno %d\n", 
	     __func__,ret);
      goto err1;
   }
    /* reset device */
   ret = gpio_direction_output(S3C64XX_GPK(2), 0);
   if ( ret < 0){
      pr_err("%s: failed to configure output direction for GPK2, errno %d\n",
	     __func__,ret);
      goto err1;
   }
   mdelay(100);
   gpio_set_value(S3C64XX_GPK(2), 1);
   mdelay(100);
   
err1:
   gpio_free(S3C64XX_GPK(2));
   
err2:
   gpio_free(S3C64XX_GPK(1));
   
   return ret;
}

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
   setup_timer(&gpio_headp_sts_timer, gpio_headp_sts_check, NULL);

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
static void smartq_gpio_usb_free(void)
{
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

static struct attribute *smartq_attrs[] ={
   &dev_attr_usbextpwr_en.attr,
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
   
   /* WiFi: power save and reset control */
   smartq_gpio_wifi_init();
   smartq_gpio_audio_init();
   smartq_gpio_usb_init();
   
   /* SD  : WP */
   gpio_direction_input(S3C64XX_GPK(0)); /* GPIO-104 */
   
   return 0;
}

static int smartq_gpio_remove(struct platform_device *dev)
{
   smartq_gpio_usb_free();
   
   return 0;
}

static struct platform_driver smartq_gpio = {
     .probe   = smartq_gpio_probe,
     .remove  = smartq_gpio_remove,
     //.suspend = smartq_gpio_suspend,
     //.resume  = smartq_gpio_resume,
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
