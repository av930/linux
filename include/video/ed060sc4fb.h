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

	int gpio_ckv;
	int gpio_cl;
	int gpio_gmode;
	int gpio_gpv;
	int gpio_oe;
	int gpio_le;	
	int gpio_sph;
	int gpio_data[8];

	int gpio_vdd;
	int gpio_vpos;
	int gpio_vneg;
}

#endif
