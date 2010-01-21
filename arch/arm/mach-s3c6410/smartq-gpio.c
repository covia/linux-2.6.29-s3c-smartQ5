/* 
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

static int __init smartq_wifi_init(void)
{
   int ret;
   
   ret = gpio_request(S3C64XX_GPK(1), "wifi_enable");
   if (ret < 0) {
      pr_err("%s: failed to get GPK1\n", __func__);
      return ret;
   }
   
   ret = gpio_request(S3C64XX_GPK(2), "wifi_reset");
   if (ret < 0) {
      pr_err("%s: failed to get GPK2\n", __func__);
      gpio_free(S3C64XX_GPK(1));
      return ret;
   }
   
   /* turn power on */
   gpio_direction_output(S3C64XX_GPK(1), 1);
   
   /* reset device */
   gpio_direction_output(S3C64XX_GPK(2), 0);
   mdelay(100);
   gpio_set_value(S3C64XX_GPK(2), 1);
   
   return 0;
}

static int __init smartq_sd_wp_init(void)
{
   gpio_direction_input(S3C64XX_GPK(0));
}

static int __init s3c6410_smartq_gpio_init(void)
{
   printk(KERN_INFO"SMARTQ: WiFI GPIO initial.\n");
   smartq_wifi_init();
   printk(KERN_INFO"SMARTQ: SD WP initial.\n");   
   smartq_sd_wp_init();
   
   return 0;
}
__initcall(s3c6410_smartq_gpio_init);
