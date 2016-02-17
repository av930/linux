/*
 * ED060SC4 panel framebuffer driver
 *
 * Copyright (C) 2016 Homin Lee <homin.lee@suapapa.net>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/uaccess.h>

#include <video/ed060sc4fb.h>

/* Display specific information */
#define DPY_W 600
#define DPY_H 800

static struct fb_fix_screeninfo ed060sc4fb_fix = {
	.id =		"ed060sc4fb",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_MONO01,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0,
	.line_length =	DPY_W,
	.accel =	FB_ACCEL_NONE,
};

static struct fb_var_screeninfo ed060sc4fb_var = {
	.xres		= DPY_W,
	.yres		= DPY_H,
	.xres_virtual	= DPY_W,
	.yres_virtual	= DPY_H,
	.bits_per_pixel	= 1,
	.nonstd		= 1,
};

/* main ed060sc4fb functions */

static void apollo_send_data(struct ed060sc4fb_par *par, unsigned char data)
{
	/* set data */
	par->board->set_data(par, data);

	/* set DS low */
	par->board->set_ctl(par, HCB_DS_BIT, 0);

	/* wait for ack */
	par->board->wait_for_ack(par, 0);

	/* set DS hi */
	par->board->set_ctl(par, HCB_DS_BIT, 1);

	/* wait for ack to clear */
	par->board->wait_for_ack(par, 1);
}

static void apollo_send_command(struct ed060sc4fb_par *par, unsigned char data)
{
	/* command so set CD to high */
	par->board->set_ctl(par, HCB_CD_BIT, 1);

	/* actually strobe with command */
	apollo_send_data(par, data);

	/* clear CD back to low */
	par->board->set_ctl(par, HCB_CD_BIT, 0);
}

static void ed060sc4fb_dpy_update(struct ed060sc4fb_par *par)
{
	int i;
	unsigned char *buf = (unsigned char __force *)par->info->screen_base;

	apollo_send_command(par, APOLLO_START_NEW_IMG);

	for (i=0; i < (DPY_W*DPY_H/8); i++) {
		apollo_send_data(par, *(buf++));
	}

	apollo_send_command(par, APOLLO_STOP_IMG_DATA);
	apollo_send_command(par, APOLLO_DISPLAY_IMG);
}

/* this is called back from the deferred io workqueue */
static void ed060sc4fb_dpy_deferred_io(struct fb_info *info,
				struct list_head *pagelist)
{
	ed060sc4fb_dpy_update(info->par);
}

static void ed060sc4fb_fillrect(struct fb_info *info,
				   const struct fb_fillrect *rect)
{
	struct ed060sc4fb_par *par = info->par;

	sys_fillrect(info, rect);

	ed060sc4fb_dpy_update(par);
}

static void ed060sc4fb_copyarea(struct fb_info *info,
				   const struct fb_copyarea *area)
{
	struct ed060sc4fb_par *par = info->par;

	sys_copyarea(info, area);

	ed060sc4fb_dpy_update(par);
}

static void ed060sc4fb_imageblit(struct fb_info *info,
				const struct fb_image *image)
{
	struct ed060sc4fb_par *par = info->par;

	sys_imageblit(info, image);

	ed060sc4fb_dpy_update(par);
}

/*
 * this is the slow path from userspace. they can seek and write to
 * the fb. it's inefficient to do anything less than a full screen draw
 */
static ssize_t ed060sc4fb_write(struct fb_info *info, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct ed060sc4fb_par *par = info->par;
	unsigned long p = *ppos;
	void *dst;
	int err = 0;
	unsigned long total_size;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	total_size = info->fix.smem_len;

	if (p > total_size)
		return -EFBIG;

	if (count > total_size) {
		err = -EFBIG;
		count = total_size;
	}

	if (count + p > total_size) {
		if (!err)
			err = -ENOSPC;

		count = total_size - p;
	}

	dst = (void __force *) (info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if  (!err)
		*ppos += count;

	ed060sc4fb_dpy_update(par);

	return (err) ? err : count;
}

static struct fb_ops ed060sc4fb_ops = {
	.owner		= THIS_MODULE,
	.fb_read        = fb_sys_read,
	.fb_write	= ed060sc4fb_write,
	.fb_fillrect	= ed060sc4fb_fillrect,
	.fb_copyarea	= ed060sc4fb_copyarea,
	.fb_imageblit	= ed060sc4fb_imageblit,
};

static struct fb_deferred_io ed060sc4fb_defio = {
	.delay		= HZ,
	.deferred_io	= ed060sc4fb_dpy_deferred_io,
};

static int ed060sc4fb_probe(struct platform_device *dev)
{
	struct fb_info *info;
	struct ed060sc4_board *board;
	int retval = -ENOMEM;
	int videomemorysize;
	unsigned char *videomemory;
	struct ed060sc4fb_par *par;

	/* pick up board specific routines */
	board = dev->dev.platform_data;
	if (!board)
		return -EINVAL;

	/* try to count device specific driver, if can't, platform recalls */
	if (!try_module_get(board->owner))
		return -ENODEV;

	videomemorysize = (DPY_W*DPY_H)/8;

	videomemory = vzalloc(videomemorysize);
	if (!videomemory)
		goto err_videomem_alloc;

	info = framebuffer_alloc(sizeof(struct ed060sc4fb_par), &dev->dev);
	if (!info)
		goto err_fballoc;

	info->screen_base = (char __force __iomem *)videomemory;
	info->fbops = &ed060sc4fb_ops;

	info->var = ed060sc4fb_var;
	info->fix = ed060sc4fb_fix;
	info->fix.smem_len = videomemorysize;
	par = info->par;
	par->info = info;
	par->board = board;
	par->send_command = apollo_send_command;
	par->send_data = apollo_send_data;

	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;

	info->fbdefio = &ed060sc4fb_defio;
	fb_deferred_io_init(info);

	retval = register_framebuffer(info);
	if (retval < 0)
		goto err_fbreg;
	platform_set_drvdata(dev, info);

	fb_info(info, "Hecuba frame buffer device, using %dK of video memory\n",
		videomemorysize >> 10);

	/* this inits the dpy */
	retval = par->board->init(par);
	if (retval < 0)
		goto err_fbreg;

	return 0;
err_fbreg:
	framebuffer_release(info);
err_fballoc:
	vfree(videomemory);
err_videomem_alloc:
	module_put(board->owner);
	return retval;
}

static int ed060sc4fb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		struct ed060sc4fb_par *par = info->par;
		fb_deferred_io_cleanup(info);
		unregister_framebuffer(info);
		vfree((void __force *)info->screen_base);
		if (par->board->remove)
			par->board->remove(par);
		module_put(par->board->owner);
		framebuffer_release(info);
	}
	return 0;
}

static struct platform_driver ed060sc4fb_driver = {
	.probe	= ed060sc4fb_probe,
	.remove = ed060sc4fb_remove,
	.driver	= {
		.name	= "ed060sc4fb",
	},
};
module_platform_driver(ed060sc4fb_driver);

MODULE_DESCRIPTION("fbdev driver for ED060SC4 panel");
MODULE_AUTHOR("Homin Lee");
MODULE_LICENSE("GPL");
