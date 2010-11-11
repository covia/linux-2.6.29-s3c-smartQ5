/* 
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
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h> 
#include <linux/sysfs.h>
#include <mach/gpio.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>   // added by chris(CVKK)  2010/11/03

static struct rfkill *rfkill;

static int bt_rfkill_get_state(void *data, enum rfkill_state *state)
{	
	if (gpio_get_value(S3C64XX_GPL(1))) {
		*state = RFKILL_STATE_UNBLOCKED;
	} else {
		*state = RFKILL_STATE_SOFT_BLOCKED;
	}
	return 0;
}

// modified by chris(CVKK)  2010/11/03
static int bt_rfkill_toggle_radio(void *data, enum rfkill_state state)
{
	switch (state) {
		case RFKILL_STATE_SOFT_BLOCKED:
            gpio_set_value(S3C64XX_GPL(1), 0);			
			break;
		case RFKILL_STATE_UNBLOCKED:
            gpio_set_value(S3C64XX_GPL(1), 1);			
			break;
		case RFKILL_STATE_HARD_BLOCKED:
			break;
		case RFKILL_STATE_MAX:
			break;
	}
    mdelay(50);
	
    return 0;
}
// end by chris(CVKK)

static int __devinit bt_probe(struct platform_device *pdev)
{
        int ret;

        rfkill = rfkill_allocate(&pdev->dev, RFKILL_TYPE_BLUETOOTH);
        if (!rfkill) {
                printk("Unable to allocate RFKILL device.\n");
                ret = -ENOMEM;
                return ret;
        }

	// chmod. User needs permission to read the status of bluetooth.
        rfkill->dev.class->dev_attrs[2].attr.mode = S_IRUGO | S_IWUGO,

        rfkill->state = gpio_get_value(S3C64XX_GPL(1)) ? 
	                     RFKILL_STATE_UNBLOCKED : RFKILL_STATE_SOFT_BLOCKED;  // chris(CVKK)  2010/11/03

        rfkill->user_claim_unsupported = 1;
        rfkill->name = pdev->name;
        rfkill->data = pdev;
        rfkill->toggle_radio = bt_rfkill_toggle_radio;
		rfkill->get_state = bt_rfkill_get_state;
		rfkill->dev.class->suspend = NULL;
        rfkill->dev.class->resume = NULL;

        ret = rfkill_register(rfkill);
        if (ret) {
           printk("Bluetooth RFKILL register failed.\n");
        }

        platform_set_drvdata(pdev, rfkill); 

        return 0;
}

static struct platform_driver bt_driver = {
	.probe		= bt_probe,
	//.remove		= __devexit_p(bt_remove),
	.driver		= {
		.name	= "smdk6410-bluetooth",
		.owner	= THIS_MODULE,
	}
};

static int __init bt_init(void)
{
	return platform_driver_register(&bt_driver);
}

static void __exit bt_exit(void)
{
	rfkill_unregister(rfkill);
	platform_driver_unregister(&bt_driver);
}

module_init(bt_init);
module_exit(bt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Covia");
MODULE_DESCRIPTION("RF device interface for smdk6410 bluetooth.");
MODULE_SUPPORTED_DEVICE("SmartQ5");

