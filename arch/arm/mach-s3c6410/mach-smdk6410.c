/* linux/arch/arm/mach-s3c6410/mach-smdk6410.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/reboot.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/regs-mem.h>
#include <mach/gpio-core.h>

#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <plat/iic.h>

#include <plat/regs-rtc.h>
#include <plat/regs-clock.h>
#include <plat/regs-gpio.h>

#include <plat/nand.h>
#include <plat/partition.h>
#include <plat/s3c6410.h>
#include <plat/clock.h>
#include <plat/regs-clock.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/ts.h>
#include <plat/adc.h>
#include <plat/reserved_mem.h>
#include <plat/pm.h>
#include <plat/pll.h>
#include <plat/spi.h>

#include <linux/android_pmem.h>

#ifdef CONFIG_USB_SUPPORT
#include <plat/regs-otg.h>
#include <linux/usb/ch9.h>

/* S3C_USB_CLKSRC 0: EPLL 1: CLK_48M */
#define S3C_USB_CLKSRC	1
#define USB_HOST_PORT2_EN // JC

#ifdef USB_HOST_PORT2_EN
//#define OTGH_PHY_CLK_VALUE      (0x60)  /* Serial Interface, otg_phy input clk 48Mhz Oscillator */
#define OTGH_PHY_CLK_VALUE      (0x42)  /* Serial Interface, otg_phy input clk 48Mhz Oscillator */
#else
#define OTGH_PHY_CLK_VALUE      (0x20)  /* UTMI Interface, otg_phy input clk 48Mhz Oscillator */
#endif
#endif

#define UCON S3C_UCON_DEFAULT | S3C_UCON_UCLK
#define ULCON S3C_LCON_CS8 | S3C_LCON_PNONE | S3C_LCON_STOPB
#define UFCON S3C_UFCON_RXTRIG8 | S3C_UFCON_FIFOMODE

#ifndef CONFIG_HIGH_RES_TIMERS
extern struct sys_timer s3c_timer;
#else
extern struct sys_timer sec_timer;
#endif /* CONFIG_HIGH_RES_TIMERS */

extern void s3c64xx_reserve_bootmem(void);

#if defined(CONFIG_SPI_CNTRLR_0) || defined(CONFIG_SPI_CNTRLR_1) 
static void s3c_cs_suspend(int pin, pm_message_t pm)
{
	/* Whatever need to be done */
}

static void s3c_cs_resume(int pin)
{
	/* Whatever need to be done */
}

static void cs_set(int pin, int lvl)
{
	unsigned int val;

	val = __raw_readl(S3C64XX_GPFDAT);
	val &= ~(1<<pin);

	if(lvl == CS_HIGH)
	   val |= (1<<pin);

	__raw_writel(val, S3C64XX_GPFDAT);
}

static void s3c_cs_setF13(int pin, int lvl)
{
	cs_set(13, lvl);
}

static void s3c_cs_setF14(int pin, int lvl)
{
	cs_set(14, lvl);
}

static void s3c_cs_setF15(int pin, int lvl)
{
	cs_set(15, lvl);
}

static void cs_cfg(int pin, int pull)
{
	unsigned int val;

	val = __raw_readl(S3C64XX_GPFCON);
	val &= ~(3<<(pin*2));
	val |= (1<<(pin*2)); /* Output Mode */
	__raw_writel(val, S3C64XX_GPFCON);

	val = __raw_readl(S3C64XX_GPFPUD);
	val &= ~(3<<(pin*2));

	if(pull == CS_HIGH)
	   val |= (2<<(pin*2));	/* PullUp */
	else
	   val |= (1<<(pin*2)); /* PullDown */

	__raw_writel(val, S3C64XX_GPFPUD);
}

static void s3c_cs_configF13(int pin, int mode, int pull)
{
	cs_cfg(13, pull);
}

static void s3c_cs_configF14(int pin, int mode, int pull)
{
	cs_cfg(14, pull);
}

static void s3c_cs_configF15(int pin, int mode, int pull)
{
	cs_cfg(15, pull);
}

static void s3c_cs_set(int pin, int lvl)
{
	if(lvl == CS_HIGH)
	   s3c_gpio_setpin(pin, 1);
	else
	   s3c_gpio_setpin(pin, 0);
}

static void s3c_cs_config(int pin, int mode, int pull)
{
	s3c_gpio_cfgpin(pin, mode);

	if(pull == CS_HIGH)
	   s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);
	else
	   s3c_gpio_setpull(pin, S3C_GPIO_PULL_DOWN);
}
#endif

#if defined(CONFIG_SPI_CNTRLR_0)
static struct s3c_spi_pdata s3c_slv_pdata_0[] __initdata = {
	[0] = {	/* Slave-0 */
		.cs_level     = CS_FLOAT,
		.cs_pin       = S3C64XX_GPC(3),
		.cs_mode      = S3C64XX_GPC_OUTPUT(3),
		.cs_set       = s3c_cs_set,
		.cs_config    = s3c_cs_config,
		.cs_suspend   = s3c_cs_suspend,
		.cs_resume    = s3c_cs_resume,
	},
	[1] = {	/* Slave-1 */
		.cs_level     = CS_FLOAT,
		.cs_pin       = S3C64XX_GPF(13),
		.cs_mode      = S3C64XX_GPF_OUTPUT(13),
		.cs_set       = s3c_cs_setF13,
		.cs_config    = s3c_cs_configF13,
		.cs_suspend   = s3c_cs_suspend,
		.cs_resume    = s3c_cs_resume,
	},
};
#endif

#if defined(CONFIG_SPI_CNTRLR_1)
static struct s3c_spi_pdata s3c_slv_pdata_1[] __initdata = {
	[0] = {	/* Slave-0 */
		.cs_level     = CS_FLOAT,
		.cs_pin       = S3C64XX_GPC(7),
		.cs_mode      = S3C64XX_GPC_OUTPUT(7),
		.cs_set       = s3c_cs_set,
		.cs_config    = s3c_cs_config,
		.cs_suspend   = s3c_cs_suspend,
		.cs_resume    = s3c_cs_resume,
	},
	[1] = {	/* Slave-1 */
		.cs_level     = CS_FLOAT,
		.cs_pin       = S3C64XX_GPF(14),
		.cs_mode      = S3C64XX_GPF_OUTPUT(14),
		.cs_set       = s3c_cs_setF14,
		.cs_config    = s3c_cs_configF14,
		.cs_suspend   = s3c_cs_suspend,
		.cs_resume    = s3c_cs_resume,
	},
	[2] = {	/* Slave-2 */
		.cs_level     = CS_FLOAT,
		.cs_pin       = S3C64XX_GPF(15),
		.cs_mode      = S3C64XX_GPF_OUTPUT(15),
		.cs_set       = s3c_cs_setF15,
		.cs_config    = s3c_cs_configF15,
		.cs_suspend   = s3c_cs_suspend,
		.cs_resume    = s3c_cs_resume,
	},
};
#endif

static struct spi_board_info s3c_spi_devs[] __initdata = {
#if defined(CONFIG_SPI_CNTRLR_0)
	[0] = {
		.modalias	 = "spidev", /* Test Interface */
		.mode		 = SPI_MODE_0,	/* CPOL=0, CPHA=0 */
		.max_speed_hz    = 100000,
		/* Connected to SPI-0 as 1st Slave */
		.bus_num	 = 0,
		.irq		 = IRQ_SPI0,
		.chip_select	 = 0,
	},
	[1] = {
		.modalias	 = "spidev", /* Test Interface */
		.mode		 = SPI_MODE_0,	/* CPOL=0, CPHA=0 */
		.max_speed_hz    = 100000,
		/* Connected to SPI-0 as 2nd Slave */
		.bus_num	= 0,
		.irq		= IRQ_SPI0,
		.chip_select	= 1,
	},
#endif
#if defined(CONFIG_SPI_CNTRLR_1)
	[2] = {
		.modalias	 = "spidev", /* Test Interface */
		.mode		 = SPI_MODE_0,	/* CPOL=0, CPHA=0 */
		.max_speed_hz    = 100000,
		/* Connected to SPI-1 as 1st Slave */
		.bus_num	 = 1,
		.irq		 = IRQ_SPI1,
		.chip_select	 = 0,
	},
	[3] = {
		.modalias	 = "spidev", /* Test Interface */
		.mode		 = SPI_MODE_0,	/* CPOL=0, CPHA=0 */
		.max_speed_hz    = 100000,
		/* Connected to SPI-1 as 2nd Slave */
		.bus_num	= 1,
		.irq		= IRQ_SPI1,
		.chip_select	= 1,
	},
	[4] = {
		.modalias	 = "mmc_spi", /* MMC SPI */
		.mode		 = SPI_MODE_0 | SPI_CS_HIGH,	/* CPOL=0, CPHA=0 & CS is Active High */
		.max_speed_hz    = 100000,
		/* Connected to SPI-1 as 3rd Slave */
		.bus_num	= 1,
		.irq		= IRQ_SPI1,
		.chip_select	= 2,
	},
#endif
};

static struct s3c_uartcfg smdk6410_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon			= S3C_UCON_DEFAULT,
		.ulcon		= S3C_ULCON_DEFAULT,
		.ufcon		= S3C_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon			= S3C_UCON_DEFAULT,
		.ulcon		= S3C_ULCON_DEFAULT,
		.ufcon		= S3C_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon			= S3C_UCON_DEFAULT,
		.ulcon		= S3C_ULCON_DEFAULT,
		.ufcon		= S3C_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon			= S3C_UCON_DEFAULT,
		.ulcon		= S3C_ULCON_DEFAULT,
		.ufcon		= S3C_UFCON_DEFAULT,
	},
};

struct map_desc smdk6410_iodesc[] = {};

struct platform_device sec_device_backlight = {
	.name	= "smdk-backlight",
	.id		= -1,
};

struct platform_device sec_device_battery = {
	.name	= "smdk6410-battery",
	.id		= -1,
};

static struct s3c6410_pmem_setting pmem_setting = {
 	.pmem_start = RESERVED_PMEM_START,
	.pmem_size = RESERVED_PMEM,
	.pmem_gpu1_start = GPU1_RESERVED_PMEM_START,
	.pmem_gpu1_size = RESERVED_PMEM_GPU1,
	.pmem_render_start = RENDER_RESERVED_PMEM_START,
	.pmem_render_size = RESERVED_PMEM_RENDER,
	.pmem_stream_start = STREAM_RESERVED_PMEM_START,
	.pmem_stream_size = RESERVED_PMEM_STREAM,
	.pmem_preview_start = PREVIEW_RESERVED_PMEM_START,
	.pmem_preview_size = RESERVED_PMEM_PREVIEW,
	.pmem_picture_start = PICTURE_RESERVED_PMEM_START,
	.pmem_picture_size = RESERVED_PMEM_PICTURE,
	.pmem_jpeg_start = JPEG_RESERVED_PMEM_START,
	.pmem_jpeg_size = RESERVED_PMEM_JPEG,
        .pmem_skia_start = SKIA_RESERVED_PMEM_START,
        .pmem_skia_size = RESERVED_PMEM_SKIA,
};
#if 1 /* 2010-0208, added by CVKK(JC) */
struct platform_device smartq_gpio = {
     .name           = "smartq_gpio",
     .id             = -1,
     .num_resources  = 0,
};
#endif
static struct platform_device *smdk6410_devices[] __initdata = {
#if 1 /* 2010-0119, modified by CVKK(JC) , For SmartQ5 */
        &smartq_gpio,
	&s3c_device_hsmmc1, /* for inand */
	&s3c_device_hsmmc0, 
        &s3c_device_hsmmc2, /* for marvell 8686 wireless device */
#else /* official kernel */
#ifdef CONFIG_S3C6410_SD_CH0
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C6410_SD_CH1
	&s3c_device_hsmmc1,
#endif
#endif
#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
        &smartq_button_device,
        &smartq_pwr_button_device,
#endif     
#if defined(CONFIG_SND_S3C64XX_SOC_I2S)     
        &s3c64xx_device_iis0,
#endif     
	&s3c_device_i2c0,
	&s3c_device_i2c1,
#if defined(CONFIG_SPI_CNTRLR_0)
	&s3c_device_spi0,
#endif
#if defined(CONFIG_SPI_CNTRLR_1)
	&s3c_device_spi1,
#endif
	&s3c_device_ts,
//	&s3c_device_smc911x,
	&s3c_device_lcd,
	&s3c_device_nand,
	&s3c_device_keypad,
	&s3c_device_usb,
	&s3c_device_usbgadget,
#ifdef CONFIG_S3C_ADC
	&s3c_device_adc,
#endif
#ifdef CONFIG_RTC_DRV_S3C
	&s3c_device_rtc,
#endif
#ifdef CONFIG_VIDEO_G2D
	&s3c_device_g2d,
#endif
#ifdef CONFIG_VIDEO_FIMC
	&s3c_device_fimc0,
	&s3c_device_fimc1,
#endif
#if CONFIG_VIDEO_CAM 
	&s3c_device_camif,
#endif
#ifdef CONFIG_VIDEO_MFC
	&s3c_device_mfc,
#endif
#ifdef CONFIG_VIDEO_G3D
	&s3c_device_g3d,
#endif
#ifdef CONFIG_VIDEO_ROTATOR
	&s3c_device_rotator,
#endif
#ifdef CONFIG_VIDEO_JPEG
	&s3c_device_jpeg,
#endif
	&s3c_device_vpp,
#ifdef CONFIG_S3C6410_TVOUT
	&s3c_device_tvenc,
	&s3c_device_tvscaler,
#endif
	&sec_device_backlight,
	&sec_device_battery,
};

static struct i2c_board_info i2c_devs0[] __initdata = {
	{ I2C_BOARD_INFO("24c08", 0x50), },
/*	{ I2C_BOARD_INFO("WM8580", 0x1b), },	*/
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{ I2C_BOARD_INFO("24c128", 0x57), },	/* Samsung S524AD0XD1 */
	{ I2C_BOARD_INFO("WM8580", 0x1b), },
};

static struct s3c_ts_mach_info s3c_ts_platform __initdata = {
	.delay 			= 10000,
	.presc 			= 49,
	.oversampling_shift	= 2,
	.resol_bit 		= 12,
	.s3c_adc_con		= ADC_TYPE_2,
};
static struct s3c_adc_mach_info s3c_adc_platform __initdata= {
	/* Support 12-bit resolution */
	.delay	= 	0xff,
	.presc 	= 	49,
	.resolution = 	12,
};

static void s3c_smartq_poweroff(void)
{
   int dc_status = 0;
   
   dc_status = gpio_get_value(S3C64XX_GPL(13));
   printk(KERN_INFO"DC status [%d]\n",dc_status);
   
   /* disable speaker */
   gpio_set_value(S3C64XX_GPK(12), 0);
   if (dc_status)
     machine_restart(NULL);
   else {
      // clear the LCD screen to white      
      //s3cfb_clear_lcd(0);
      gpio_direction_output(S3C64XX_GPK(15), 1);
   }
}

static void __init smdk6410_map_io(void)
{
	s3c_device_nand.name = "s3c6410-nand";
#if 1 /* 2010-0126, added by CVKK(JC), For SmartQ5 */
        pm_power_off = s3c_smartq_poweroff;
#endif   
	s3c64xx_init_io(smdk6410_iodesc, ARRAY_SIZE(smdk6410_iodesc));
	s3c64xx_gpiolib_init();
	s3c_init_clocks(XTAL_FREQ);
	s3c_init_uarts(smdk6410_uartcfgs, ARRAY_SIZE(smdk6410_uartcfgs));
}
#if 0 /* 2010-0119, commend by CVKK(JC), For SmartQ5 */
static void __init smdk6410_smc911x_set(void)
{
	unsigned int tmp;

	tmp = __raw_readl(S3C64XX_SROM_BW);
	tmp &= ~(S3C64XX_SROM_BW_WAIT_ENABLE1_MASK | S3C64XX_SROM_BW_WAIT_ENABLE1_MASK |
		S3C64XX_SROM_BW_DATA_WIDTH1_MASK);
	tmp |= S3C64XX_SROM_BW_BYTE_ENABLE1_ENABLE | S3C64XX_SROM_BW_WAIT_ENABLE1_ENABLE |
		S3C64XX_SROM_BW_DATA_WIDTH1_16BIT;

	__raw_writel(tmp, S3C64XX_SROM_BW);

	__raw_writel(S3C64XX_SROM_BCn_TACS(0) | S3C64XX_SROM_BCn_TCOS(4) |
			S3C64XX_SROM_BCn_TACC(13) | S3C64XX_SROM_BCn_TCOH(1) |
			S3C64XX_SROM_BCn_TCAH(4) | S3C64XX_SROM_BCn_TACP(6) |
			S3C64XX_SROM_BCn_PMC_NORMAL, S3C64XX_SROM_BC1);
}
#endif
static void __init smdk6410_fixup (struct machine_desc *desc, struct tag *tags,
	      char **cmdline, struct meminfo *mi)
{
	/*
	 * Bank start addresses are not present in the information
	 * passed in from the boot loader.  We could potentially
	 * detect them, but instead we hard-code them.
	 */
	mi->nr_banks = 1;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = PHYS_UNRESERVED_SIZE;
	mi->bank[0].node = 0;
}

static void smdk6410_set_qos(void)
{
	u32 reg;

	/* AXI sfr */
	reg = (u32) ioremap((unsigned long) 0x7e003000, SZ_4K);

	/* QoS override: FIMD min. latency */
	writel(0x2, S3C_VA_SYS + 0x128);

	/* AXI QoS */
	writel(0x7, reg + 0x460);	/* (8 - MFC ch.) */
	writel(0x7ff7, reg + 0x464);

	/* Bus cacheable */
	writel(0x8ff, S3C_VA_SYS + 0x838);
}

static void __init smdk6410_machine_init(void)
{
#if 0 /* 2010-0120, commented by CVKK(JC) */   
	s3c_device_nand.dev.platform_data = &s3c_nand_mtd_part_info;

	smdk6410_smc911x_set();
#endif

	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);

	s3c_ts_set_platdata(&s3c_ts_platform);
	s3c_adc_set_platdata(&s3c_adc_platform);

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

#if defined(CONFIG_SPI_CNTRLR_0)
	s3c_spi_set_slaves(BUSNUM(0), ARRAY_SIZE(s3c_slv_pdata_0), s3c_slv_pdata_0);
#endif
#if defined(CONFIG_SPI_CNTRLR_1)
	s3c_spi_set_slaves(BUSNUM(1), ARRAY_SIZE(s3c_slv_pdata_1), s3c_slv_pdata_1);
#endif
	spi_register_board_info(s3c_spi_devs, ARRAY_SIZE(s3c_spi_devs));

	platform_add_devices(smdk6410_devices, ARRAY_SIZE(smdk6410_devices));
	s3c6410_add_mem_devices (&pmem_setting);

	s3c6410_pm_init();

	smdk6410_set_qos();
}

MACHINE_START(SMDK6410, "SMDK6410")
	/* Maintainer: Ben Dooks <ben@fluff.org> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C64XX_PA_SDRAM + 0x100,
	.fixup		= smdk6410_fixup,
	.init_irq	= s3c6410_init_irq,
	.map_io		= smdk6410_map_io,
	.init_machine	= smdk6410_machine_init,
#ifndef CONFIG_HIGH_RES_TIMERS
	.timer		= &s3c64xx_timer,
#else
	.timer		= &sec_timer,
#endif /* CONFIG_HIGH_RES_TIMERS */

MACHINE_END

#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
#if 1 /* 2010-0210, modified by CVKK(JC), For SmartQ5 */
void otg_phy_init(u32 otg_phy_clk)
{
	writel(readl(S3C_OTHERS)|S3C_OTHERS_USB_SIG_MASK, S3C_OTHERS);
	writel(0x0, S3C_USBOTG_PHYPWR);		/* Power up */
        writel(otg_phy_clk, S3C_USBOTG_PHYCLK);
	writel(0x1, S3C_USBOTG_RSTCON);
	udelay(50);
	writel(0x0, S3C_USBOTG_RSTCON);
	udelay(50);
}
#else
void otg_phy_init(void) {

	writel(readl(S3C_OTHERS)|S3C_OTHERS_USB_SIG_MASK, S3C_OTHERS);
	writel(0x0, S3C_USBOTG_PHYPWR);		/* Power up */
        writel(OTGH_PHY_CLK_VALUE, S3C_USBOTG_PHYCLK);
	writel(0x1, S3C_USBOTG_RSTCON);

	udelay(50);
	writel(0x0, S3C_USBOTG_RSTCON);
	udelay(50);
}
#endif
EXPORT_SYMBOL(otg_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
struct usb_ctrlrequest usb_ctrl __attribute__((aligned(8)));
EXPORT_SYMBOL(usb_ctrl);

/* OTG PHY Power Off */
void otg_phy_off(void) {
	writel(readl(S3C_USBOTG_PHYPWR)|(0x1F<<1), S3C_USBOTG_PHYPWR);
	writel(readl(S3C_OTHERS)&~S3C_OTHERS_USB_SIG_MASK, S3C_OTHERS);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_clk_en(void) {
	struct clk *otg_clk;

        switch (S3C_USB_CLKSRC) {
	case 0: /* epll clk */
		writel((readl(S3C_CLK_SRC)& ~S3C6400_CLKSRC_UHOST_MASK)
			|S3C_CLKSRC_EPLL_CLKSEL|S3C_CLKSRC_UHOST_EPLL,
			S3C_CLK_SRC);

		/* USB host colock divider ratio is 2 */
		writel((readl(S3C_CLK_DIV1)& ~S3C6400_CLKDIV1_UHOST_MASK)
			|(1<<20), S3C_CLK_DIV1);
		break;
	case 1: /* oscillator 48M clk */
		otg_clk = clk_get(NULL, "otg");
		clk_enable(otg_clk);
		writel(readl(S3C_CLK_SRC)& ~S3C6400_CLKSRC_UHOST_MASK, S3C_CLK_SRC);
//		otg_phy_init();
		otg_phy_init(OTGH_PHY_CLK_VALUE);

		/* USB host colock divider ratio is 1 */
		writel(readl(S3C_CLK_DIV1)& ~S3C6400_CLKDIV1_UHOST_MASK, S3C_CLK_DIV1);
		break;
	default:
		printk(KERN_INFO "Unknown USB Host Clock Source\n");
		BUG();
		break;
	}

	writel(readl(S3C_HCLK_GATE)|S3C_CLKCON_HCLK_UHOST|S3C_CLKCON_HCLK_SECUR,
		S3C_HCLK_GATE);
	writel(readl(S3C_SCLK_GATE)|S3C_CLKCON_SCLK_UHOST, S3C_SCLK_GATE);

}

EXPORT_SYMBOL(usb_host_clk_en);
#endif

#if defined(CONFIG_RTC_DRV_S3C)
/* RTC common Function for samsung APs*/
unsigned int s3c_rtc_set_bit_byte(void __iomem *base, uint offset, uint val)
{
	writeb(val, base + offset);

	return 0;
}

unsigned int s3c_rtc_read_alarm_status(void __iomem *base)
{
	return 1;
}

void s3c_rtc_set_pie(void __iomem *base, uint to)
{
	unsigned int tmp;

	tmp = readw(base + S3C_RTCCON) & ~S3C_RTCCON_TICEN;

        if (to)
                tmp |= S3C_RTCCON_TICEN;

        writew(tmp, base + S3C_RTCCON);
}

void s3c_rtc_set_freq_regs(void __iomem *base, uint freq, uint s3c_freq)
{
	unsigned int tmp;

        tmp = readw(base + S3C_RTCCON) & (S3C_RTCCON_TICEN | S3C_RTCCON_RTCEN );
        writew(tmp, base + S3C_RTCCON);
        s3c_freq = freq;
        tmp = (32768 / freq)-1;
        writel(tmp, base + S3C_TICNT);
}

void s3c_rtc_enable_set(struct platform_device *pdev,void __iomem *base, int en)
{
	unsigned int tmp;

	if (!en) {
		tmp = readw(base + S3C_RTCCON);
		writew(tmp & ~ (S3C_RTCCON_RTCEN | S3C_RTCCON_TICEN), base + S3C_RTCCON);
	} else {
		/* re-enable the device, and check it is ok */
		if ((readw(base+S3C_RTCCON) & S3C_RTCCON_RTCEN) == 0){
			dev_info(&pdev->dev, "rtc disabled, re-enabling\n");

			tmp = readw(base + S3C_RTCCON);
			writew(tmp|S3C_RTCCON_RTCEN, base+S3C_RTCCON);
		}

		if ((readw(base + S3C_RTCCON) & S3C_RTCCON_CNTSEL)){
			dev_info(&pdev->dev, "removing RTCCON_CNTSEL\n");

			tmp = readw(base + S3C_RTCCON);
			writew(tmp& ~S3C_RTCCON_CNTSEL, base+S3C_RTCCON);
		}

		if ((readw(base + S3C_RTCCON) & S3C_RTCCON_CLKRST)){
			dev_info(&pdev->dev, "removing RTCCON_CLKRST\n");

			tmp = readw(base + S3C_RTCCON);
			writew(tmp & ~S3C_RTCCON_CLKRST, base+S3C_RTCCON);
		}
	}
}
#endif

#if defined(CONFIG_KEYPAD_S3C) || defined (CONFIG_KEYPAD_S3C_MODULE)
void s3c_setup_keypad_cfg_gpio(int rows, int columns)
{
	unsigned int gpio;
	unsigned int end;

	end = S3C64XX_GPK(8 + rows);

	/* Set all the necessary GPK pins to special-function 0 */
	for (gpio = S3C64XX_GPK(8); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}

	end = S3C64XX_GPL(0 + columns);

	/* Set all the necessary GPL pins to special-function 0 */
	for (gpio = S3C64XX_GPL(0); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}
}

EXPORT_SYMBOL(s3c_setup_keypad_cfg_gpio);
#endif

void s3c_config_sleep_gpio(void)
{	
}
EXPORT_SYMBOL(s3c_config_sleep_gpio);

void s3c_config_wakeup_gpio(void)
{
}
EXPORT_SYMBOL(s3c_config_wakeup_gpio);

void s3c_config_wakeup_source(void)
{
	/* EINT10 */
	s3c_gpio_cfgpin(S3C64XX_GPN(10), S3C64XX_GPN10_EINT10);
	s3c_gpio_setpull(S3C64XX_GPN(10), S3C_GPIO_PULL_UP);

	udelay(50);

	__raw_writel((__raw_readl(S3C64XX_EINT0CON0) & ~(0x7 << 20)) |
		     (S3C64XX_EXTINT_BOTHEDGE << 20), S3C64XX_EINT0CON0);

	__raw_writel(1UL << (IRQ_EINT(10) - IRQ_EINT(0)), S3C64XX_EINT0PEND);
	__raw_writel(__raw_readl(S3C64XX_EINT0MASK)&~(1UL << (IRQ_EINT(10) - IRQ_EINT(0))), S3C64XX_EINT0MASK);

	__raw_writel((0x0fffffff&~(3<<9)), S3C_EINT_MASK);

	/* Alarm Wakeup Enable */
	__raw_writel((__raw_readl(S3C_PWR_CFG) & ~(0x1 << 10)), S3C_PWR_CFG);
}
EXPORT_SYMBOL(s3c_config_wakeup_source);
