/*
 * 2010-0120, Jackal Chan <jackal.cvkk@gmail.com>
 */
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#include <mach/gpio.h>

#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>

static struct gpio_keys_button smartq_buttons[] = {
     {
	  .gpio           = S3C64XX_GPN(2),
	  .code           = KEY_BACK,
	  .desc           = "KEY4",
	  .active_low     = 1,
	  .debounce_interval = 5,
     },
     {
	  .gpio           = S3C64XX_GPN(15),
	  .code           = KEY_MENU,
	  .desc           = "KEY3",
	  .active_low     = 1,
	  .debounce_interval = 5,
     },
     {
	  .gpio           = S3C64XX_GPN(12),
	  .code           = KEY_HOME,
	  .desc           = "KEY2",
	  .active_low     = 1,
	  .debounce_interval = 5,
     },
};

static struct gpio_keys_platform_data smartq_button_data = {
     .buttons        = smartq_buttons,
     .nbuttons       = ARRAY_SIZE(smartq_buttons),
};

struct platform_device smartq_button_device  = {
     .name           = "gpio-keys",
     .id             = 0,
     .num_resources  = 0,
     .dev            = {
	.platform_data  = &smartq_button_data,
     }
};

static struct gpio_keys_button smartq_pwr_button[] = {
     {
	  .gpio           = S3C64XX_GPL(14),
	  .code           = KEY_POWER,
	  .desc           = "KEY5",
	  .active_low     = 1,
	  .debounce_interval = 100,
     },
};

static struct gpio_keys_platform_data smartq_pwr_button_data = {
     .buttons        = smartq_pwr_button,
     .nbuttons       = ARRAY_SIZE(smartq_pwr_button),
     .rep            = 0,
};

struct platform_device smartq_pwr_button_device = {
     .name           = "gpio-keys",
     .id             = 1,
     .num_resources  = 0,
     .dev            = {
	.platform_data  = &smartq_pwr_button_data,
     }
};
