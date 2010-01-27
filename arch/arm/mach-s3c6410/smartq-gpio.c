/*
 * 2010-0120, Jackal Chan <jackal.cvkk@gmail.com>
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
   
   ret = gpio_request(S3C64XX_GPK(1), "GPK");
   if (ret < 0) {
      pr_err("%s: failed to request GPK1 for WiFi power control.\n", __func__);
      return ret;
   }
   ret = gpio_request(S3C64XX_GPK(2), "GPK");
   if (ret < 0) {
      pr_err("%s: failed to request GPK2 for WiFi reset.\n", __func__);
      return ret;
   }
   
   /* turn power on */
   gpio_direction_output(S3C64XX_GPK(1), 1);
   
   /* reset device */
   gpio_direction_output(S3C64XX_GPK(2), 0);
   mdelay(100);
   gpio_set_value(S3C64XX_GPK(2), 1);
   mdelay(100);
   
   gpio_free(S3C64XX_GPK(1));
   gpio_free(S3C64XX_GPK(2));
   return 0;
}

static int __init s3c6410_smartq_gpio_init(void)
{
   /* WiFi: power save and reset control */
   smartq_wifi_init();
   /* SD  : WP */
   gpio_direction_input(S3C64XX_GPK(0)); /* GPIO-104 */   
   /* AUDIO: headphone status GPIO initial */
   gpio_direction_input(S3C64XX_GPL(12)); /* GPIO-133 */
   /* AUDIO: speaker output GPIO initial */
   gpio_direction_output(S3C64XX_GPK(12),1); /* GPIO-116 */
   /* USB: external power GPL0 */
   gpio_direction_output(S3C64XX_GPL(0),1); /* GPIO-121 */
   /* USB:               GPL1 */
   gpio_direction_output(S3C64XX_GPL(1),1); /* GPIO-122 */
   /* AC insert detect GPL13 */
   gpio_direction_input(S3C64XX_GPL(13)); /* GPIO-134 */
   return 0;
}
__initcall(s3c6410_smartq_gpio_init);
