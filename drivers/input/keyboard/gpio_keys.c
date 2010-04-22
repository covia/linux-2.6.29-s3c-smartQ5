/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>

#include <asm/gpio.h>

#if 1 /* 2009-0903, modified by CVKK(JC) */
#define HOLD_KEY_TIME 1*HZ
#define MULTI_FUN_KEY_DEF KEY_MENU  /* keycode: 139 */
#define MULTI_FUN_KEY_OTH KEY_SYSRQ /* keycode:  99 */ 
static struct timer_list cv_handle_multi_key_timer;
static int multi_key_code = MULTI_FUN_KEY_DEF;

#define MULTI_FUN_KEY_DEF_02 KEY_BACK     /* keycode: 158 */
#define MULTI_FUN_KEY_OTH_02 KEY_LINEFEED /* keycode: 101 */
static struct timer_list cv_handle_multi_key_02_timer;
static int multi_key_code_02 = MULTI_FUN_KEY_DEF_02;
#endif
#if 23
#define MULTI_FUN_KEY_PLUS    158    // SmartQ '+' key 
#define MULTI_FUN_KEY_PLUS2   101      
static struct timer_list cv_handle_multi_key_plus_timer;
static int multi_key_plus_code = MULTI_FUN_KEY_PLUS;
#endif

struct gpio_button_data {
	struct gpio_keys_button *button;
	struct input_dev *input;
	struct timer_list timer;
};

struct gpio_keys_drvdata {
	struct input_dev *input;
	struct gpio_button_data data[0];
};

#if 1 /* TERRY(2010-0401): Simulate power key when system wakes up */
/* 
 * When the PWR key(scancode: 116) wakes up the device, the key event is dropped
 * and cannot be passed to userspace.  As a result, Android WindowManager or
 * KeyGuard will not get notified to wake themselves by only one keypress.  In
 * order to fix this issue, we implement a function for wakelock.c to simulate 
 * the key event when system wakes up (either from button or alarm) to fix this
 * issue.
 */

struct input_dev *input_device;

void gpio_keys_trigger_wakeup_key(void)
{
	if (!input_device) {
		return;
	}

	input_event(input_device, EV_KEY, KEY_POWER, 1);
	input_sync(input_device);
	mdelay(5);
	input_event(input_device, EV_KEY, KEY_POWER, 0);
	input_sync(input_device);
}
EXPORT_SYMBOL(gpio_keys_trigger_wakeup_key);
#endif

static void gpio_keys_report_event(struct gpio_button_data *bdata)
{
	struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned int type = button->type ?: EV_KEY;
	int state = (gpio_get_value(button->gpio) ? 1 : 0) ^ button->active_low;

#if 1 /* 2009-0903, modified by CVKK(JC) */
	if (button->code == MULTI_FUN_KEY_DEF) {
		if (state) {
			mod_timer(&cv_handle_multi_key_timer, jiffies + HOLD_KEY_TIME);
		} else {
			if (button->code == multi_key_code) {
				del_timer(&cv_handle_multi_key_timer);
				input_event(input, type, button->code, 1);
				input_sync(input);
				mdelay(button->debounce_interval);
				input_event(input, type, multi_key_code, state);
				input_sync(input);
			}
			multi_key_code = MULTI_FUN_KEY_DEF;
		}
        } else if (button->code == MULTI_FUN_KEY_DEF_02) {
		if (state) {
			mod_timer(&cv_handle_multi_key_02_timer, jiffies + HOLD_KEY_TIME);
		} else {
			if (button->code == multi_key_code_02) {
				del_timer(&cv_handle_multi_key_02_timer);
				input_event(input, type, button->code, 1);
				input_sync(input);
				mdelay(button->debounce_interval);
				input_event(input, type, multi_key_code_02, state);
				input_sync(input);
			}
			multi_key_code_02 = MULTI_FUN_KEY_DEF_02;
		}
	} 
	else if (button->code == MULTI_FUN_KEY_PLUS) {
	   if (state) {
	      mod_timer(&cv_handle_multi_key_plus_timer, jiffies + HOLD_KEY_TIME);
	   } else {
	      if (button->code == multi_key_plus_code) {
		 del_timer(&cv_handle_multi_key_plus_timer);
		 input_event(input, type, button->code, 1);
		 input_sync(input);
		 mdelay(button->debounce_interval);
		 input_event(input, type, multi_key_plus_code, state);
		 input_sync(input);
	      }
	      multi_key_plus_code = MULTI_FUN_KEY_PLUS;
	   }
	} 
	else {
		input_event(input, type, button->code, !!state);
		input_sync(input);
	}
#else   
	input_event(input, type, button->code, !!state);
	input_sync(input);
#endif
}

#if 1 /* 2009-0903, added by CVKK(JC) */
static void cv_handle_multi_key(unsigned long _data)
{
	struct gpio_button_data *data = (struct gpio_button_data *)_data;
	struct gpio_keys_button *button = data->button;
	struct input_dev *input = data->input;
	unsigned int type = button->type ?: EV_KEY;

	multi_key_code = MULTI_FUN_KEY_OTH;
	input_event(input, type, multi_key_code, 1);
	input_sync(input);
	mdelay(button->debounce_interval);
	input_event(input, type, multi_key_code, 0);
	input_sync(input);
}
static void cv_handle_multi_key_02(unsigned long _data)
{
	struct gpio_button_data *data = (struct gpio_button_data *)_data;
	struct gpio_keys_button *button = data->button;
	struct input_dev *input = data->input;
	unsigned int type = button->type ?: EV_KEY;

	multi_key_code_02 = MULTI_FUN_KEY_OTH_02;
	input_event(input, type, multi_key_code_02, 1);
	input_sync(input);
	mdelay(button->debounce_interval);
	input_event(input, type, multi_key_code_02, 0);
	input_sync(input);
}
static void cv_handle_multi_key_plus(unsigned long _data)
{
   struct gpio_button_data *data = (struct gpio_button_data *)_data;
   struct gpio_keys_button *button = data->button;
   struct input_dev *input = data->input;
   unsigned int type = button->type ?: EV_KEY;

   multi_key_plus_code = MULTI_FUN_KEY_PLUS2;
   input_event(input, type, multi_key_plus_code, 1);
   input_sync(input);
   mdelay(button->debounce_interval);
   input_event(input, type, multi_key_plus_code, 0);
   input_sync(input);
}
#endif

static void gpio_check_button(unsigned long _data)
{
	struct gpio_button_data *data = (struct gpio_button_data *)_data;
	gpio_keys_report_event(data);
}

static irqreturn_t gpio_keys_isr(int irq, void *dev_id)
{
	struct gpio_button_data *bdata = dev_id;
	struct gpio_keys_button *button = bdata->button;

	BUG_ON(irq != gpio_to_irq(button->gpio));

	if (button->debounce_interval)
		mod_timer(&bdata->timer,
			jiffies + msecs_to_jiffies(button->debounce_interval));
	else
		gpio_keys_report_event(bdata);

	return IRQ_HANDLED;
}

static int __devinit gpio_keys_probe(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_drvdata *ddata;
	struct input_dev *input;
	int i, error;
	int wakeup = 0;

	ddata = kzalloc(sizeof(struct gpio_keys_drvdata) +
			pdata->nbuttons * sizeof(struct gpio_button_data),
			GFP_KERNEL);
	input = input_allocate_device();
	if (!ddata || !input) {
		error = -ENOMEM;
		goto fail1;
	}

	platform_set_drvdata(pdev, ddata);

	input->name = pdev->name;
	input->phys = "gpio-keys/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* Enable auto repeat feature of Linux input subsystem */
	if (pdata->rep)
		__set_bit(EV_REP, input->evbit);

	ddata->input = input;

#if 1 /* TERRY(2010-0401): Save the address of input */
	input_device = input;
#endif

	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio_keys_button *button = &pdata->buttons[i];
		struct gpio_button_data *bdata = &ddata->data[i];
		int irq;
		unsigned int type = button->type ?: EV_KEY;

		bdata->input = input;
		bdata->button = button;
		setup_timer(&bdata->timer,
			    gpio_check_button, (unsigned long)bdata);
#if 1 /* 2009-0903, added by CVKK(JC) */
		if (button->code == MULTI_FUN_KEY_DEF)
			setup_timer(&cv_handle_multi_key_timer, cv_handle_multi_key, (unsigned long)bdata);
		if (button->code == MULTI_FUN_KEY_DEF_02)
			setup_timer(&cv_handle_multi_key_02_timer, cv_handle_multi_key_02, (unsigned long)bdata);
	        if (button->code == MULTI_FUN_KEY_PLUS)
	            setup_timer(&cv_handle_multi_key_plus_timer, cv_handle_multi_key_plus, 
			                  (unsigned long)bdata);
#endif	   

		error = gpio_request(button->gpio, button->desc ?: "gpio_keys");
		if (error < 0) {
			pr_err("gpio-keys: failed to request GPIO %d,"
				" error %d\n", button->gpio, error);
			goto fail2;
		}

		error = gpio_direction_input(button->gpio);
		if (error < 0) {
			pr_err("gpio-keys: failed to configure input"
				" direction for GPIO %d, error %d\n",
				button->gpio, error);
			gpio_free(button->gpio);
			goto fail2;
		}

		irq = gpio_to_irq(button->gpio);
		if (irq < 0) {
			error = irq;
			pr_err("gpio-keys: Unable to get irq number"
				" for GPIO %d, error %d\n",
				button->gpio, error);
			gpio_free(button->gpio);
			goto fail2;
		}

		error = request_irq(irq, gpio_keys_isr,
				    IRQF_SAMPLE_RANDOM | IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
				    button->desc ? button->desc : "gpio_keys",
				    bdata);
		if (error) {
			pr_err("gpio-keys: Unable to claim irq %d; error %d\n",
				irq, error);
			gpio_free(button->gpio);
			goto fail2;
		}

		if (button->wakeup)
			wakeup = 1;

		input_set_capability(input, type, button->code);
#if 1 /* 2009-0903 , added by CVKK(JC) */
		if (button->code == MULTI_FUN_KEY_DEF)
			input_set_capability(input, type, MULTI_FUN_KEY_OTH);
		if (button->code == MULTI_FUN_KEY_DEF_02)
			input_set_capability(input, type, MULTI_FUN_KEY_OTH_02);
	        if (button->code == MULTI_FUN_KEY_PLUS)
	                input_set_capability(input, type, MULTI_FUN_KEY_PLUS2);
#endif	   
	}

	error = input_register_device(input);
	if (error) {
		pr_err("gpio-keys: Unable to register input device, "
			"error: %d\n", error);
		goto fail2;
	}

	device_init_wakeup(&pdev->dev, wakeup);

	return 0;

 fail2:
	while (--i >= 0) {
		free_irq(gpio_to_irq(pdata->buttons[i].gpio), &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		gpio_free(pdata->buttons[i].gpio);
	}

	platform_set_drvdata(pdev, NULL);
 fail1:
	input_free_device(input);
	kfree(ddata);

	return error;
}

static int __devexit gpio_keys_remove(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

#if 1 /* TERRY(2010-0401): Clear the address of input */
	input_device = NULL;
#endif

	device_init_wakeup(&pdev->dev, 0);

	for (i = 0; i < pdata->nbuttons; i++) {
		int irq = gpio_to_irq(pdata->buttons[i].gpio);
		free_irq(irq, &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		gpio_free(pdata->buttons[i].gpio);
	}

	input_unregister_device(input);

	return 0;
}


#ifdef CONFIG_PM
static int gpio_keys_suspend(struct platform_device *pdev, pm_message_t state)
{
#if 0 /* TERRY(2010-0401): We already set the wake up source in mach-smdk6410.c */
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

	if (device_may_wakeup(&pdev->dev)) {
		for (i = 0; i < pdata->nbuttons; i++) {
			struct gpio_keys_button *button = &pdata->buttons[i];
			if (button->wakeup) {
				int irq = gpio_to_irq(button->gpio);
				enable_irq_wake(irq);
			}
		}
	}
#endif
	return 0;
}

static int gpio_keys_resume(struct platform_device *pdev)
{
#if 0 /* TERRY(2010-0401): We already set the wake up source in mach-smdk6410.c */
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

	if (device_may_wakeup(&pdev->dev)) {
		for (i = 0; i < pdata->nbuttons; i++) {
			struct gpio_keys_button *button = &pdata->buttons[i];
			if (button->wakeup) {
				int irq = gpio_to_irq(button->gpio);
				disable_irq_wake(irq);
			}
		}
	}
#endif
	return 0;
}
#else
#define gpio_keys_suspend	NULL
#define gpio_keys_resume	NULL
#endif

static struct platform_driver gpio_keys_device_driver = {
	.probe		= gpio_keys_probe,
	.remove		= __devexit_p(gpio_keys_remove),
	.suspend	= gpio_keys_suspend,
	.resume		= gpio_keys_resume,
	.driver		= {
		.name	= "gpio-keys",
		.owner	= THIS_MODULE,
	}
};

static int __init gpio_keys_init(void)
{
	return platform_driver_register(&gpio_keys_device_driver);
}

static void __exit gpio_keys_exit(void)
{
	platform_driver_unregister(&gpio_keys_device_driver);
}

module_init(gpio_keys_init);
module_exit(gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>");
MODULE_DESCRIPTION("Keyboard driver for CPU GPIOs");
MODULE_ALIAS("platform:gpio-keys");
