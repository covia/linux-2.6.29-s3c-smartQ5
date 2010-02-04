/*
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@openedhand.com>
 *
 * Based on WM8753.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _WM8987_H
#define _WM8987_H

/* WM8987 register space */

#define WM8987_LINVOL    0x00
#define WM8987_RINVOL    0x01
#define WM8987_LOUT1V    0x02
#define WM8987_ROUT1V    0x03
#define WM8987_ADCDAC    0x05
#define WM8987_IFACE     0x07
#define WM8987_SRATE     0x08
#define WM8987_LDAC      0x0a
#define WM8987_RDAC      0x0b
#define WM8987_BASS      0x0c
#define WM8987_TREBLE    0x0d
#define WM8987_RESET     0x0f
#define WM8987_3D        0x10
#define WM8987_ALC1      0x11
#define WM8987_ALC2      0x12
#define WM8987_ALC3      0x13
#define WM8987_NGATE     0x14
#define WM8987_LADC      0x15
#define WM8987_RADC      0x16
#define WM8987_ADCTL1    0x17
#define WM8987_ADCTL2    0x18
#define WM8987_PWR1      0x19
#define WM8987_PWR2      0x1a
#define WM8987_ADCTL3    0x1b
#define WM8987_ADCIN     0x1f
#define WM8987_LADCIN    0x20
#define WM8987_RADCIN    0x21
#define WM8987_LOUTM1    0x22
#define WM8987_LOUTM2    0x23
#define WM8987_ROUTM1    0x24
#define WM8987_ROUTM2    0x25
#define WM8987_MOUTM1    0x26
#define WM8987_MOUTM2    0x27
#define WM8987_LOUT2V    0x28
#define WM8987_ROUT2V    0x29
#define WM8987_MOUTV     0x2a

#define WM8987_CACHE_REGNUM 0x2a

#define WM8987_SYSCLK	0

struct wm8987_setup_data {
	int spi;
	int i2c_bus;
	unsigned short i2c_address;
};

extern struct snd_soc_dai wm8987_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8987;

#endif
