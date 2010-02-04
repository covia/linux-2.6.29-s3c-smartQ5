/* sound/soc/s3c24xx/smartq_wm8987.c
 *
 * Copyright 2009 Maurus Cuelenaere <mcuelenaere@gmail.com>
 *
 * Based on smdk6410_wm8987.c
 *     Copyright 2007 Wolfson Microelectronics PLC. - linux@wolfsonmicro.com
 *     Graeme Gregory - graeme.gregory@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  ChangeLog
 *    o 0.1 2010-0204, Jackal Chan <jackal.cvkk@gmail.com>
 *          - porting to 2.6.29 kernel for SmartQ5 MID
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>
#include <asm/io.h>

#include <mach/gpio.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>

#include "../codecs/wm8987.h"

#include "s3c24xx-pcm.h"
#include "s3c64xx-i2s.h"

static struct snd_soc_card snd_soc_smartq;

static int smartq_hifi_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
   struct snd_soc_pcm_runtime *rtd = substream->private_data;
   struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
   struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
   struct s3c_i2sv2_rate_calc div;
   unsigned int clk = 0;
   int ret;
   
   s3c_i2sv2_iis_calc_rate(&div, NULL, params_rate(params),
			   s3c64xx_i2s_get_clock(cpu_dai));

   switch (params_rate(params)) {
    case 8000:
    case 16000:
    case 32000:
    case 96000:
      clk = 12288000;
      break;
    case 11025:
    case 22050:
    case 44100:
      clk = 16934400;
      break;
    case 48000:
      clk = 18432000;
      break;
   }

   /* set codec DAI configuration */
   ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			     SND_SOC_DAIFMT_NB_NF |
			     SND_SOC_DAIFMT_CBS_CFS);
   if (ret < 0) return ret;
   
   /* set cpu DAI configuration */
   ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			     SND_SOC_DAIFMT_NB_NF |
			     SND_SOC_DAIFMT_CBS_CFS);
   if (ret < 0) return ret;
   
   /* set the codec system clock for DAC and ADC */
   ret = snd_soc_dai_set_sysclk(codec_dai, WM8987_SYSCLK, clk,
				SND_SOC_CLOCK_IN);
   if (ret < 0)	return ret;

   /* set MCLK division for sample rate */
   ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK,
				div.fs_div);
   if (ret < 0)	return ret;

   /* set prescaler division for sample rate */
   ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_PRESCALER,
				div.clk_div - 1);
   if (ret < 0) return ret;

   return 0;
}

/*
 * SmartQ WM8987 HiFi DAI operations.
 */
static struct snd_soc_ops smartq_hifi_ops = {
	.hw_params = smartq_hifi_hw_params,
};

static const struct snd_soc_dapm_route audio_map[] = {
	{"Headphone Jack", NULL, "LOUT2"},
	{"Headphone Jack", NULL, "ROUT2"},

	{"Ext Spk", NULL, "LOUT2"},
	{"Ext Spk", NULL, "ROUT2"},

	{"LINPUT2", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Int Mic"},
};

static int smartq_wm8987_init(struct snd_soc_codec *codec)
{
	int err = 0;
   
	/* set up smartq specific audio path audio_mapnects */
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	/* set endpoints to default off mode */
	snd_soc_dapm_enable_pin(codec, "Ext Spk");
	snd_soc_dapm_enable_pin(codec, "Int Mic");
	snd_soc_dapm_disable_pin(codec, "Headphone Jack");
        /* not connection */
	snd_soc_dapm_nc_pin(codec, "LINPUT1");
	snd_soc_dapm_nc_pin(codec, "RINPUT1");
	snd_soc_dapm_nc_pin(codec, "RINPUT2");
	snd_soc_dapm_nc_pin(codec, "ROUT1");
	snd_soc_dapm_nc_pin(codec, "OUT3");

	err = snd_soc_dapm_sync(codec);
	if (err) return err;
   
	return err;
}

static struct snd_soc_dai_link smartq_dai[] = {
	{
		.name		= "WM8987",
		.stream_name	= "WM8987 HiFi",
		.cpu_dai	= &s3c64xx_i2s_dai[0],
		.codec_dai	= &wm8987_dai,
		.init		= smartq_wm8987_init,
		.ops		= &smartq_hifi_ops,
	},
};

static struct snd_soc_card snd_soc_smartq = {
	.name = "smartq",
	.platform = &s3c24xx_soc_platform,
	.dai_link = smartq_dai,
	.num_links = ARRAY_SIZE(smartq_dai),
};

static struct wm8987_setup_data smartq_wm8987_setup = {
	.i2c_address = 0x1a,
};

static struct snd_soc_device smartq_snd_devdata = {
	.card = &snd_soc_smartq,
	.codec_dev = &soc_codec_dev_wm8987,
	.codec_data = &smartq_wm8987_setup,
};

static struct platform_device *smartq_snd_device;

static int __init smartq_init(void)
{
   int ret;
   
   /* For I2C initial */
   s3c_gpio_setpull(S3C64XX_GPB(5), S3C_GPIO_PULL_NONE);
   s3c_gpio_setpull(S3C64XX_GPB(6), S3C_GPIO_PULL_NONE);
   
   s3c_gpio_cfgpin(S3C64XX_GPB(5), S3C64XX_GPB5_I2C_SCL0);
   s3c_gpio_cfgpin(S3C64XX_GPB(6), S3C64XX_GPB6_I2C_SDA0);
   
   smartq_snd_device = platform_device_alloc("soc-audio", -1);
   if (!smartq_snd_device)
     return -ENOMEM;

   platform_set_drvdata(smartq_snd_device, &smartq_snd_devdata);
   smartq_snd_devdata.dev = &smartq_snd_device->dev;
   ret = platform_device_add(smartq_snd_device);

   if (ret) {
      platform_device_put(smartq_snd_device);
      return ret;
   }

   return 0;
}

static void __exit smartq_exit(void)
{
	platform_device_unregister(smartq_snd_device);
}

module_init(smartq_init);
module_exit(smartq_exit);

/* Module information */
MODULE_AUTHOR("Maurus Cuelenaere");
MODULE_DESCRIPTION("ALSA SoC SmartQ WM8987");
MODULE_LICENSE("GPL");
