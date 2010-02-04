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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/mutex.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>

#include <plat/regs-timer.h>
#include <mach/regs-irq.h>

#define HEADPHONE_DEBOUCE_INTERVAL 100 /* ms */

static struct timer_list gpio_headp_sts_timer;

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

static int __init smartq_wifi_init(void)
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

static int __init smartq_audio_init(void)
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

static int __init s3c6410_smartq_gpio_init(void)
{
   /* WiFi: power save and reset control */
   smartq_wifi_init();
   
   smartq_audio_init();
   
   /* SD  : WP */
   gpio_direction_input(S3C64XX_GPK(0)); /* GPIO-104 */
   
   /* USB: external power GPL0 */
   gpio_direction_output(S3C64XX_GPL(0),1); /* GPIO-121 */
   /* USB:               GPL1 */
   gpio_direction_output(S3C64XX_GPL(1),1); /* GPIO-122 */
   /* AC insert detect GPL13 */
   //gpio_direction_input(S3C64XX_GPL(13)); /* GPIO-134 */
   
   return 0;
}
__initcall(s3c6410_smartq_gpio_init);
