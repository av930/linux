/*
 * ed060sc4fb.h - definitions for the ed060sc4 framebuffer driver
 *
 * Copyright (C) 2016 by Homin Lee <homin.lee@suapapa.net>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _LINUX_ED060SC4FB_H_
#define _LINUX_ED060SC4FB_H_

/* struct used by ed060sc4 */

struct ed060sc4fb_par {
	struct fb_info *info;

	int gpio_ckv;	/* gpio05 */
	int gpio_cl;	/* gpio04 */
	int gpio_gmode;	/* gpio11 */
	int gpio_oe;	/* gpio08 */
	int gpio_le;	/* gpio07 */
	int gpio_sph;	/* gpio10 */
	int gpio_spv;	/* gpio13 */
	int gpio_data[8];	/* gpio20 ~ gpio27 */

	int gpio_vdd3;	/* gpio19 */
	int gpio_vdd5;	/* gpio18 */
	int gpio_vpos;	/* gpio16 */
	int gpio_vneg;	/* gpio17 */
};

#endif
