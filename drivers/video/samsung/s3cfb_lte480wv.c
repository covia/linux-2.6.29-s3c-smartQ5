/*
 * drivers/video/samsung/s3cfb_lte480wv.c
 *
 * $Id: s3cfb_lte480wv.c,v 1.12 2008/06/05 02:13:24 jsgood Exp $
 *
 * Copyright (C) 2008 Jinsung Yang <jsgood.yang@samsung.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	S3C Frame Buffer Driver
 *	based on skeletonfb.c, sa1100fb.h, s3c2410fb.c
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>

#include <plat/regs-gpio.h>
#include <plat/regs-lcd.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-clock.h>

#include "s3cfb.h"

#define BACKLIGHT_STATUS_ALC	0x100
#define BACKLIGHT_LEVEL_VALUE	0x0FF	/* 0 ~ 255 */

#define BACKLIGHT_LEVEL_MIN		1
#define BACKLIGHT_LEVEL_DEFAULT	(BACKLIGHT_STATUS_ALC | 0xFF)	/* Default Setting */
#define BACKLIGHT_LEVEL_MAX		(BACKLIGHT_STATUS_ALC | BACKLIGHT_LEVEL_VALUE)

int lcd_power = OFF;
EXPORT_SYMBOL(lcd_power);

int lcd_power_ctrl(s32 value);
EXPORT_SYMBOL(lcd_power_ctrl);

int backlight_power = OFF;
EXPORT_SYMBOL(backlight_power);

void backlight_power_ctrl(s32 value);
EXPORT_SYMBOL(backlight_power_ctrl);

int backlight_level = BACKLIGHT_LEVEL_DEFAULT;
EXPORT_SYMBOL(backlight_level);

void backlight_level_ctrl(s32 value);
EXPORT_SYMBOL(backlight_level_ctrl);

#if 23
#define S3C_FB_HFP		40	/* front porch */
#define S3C_FB_HSW		1	/* hsync width */
#define S3C_FB_HBP		216	/* back porch */
#define S3C_FB_VFP		10	/* front porch */
#define S3C_FB_VSW		1	/* vsync width */
#define S3C_FB_VBP		35	/* back porch */
#else
#define S3C_FB_HFP		8	/* front porch */
#define S3C_FB_HSW		3	/* hsync width */
#define S3C_FB_HBP		13	/* back porch */
#define S3C_FB_VFP		5	/* front porch */
#define S3C_FB_VSW		1	/* vsync width */
#define S3C_FB_VBP		7	/* back porch */
#endif


#define S3C_FB_HRES		800	/* horizon pixel  x resolition */
#define S3C_FB_VRES		480	/* line cnt       y resolution */

#define S3C_FB_HRES_VIRTUAL	S3C_FB_HRES     /* horizon pixel  x resolition */
#define S3C_FB_VRES_VIRTUAL	S3C_FB_VRES * 2 /* line cnt       y resolution */

#define S3C_FB_HRES_OSD		800	/* horizon pixel  x resolition */
#define S3C_FB_VRES_OSD		480	/* line cnt       y resolution */

#define S3C_FB_HRES_OSD_VIRTUAL	S3C_FB_HRES_OSD     /* horizon pixel  x resolition */
#define S3C_FB_VRES_OSD_VIRTUAL S3C_FB_VRES_OSD * 2 /* line cnt       y resolution */

#if 23
#define S3C_FB_VFRAME_FREQ     	60	/* frame rate freq */
#else
#define S3C_FB_VFRAME_FREQ     	75	/* frame rate freq */
#endif

#define S3C_FB_PIXEL_CLOCK	(S3C_FB_VFRAME_FREQ * (S3C_FB_HFP + S3C_FB_HSW + S3C_FB_HBP + S3C_FB_HRES) * (S3C_FB_VFP + S3C_FB_VSW + S3C_FB_VBP + S3C_FB_VRES))

#if 1
#define S3C_GPIONO(bank,offset) ((bank) + (offset))
#define S3C_GPIO_BANKM   (32*15)
#define S3C_GPM0       S3C_GPIONO(S3C_GPIO_BANKM, 0)
#define S3C_GPM1       S3C_GPIONO(S3C_GPIO_BANKM, 1)
#define S3C_GPM2       S3C_GPIONO(S3C_GPIO_BANKM, 2)

//#define LCD_SCEN S3C_GPM0 S3C64XX_GPM(0)
#define LCD_SCEN S3C_GPM0
#define LCD_SCL S3C_GPM1
#define LCD_SDA S3C_GPM2
#define S3C_GPIO_OUTP 1
#define S3C_GPIO_INP  0

#define S3C_GPIO_BANKA	 (32*0)
#define S3C_GPIO_BANKB	 (32*1)
#define S3C_GPIO_BANKC	 (32*2)
#define S3C_GPIO_BANKD	 (32*3)
#define S3C_GPIO_BANKE	 (32*4)
#define S3C_GPIO_BANKF	 (32*5)
#define S3C_GPIO_BANKG	 (32*6)
#define S3C_GPIO_BANKH	 (32*7)
#define S3C_GPIO_BANKI	 (32*8)
#define S3C_GPIO_BANKJ	 (32*9)
#define S3C_GPIO_BANKO	 (32*10)
#define S3C_GPIO_BANKP	 (32*11)
#define S3C_GPIO_BANKQ	 (32*12)

#define S3C_GPIO_BASE(pin)   ((pin & ~31) >> 5)
#define S3C_GPIO_OFFSET(pin) (pin & 31)

#define S3C_GPIONO(bank,offset) ((bank) + (offset))

#define S3C_GPIO_BANKK   (32*13)
#define S3C_GPIO_BANKL	 (32*14)
#define S3C_GPIO_BANKN   (32*16)

#define S3C_GPH0	S3C_GPIONO(S3C_GPIO_BANKH, 0)
#define S3C_GPK0       S3C_GPIONO(S3C_GPIO_BANKK, 0)
#define S3C_GPL0       S3C_GPIONO(S3C_GPIO_BANKL, 0)

#define S3C_GPL8       S3C_GPIONO(S3C_GPIO_BANKL, 8)
#define S3C_GPL14	S3C_GPIONO(S3C_GPIO_BANKL, 14)
#define S3C_GPK8       S3C_GPIONO(S3C_GPIO_BANKK, 8)
#define S3C_GPK15	S3C_GPIONO(S3C_GPIO_BANKK, 15)
#define S3C_GPH8	S3C_GPIONO(S3C_GPIO_BANKH, 8)
#define S3C_GPH9	S3C_GPIONO(S3C_GPIO_BANKH, 9)

static int lcd_write(unsigned char,unsigned char);
static void lcd_init_hw(void);
static void lcd_spi_stop(void);
static void lcd_spi_start(void);

static void __iomem *gpio_base_offset[]=
{
       S3C64XX_VA_GPIO + 0x00,         //GPA ,4bit
       S3C64XX_VA_GPIO + 0x20,         //GPB ,4bit
       S3C64XX_VA_GPIO + 0x40,         //GPC ,4bit
       S3C64XX_VA_GPIO + 0x60,         //GPD ,4bit
       S3C64XX_VA_GPIO + 0x80,         //GPE ,4bit
       S3C64XX_VA_GPIO + 0xA0,         //GPF ,2bit
       S3C64XX_VA_GPIO + 0xC0,         //GPG ,4bit
       S3C64XX_VA_GPIO + 0xE0,         //GPH ,4bit
       S3C64XX_VA_GPIO + 0x100,        //GPI ,2bit
       S3C64XX_VA_GPIO + 0x120,        //GPJ ,2bit
       S3C64XX_VA_GPIO + 0x140,        //GPO ,2bit
       S3C64XX_VA_GPIO + 0x160,        //GPP ,2bit
       S3C64XX_VA_GPIO + 0x180,        //GPQ ,2bit
       S3C64XX_VA_GPIO + 0x800,        //GPK ,4bit
       S3C64XX_VA_GPIO + 0x810,        //GPL ,4bit
       S3C64XX_VA_GPIO + 0x820,        //GPM ,4bit
       S3C64XX_VA_GPIO + 0x830         //GPN ,2bit
};

void s3c_gpio_setpin_2(unsigned int pin, unsigned int to) 
{
        void __iomem *base = gpio_base_offset[S3C_GPIO_BASE(pin)];
        unsigned long offs = S3C_GPIO_OFFSET(pin);
        unsigned long flags;
        unsigned long dat;

        if((pin>=S3C_GPH0 && pin<=S3C_GPH9)||\
                (pin>=S3C_GPK0 && pin<=S3C_GPK15)||\
                (pin>=S3C_GPL0 && pin<=S3C_GPL14))
        {
                base = base+0x04;
        }

        local_irq_save(flags);
        local_irq_disable();

        dat = __raw_readl(base + 0x04);
        dat &= ~(1 << offs);
        dat |= to << offs;
        __raw_writel(dat, base + 0x04);

        local_irq_restore(flags);
}

static void s3c_gpio_cfgpin_2(unsigned int pin, unsigned int function)
{
        void __iomem *base = gpio_base_offset[S3C_GPIO_BASE(pin)];
        unsigned long mask;
        unsigned long con;
        unsigned long flags;
        unsigned long offs =  S3C_GPIO_OFFSET(pin);

        if ((pin < S3C_GPIO_BANKF)||((pin >=S3C_GPIO_BANKG)&&\
                (pin<S3C_GPIO_BANKI))||((pin>=S3C_GPIO_BANKK)&&\
                (pin<S3C_GPIO_BANKN)))
        {
                offs = (offs) *4; 

                if((pin == S3C_GPH8)||(pin==S3C_GPH9)||\
                        (pin>=S3C_GPK8&&pin<=S3C_GPK15)||\
                        (pin>=S3C_GPL8&&pin<=S3C_GPL14))
                {
                        base = base + 0x04;
                        /*  only for con1:8~14 or 15 regiter configuratio nvalue change ...*/
                        offs = offs - 32; 
                }
                mask = 0xF << offs;
        }
        else
        {
               offs = offs*2;
                mask = 0x3 << offs;
        }

        local_irq_save(flags);
        local_irq_disable();

        con  = __raw_readl(base + 0x00);
        con &= ~mask;
        con |= (function << offs);

        __raw_writel(con, base + 0x00);

        local_irq_restore(flags);
}


static void lcd_spi_start(void)
{
	//set scen output 0
	s3c_gpio_cfgpin_2(LCD_SCEN,S3C_GPIO_OUTP);
	s3c_gpio_setpin_2(LCD_SCEN,0);
}
static void lcd_spi_stop(void)
{
	//set scen output 1
	s3c_gpio_cfgpin_2(LCD_SCEN,S3C_GPIO_OUTP);
	s3c_gpio_setpin_2(LCD_SCEN,1);
	s3c_gpio_setpin_2(LCD_SCL,0);
	s3c_gpio_setpin_2(LCD_SDA,0);
	s3c_gpio_cfgpin_2(LCD_SCL,S3C_GPIO_INP);
	s3c_gpio_cfgpin_2(LCD_SDA,S3C_GPIO_INP);
}
static int lcd_write(unsigned char addr,unsigned char data)
{
	unsigned char myaddr,mydata,i;
	myaddr = (addr&0x3f)<<1 ;
	myaddr <<= 1;
	myaddr |= 0x1;
	lcd_spi_start();

	s3c_gpio_cfgpin_2(LCD_SCL,S3C_GPIO_OUTP);
	s3c_gpio_setpin_2(LCD_SCL,0);
	s3c_gpio_cfgpin_2(LCD_SDA,S3C_GPIO_OUTP);
	for(i=0;i<8;i++){
	    s3c_gpio_setpin_2(LCD_SCL,0);
	    udelay(1);
	    s3c_gpio_setpin_2(LCD_SDA,(myaddr&0x80)>>7);
	    myaddr <<= 1 ;
	    udelay(1);
	    s3c_gpio_setpin_2(LCD_SCL,1);
	    udelay(1);
	} //8nd is null turn
	mydata = data;
	for(i=0;i<8;i++){
	    s3c_gpio_setpin_2(LCD_SCL,0);
	    udelay(1);
	    s3c_gpio_setpin_2(LCD_SDA,(mydata&0x80)>>7);
	    mydata <<= 1;
	    udelay(1);
	    s3c_gpio_setpin_2(LCD_SCL,1);
	    udelay(1);
	}

	lcd_spi_stop();

	return 0;
}
#endif

static void s3cfb_set_fimd_info(void)
{
	s3c_fimd.vidcon1 = S3C_VIDCON1_IHSYNC_INVERT | S3C_VIDCON1_IVSYNC_INVERT | S3C_VIDCON1_IVDEN_NORMAL;
	s3c_fimd.vidtcon0 = S3C_VIDTCON0_VBPD(S3C_FB_VBP - 1) | S3C_VIDTCON0_VFPD(S3C_FB_VFP - 1) | S3C_VIDTCON0_VSPW(S3C_FB_VSW - 1);
	s3c_fimd.vidtcon1 = S3C_VIDTCON1_HBPD(S3C_FB_HBP - 1) | S3C_VIDTCON1_HFPD(S3C_FB_HFP - 1) | S3C_VIDTCON1_HSPW(S3C_FB_HSW - 1);
	s3c_fimd.vidtcon2 = S3C_VIDTCON2_LINEVAL(S3C_FB_VRES - 1) | S3C_VIDTCON2_HOZVAL(S3C_FB_HRES - 1);

	s3c_fimd.vidosd0a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3c_fimd.vidosd0b = S3C_VIDOSDxB_OSD_RBX_F(S3C_FB_HRES - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3C_FB_VRES - 1);

	s3c_fimd.vidosd1a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3c_fimd.vidosd1b = S3C_VIDOSDxB_OSD_RBX_F(S3C_FB_HRES_OSD - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3C_FB_VRES_OSD - 1);

	s3c_fimd.width = S3C_FB_HRES;
	s3c_fimd.height = S3C_FB_VRES;
	s3c_fimd.xres = S3C_FB_HRES;
	s3c_fimd.yres = S3C_FB_VRES;

#if defined(CONFIG_FB_S3C_VIRTUAL_SCREEN)
	s3c_fimd.xres_virtual = S3C_FB_HRES_VIRTUAL;
	s3c_fimd.yres_virtual = S3C_FB_VRES_VIRTUAL;
#else
	s3c_fimd.xres_virtual = S3C_FB_HRES;
	s3c_fimd.yres_virtual = S3C_FB_VRES;
#endif

	s3c_fimd.osd_width = S3C_FB_HRES_OSD;
	s3c_fimd.osd_height = S3C_FB_VRES_OSD;
	s3c_fimd.osd_xres = S3C_FB_HRES_OSD;
	s3c_fimd.osd_yres = S3C_FB_VRES_OSD;

#if 23
	s3c_fimd.osd_xres_virtual = S3C_FB_HRES_OSD;
	s3c_fimd.osd_yres_virtual = S3C_FB_VRES_OSD;
#else
#if defined(CONFIG_FB_S3C_VIRTUAL_SCREEN)
	s3c_fimd.osd_xres_virtual = S3C_FB_HRES_OSD_VIRTUAL;
	s3c_fimd.osd_yres_virtual = S3C_FB_VRES_OSD_VIRTUAL;
#else
	s3c_fimd.osd_xres_virtual = S3C_FB_HRES_OSD;
	s3c_fimd.osd_yres_virtual = S3C_FB_VRES_OSD;
#endif
#endif

	s3c_fimd.pixclock = S3C_FB_PIXEL_CLOCK;

	s3c_fimd.hsync_len = S3C_FB_HSW;
	s3c_fimd.vsync_len = S3C_FB_VSW;
	s3c_fimd.left_margin = S3C_FB_HFP;
	s3c_fimd.upper_margin = S3C_FB_VFP;
	s3c_fimd.right_margin = S3C_FB_HBP;
	s3c_fimd.lower_margin = S3C_FB_VBP;

	s3c_fimd.set_lcd_power = lcd_power_ctrl;
	s3c_fimd.set_backlight_power = backlight_power_ctrl;
	s3c_fimd.set_brightness = backlight_level_ctrl;

	s3c_fimd.backlight_min = BACKLIGHT_LEVEL_MIN;
	s3c_fimd.backlight_max = BACKLIGHT_LEVEL_MAX;
}
#if  defined(CONFIG_S3C6410_PWM)
extern void s3cfb_set_brightness(int val);
#else
void s3cfb_set_brightness(int val){}
#endif
int lcd_power_ctrl(s32 value)
{
	int err;

	if (value) {
		printk(KERN_INFO "LCD power on sequence start\n");
		if (gpio_is_valid(S3C64XX_GPM(3))) {
			err = gpio_request(S3C64XX_GPM(3), "GPM");

			if (err) {
				printk(KERN_ERR "failed to request GPM for "
					"lcd reset control\n");
				return -1;
			}
			gpio_direction_output(S3C64XX_GPM(3), 1);
		}
		printk(KERN_INFO "LCD power on sequence end\n");
	}
	else {
		printk(KERN_INFO "LCD power off sequence start\n");
		if (gpio_is_valid(S3C64XX_GPM(3))) {
			err = gpio_request(S3C64XX_GPM(3), "GPM");

			if (err) {
				printk(KERN_ERR "failed to request GPM for "
					"lcd reset control\n");
				return -1;
			}
			gpio_direction_output(S3C64XX_GPM(3), 0);
		}
		printk(KERN_INFO "LCD power off sequence end\n");
	}
	gpio_free(S3C64XX_GPM(3));
	lcd_power = value;
	return 0;
}

static void backlight_ctrl(s32 value)
{
	int err, ret;

	if (value) {
		/* backlight ON */
		if (lcd_power == OFF) {
			
			ret = lcd_power_ctrl(ON);
			if (ret != 0) {
				printk(KERN_ERR "lcd power on control is failed\n");
				return;
			}
		}
		
		if (gpio_is_valid(S3C64XX_GPF(15))) {
			err = gpio_request(S3C64XX_GPF(15), "GPF");

			if (err) {
				printk(KERN_ERR "failed to request GPF for "
					"lcd backlight control\n");
			}

			gpio_direction_output(S3C64XX_GPF(15), 1);
		}
		s3cfb_set_brightness((int)(value/3));
	}
	else {
		ret = lcd_power_ctrl(OFF);
		if (ret != 0) {
			printk(KERN_ERR "lcd power off control is failed\n");
		}
		/* backlight OFF */
		if (gpio_is_valid(S3C64XX_GPF(15))) {
			err = gpio_request(S3C64XX_GPF(15), "GPF");

			if (err) {
				printk(KERN_ERR "failed to request GPF for "
					"lcd backlight control\n");
			}

			gpio_direction_output(S3C64XX_GPF(15), 0);
		 }
	}
	gpio_free(S3C64XX_GPF(15));
}

void backlight_level_ctrl(s32 value)
{
	if ((value < BACKLIGHT_LEVEL_MIN) ||	/* Invalid Value */
		(value > BACKLIGHT_LEVEL_MAX) ||
		(value == backlight_level))	/* Same Value */
		return;

	if (backlight_power)
		s3cfb_set_brightness((int)(value/3));	
	
	backlight_level = value;	
}

void backlight_power_ctrl(s32 value)
{
	if ((value < OFF) ||	/* Invalid Value */
		(value > ON) ||
		(value == backlight_power))	/* Same Value */
		return;

	backlight_ctrl((value ? backlight_level : OFF));	
	
	backlight_power = value;	
}

#define SMDK_DEFAULT_BACKLIGHT_BRIGHTNESS	255
static DEFINE_MUTEX(smdk_backlight_lock);

static void smdk_set_backlight_level(u8 level)
{
	if (backlight_level == level)
		return;

	backlight_ctrl(level);
	
	backlight_level = level;
}

static void smdk_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	mutex_lock(&smdk_backlight_lock);
	smdk_set_backlight_level(value);
	mutex_unlock(&smdk_backlight_lock);
}

static struct led_classdev smdk_backlight_led  = {
	.name		= "lcd-backlight",
	.brightness = SMDK_DEFAULT_BACKLIGHT_BRIGHTNESS,
	.brightness_set = smdk_brightness_set,
};

static int smdk_bl_probe(struct platform_device *pdev)
{
	led_classdev_register(&pdev->dev, &smdk_backlight_led);
	return 0;
}

static int smdk_bl_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&smdk_backlight_led);
	return 0;
}

#ifdef CONFIG_PM
static int smdk_bl_suspend(struct platform_device *pdev, pm_message_t state)
{
	led_classdev_suspend(&smdk_backlight_led);
	return 0;
}

static int smdk_bl_resume(struct platform_device *dev)
{
	led_classdev_resume(&smdk_backlight_led);
	return 0;
}
#else
#define smdk_bl_suspend	NULL
#define smdk_bl_resume	NULL
#endif

static struct platform_driver smdk_bl_driver = {
	.probe		= smdk_bl_probe,
	.remove		= smdk_bl_remove,
	.suspend	= smdk_bl_suspend,
	.resume		= smdk_bl_resume,
	.driver		= {
		.name	= "smdk-backlight",
	},
};

static int __init smdk_bl_init(void)
{
	printk("SMDK board LCD Backlight Device Driver (c) 2008 Samsung Electronics.\n");

	backlight_ctrl(0);
	mdelay(100);
	backlight_ctrl(255);
	mdelay(100);

	platform_driver_register(&smdk_bl_driver);
	return 0;
}

static void __exit smdk_bl_exit(void)
{
 	platform_driver_unregister(&smdk_bl_driver);
}

module_init(smdk_bl_init);
module_exit(smdk_bl_exit);

MODULE_AUTHOR("Jongpill Lee <boyko.lee@samsung.com>");
MODULE_DESCRIPTION("SMDK board Backlight Driver");
MODULE_LICENSE("GPL");

void s3cfb_init_hw(void)
{
	printk(KERN_INFO "LCD TYPE :: LTE480WV will be initialized.\n");

	s3cfb_set_fimd_info();
	s3cfb_set_gpio();
	lcd_init_hw();
}

static void lcd_init_hw(void)
{
#if 1
	printk("\t\nlcd_init_hw (%d)\n", __LINE__);
	lcd_spi_stop();

	lcd_write(0x02,0x07);
	lcd_write(0x03,0x5f);
	lcd_write(0x04,0x17);

	lcd_write(0x05,0x20);
	lcd_write(0x06,0x08);

	lcd_write(0x07,0x20);
	lcd_write(0x08,0x20);
	lcd_write(0x09,0x20);
	lcd_write(0x0a,0x20);

	lcd_write(0x0b,0x20);
	lcd_write(0x0c,0x20);
	lcd_write(0x0d,0x22);

	lcd_write(0x0e,0x10);
	lcd_write(0x0f,0x10);
	lcd_write(0x10,0x10);

	lcd_write(0x11,0x15);
	lcd_write(0x12,0xaa);
	lcd_write(0x13,0xff);
	lcd_write(0x14,0x86);
	lcd_write(0x15,0x89);
	lcd_write(0x16,0xc6);
	lcd_write(0x17,0xea);
	lcd_write(0x18,0x0c);
	lcd_write(0x19,0x33);
	lcd_write(0x1a,0x5e);
	lcd_write(0x1b,0xd0);
	lcd_write(0x1c,0x33);
	lcd_write(0x1d,0x7e);
	lcd_write(0x1e,0xb3);
	lcd_write(0x1f,0xff);
	lcd_write(0x20,0xf0);
	lcd_write(0x21,0xf0);
	lcd_write(0x22,0x08);
#endif
}

