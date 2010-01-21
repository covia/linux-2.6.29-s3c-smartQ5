/*
 * drivers/video/s3c/s3cfb_a070vw04.c
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

#define S3C_FB_HFP   40              /* front porch */
#define S3C_FB_HSW   1		      /* Hsync width */
#define S3C_FB_HBP   216              /* Back porch */
#define S3C_FB_VFP   10               /* front porch */
#define S3C_FB_VSW   1               /* Vsync width */
#define S3C_FB_VBP   35		     /* Back porch */

#define S3C_FB_HRES		800	/* horizon pixel  x resolition */
#define S3C_FB_VRES		480	/* line cnt       y resolution */
#define S3C_FB_HRES_VIRTUAL	800	/* horizon pixel  x resolition */
#define S3C_FB_VRES_VIRTUAL	960	/* line cnt       y resolution */

#define S3C_FB_HRES_OSD		800	/* horizon pixel  x resolition */
#define S3C_FB_VRES_OSD		480	/* line cnt       y resolution */
#define S3C_FB_VFRAME_FREQ     	60	/* frame rate freq */
#define S3C_FB_PIXEL_CLOCK	(S3C_FB_VFRAME_FREQ * (S3C_FB_HFP + S3C_FB_HSW + S3C_FB_HBP + S3C_FB_HRES) * (S3C_FB_VFP + S3C_FB_VSW + S3C_FB_VBP + S3C_FB_VRES))

#define LCD_SCEN S3C64XX_GPM(0)
#define LCD_SCL  S3C64XX_GPM(1)
#define LCD_SDA  S3C64XX_GPM(2)
#define S3C_GPIO_OUTP 1
#define S3C_GPIO_INP  0

static int lcd_write(unsigned char,unsigned char);
static void lcd_spi_stop(void);
static void lcd_spi_start(void);

int lcd_power = OFF;
EXPORT_SYMBOL(lcd_power);

int lcd_power_ctrl(s32 value);
EXPORT_SYMBOL(lcd_power_ctrl);

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
   } else {
      printk(KERN_INFO "LCD power off sequence start\n");
      if (gpio_is_valid(S3C64XX_GPM(3))) {
	 err = gpio_request(S3C64XX_GPM(3), "GPM");
	 if (err) {
	    printk(KERN_ERR "failed to request GPM for "
		   "lcd reset control\n");
	    return -1;
	 }
	 gpio_direction_output(S3C64XX_GPM(5), 0);
      }
      printk(KERN_INFO "LCD power off sequence end\n");
   }
   
   gpio_free(S3C64XX_GPM(3));
   lcd_power = value;
   
   return 0;
}

#define WAITTIME    (10 * HZ / 1000)    // 10ms
#define BACKLIGHT_STATUS_ALC    0x100
#define BACKLIGHT_LEVEL_VALUE   0x0FF   /* 0 ~ 255 */

#define BACKLIGHT_LEVEL_MIN             1
#define BACKLIGHT_LEVEL_DEFAULT (BACKLIGHT_STATUS_ALC | 0xFF)   /* Default Setting */
#define BACKLIGHT_LEVEL_MAX     (BACKLIGHT_STATUS_ALC | BACKLIGHT_LEVEL_VALUE)

static int old_display_brightness = BACKLIGHT_LEVEL_DEFAULT;

static void __set_brightness(int val)
{
   int channel = 1;  // must use channel-1
   int usec = 0;       // don't care value
   unsigned long tcnt=1000;
   unsigned long tcmp=0;
   
   if(val == BACKLIGHT_LEVEL_MAX)
     gpio_direction_output(S3C64XX_GPF(15), 1);
   else if(val == BACKLIGHT_LEVEL_MIN)
     gpio_direction_output(S3C64XX_GPF(15), 0);
   else {
      tcmp = val * 10;
      
#if defined(CONFIG_S3C6410_PWM) && defined(CONFIG_PWM)
      s3c6410_timer_setup (channel, usec, tcnt, tcmp);
#endif
   }
}

static void set_brightness(int val)
{
   int old_val = old_display_brightness;

   if(val < 0) val = 0;
   if(val > BACKLIGHT_LEVEL_MAX) val = BACKLIGHT_LEVEL_MAX;
   
   if(val > old_val) {
      while((++old_val) < val) {
	 __set_brightness(old_val);
	 set_current_state(TASK_INTERRUPTIBLE);
	 schedule_timeout(WAITTIME);
      }
   } else {
      while((--old_val) > val) {
	 __set_brightness(old_val);
	 set_current_state(TASK_INTERRUPTIBLE);
	 schedule_timeout(WAITTIME);
      }
   }
   
   __set_brightness(val);
   old_display_brightness = val;
}

/* 2009-1030, added by CVKK(JC) , For android light */
extern void s3cfb_set_brightness(int);
extern int s3c_display_brightness;
  
static void set_backlight_power(int val)
{
   if(val > 0)
     __set_brightness(old_display_brightness);
   else
     __set_brightness(BACKLIGHT_LEVEL_MIN);
}

static void s3cfb_set_fimd_info(void)
{
   s3c_fimd.vidcon1 = S3C_VIDCON1_IHSYNC_INVERT | S3C_VIDCON1_IVSYNC_INVERT |S3C_VIDCON1_IVDEN_INVERT;
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
   
   s3c_fimd.osd_xres_virtual = S3C_FB_HRES_OSD;
   s3c_fimd.osd_yres_virtual = S3C_FB_VRES_OSD;
   
   s3c_fimd.pixclock = S3C_FB_PIXEL_CLOCK;
   
   s3c_fimd.hsync_len = S3C_FB_HSW;
   s3c_fimd.vsync_len = S3C_FB_VSW;
   s3c_fimd.left_margin = S3C_FB_HFP;
   s3c_fimd.upper_margin = S3C_FB_VFP;
   s3c_fimd.right_margin = S3C_FB_HBP;
   s3c_fimd.lower_margin = S3C_FB_VBP;
   
   s3c_fimd.set_lcd_power = lcd_power_ctrl;
   s3c_fimd.set_backlight_power = set_backlight_power;
   s3c_fimd.set_brightness = set_brightness;
   s3c_fimd.backlight_min = BACKLIGHT_LEVEL_MIN;
   s3c_fimd.backlight_max = BACKLIGHT_LEVEL_MAX;
}

static void lcd_spi_start(void)
{
   //set scen output 0
   s3c_gpio_cfgpin(LCD_SCEN, S3C_GPIO_OUTP);
   gpio_set_value(LCD_SCEN, 0);
}

static void lcd_spi_stop(void)
{
   //set scen output 1
   s3c_gpio_cfgpin(LCD_SCEN,S3C_GPIO_OUTP);
   gpio_set_value(LCD_SCEN,1);
   gpio_set_value(LCD_SCL,0);
   gpio_set_value(LCD_SDA,0);
   s3c_gpio_cfgpin(LCD_SCL,S3C_GPIO_INP);
   s3c_gpio_cfgpin(LCD_SDA,S3C_GPIO_INP);
}

static int lcd_write(unsigned char addr,unsigned char data)
{
   unsigned char myaddr,mydata,i;
   
   myaddr = (addr&0x3f)<<1 ;
   myaddr <<= 1;
   myaddr |= 0x1;
   
   lcd_spi_start();

   s3c_gpio_cfgpin(LCD_SCL, S3C_GPIO_OUTP);
   gpio_set_value(LCD_SCL, 0);
   s3c_gpio_cfgpin(LCD_SDA,S3C_GPIO_OUTP);
   
   for ( i = 0; i < 8; i++) {
      gpio_set_value(LCD_SCL, 0);
      udelay(1);
      gpio_set_value(LCD_SDA, (myaddr&0x80)>>7);
      myaddr <<= 1 ;
      udelay(1);
      gpio_set_value(LCD_SCL,1);
      udelay(1);
   } //8nd is null turn
   mydata = data;
   for(i=0;i<8;i++){
      gpio_set_value(LCD_SCL,0);
      udelay(1);
      gpio_set_value(LCD_SDA,(mydata&0x80)>>7);
      mydata <<= 1;
      udelay(1);
      gpio_set_value(LCD_SCL,1);
      udelay(1);
   }
   
   lcd_spi_stop();
   
   return 0;
}

void lcd_init_hw(void)
{
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
}

void s3cfb_init_hw(void)
{
   printk(KERN_INFO "LCD TYPE :: TD043MTEX will be initialized\n");

   s3cfb_set_fimd_info();
   s3cfb_set_gpio();
   lcd_init_hw();
}

