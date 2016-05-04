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
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <video/ed060sc4fb.h>

/* Display specific information */
#define DPY_W 800
#define DPY_H 600

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

static void ed060sc4_vclk(struct ed060sc4fb_par *par)
{
	gpio_set_value(par->gpio_ckv, 1);
	udelay(1);
	gpio_set_value(par->gpio_ckv, 0);
	udelay(4);
}

static void ed060sc4_hclk(struct ed060sc4fb_par *par)
{
	ndelay(40);
	gpio_set_value(par->gpio_cl, 1);
	ndelay(40);
	gpio_set_value(par->gpio_cl, 0);
}

static void ed060sc4_vscan_start(struct ed060sc4fb_par *par)
{
	gpio_set_value(par->gpio_gmode, 1);
	ed060sc4_vclk(par);
	gpio_set_value(par->gpio_spv, 0);
	ed060sc4_vclk(par);
	gpio_set_value(par->gpio_spv, 1);
	ed060sc4_vclk(par);
}

#if 0
static void ed060sc4_vscan_write(struct ed060sc4fb_par *par)
{
	gpio_set_value(par->gpio_ckv, 1);
	gpio_set_value(par->gpio_oe, 1);
	udelay(5);
	gpio_set_value(par->gpio_oe, 0);
	gpio_set_value(par->gpio_ckv, 0);
	udelay(200);
}

static void ed060sc4_vscan_bulkwrite(struct ed060sc4fb_par *par)
{
	gpio_set_value(par->gpio_ckv, 1);
	udelay(20);
	gpio_set_value(par->gpio_ckv, 0);
	udelay(200);
}

static void ed060sc4_vscan_skip(struct ed060sc4fb_par *par)
{
	gpio_set_value(par->gpio_ckv, 1);
	udelay(1);
	gpio_set_value(par->gpio_ckv, 0);
	udelay(100);
}
#endif

static void ed060sc4_vscan_stop(struct ed060sc4fb_par *par)
{
	gpio_set_value(par->gpio_gmode, 0);
	ed060sc4_vclk(par);
	ed060sc4_vclk(par);
	ed060sc4_vclk(par);
	ed060sc4_vclk(par);
	ed060sc4_vclk(par);
}

static void ed060sc4_hscan_start(struct ed060sc4fb_par *par)
{
	gpio_set_value(par->gpio_le, 0);
	gpio_set_value(par->gpio_oe, 0);

	gpio_set_value(par->gpio_sph, 0);
}

#if 0
static void ed060sc4_hscan_write(struct ed060sc4fb_par *par,
				 const uint8_t *data, int count)
{
	int i;
	uint8_t d;

	while (count--) {
		d = *data++;
		for(i = 0; i < 8; i++) {
			gpio_set_value(par->gpio_data[i], (d & (1<<i)) ? 1 : 0);
		}
		ed060sc4_hclk(par);
	}
}
#endif

static void ed060sc4_hscan_stop(struct ed060sc4fb_par *par)
{
	gpio_set_value(par->gpio_sph, 1);
	ed060sc4_hclk(par);

	gpio_set_value(par->gpio_le, 1);
	ndelay(40);
	gpio_set_value(par->gpio_le, 0);
}

static void ed060sc4_power_on(struct ed060sc4fb_par *par)
{
	int i;

	gpio_set_value(par->gpio_vdd5, 1);
	gpio_set_value(par->gpio_vdd3, 1);

	gpio_set_value(par->gpio_le, 0);
	gpio_set_value(par->gpio_oe, 0);
	gpio_set_value(par->gpio_cl, 0);
	gpio_set_value(par->gpio_sph, 1);

	for (i = 0; i < 8; i++) {
		gpio_set_value(par->gpio_data[i], 0);
	}

	gpio_set_value(par->gpio_ckv, 0);
	gpio_set_value(par->gpio_gmode, 0);
	gpio_set_value(par->gpio_spv, 1);

	msleep(100);

	gpio_set_value(par->gpio_vneg, 1);
	msleep(1000);
	gpio_set_value(par->gpio_vpos, 1);

	ed060sc4_vscan_start(par);
	for (i = 0; i < 800; i++)
		ed060sc4_vclk(par);

	ed060sc4_vscan_stop(par);
}


static void ed060sc4_power_off(struct ed060sc4fb_par *par)
{
	int i;

	gpio_set_value(par->gpio_vpos, 0);
	gpio_set_value(par->gpio_vneg, 0);

	msleep(100);

	gpio_set_value(par->gpio_le, 0);
	gpio_set_value(par->gpio_oe, 0);
	gpio_set_value(par->gpio_cl, 0);
	gpio_set_value(par->gpio_sph, 0);

	for (i = 0; i < 8; i++) {
		gpio_set_value(par->gpio_data[i], 0);
	}

	gpio_set_value(par->gpio_ckv, 0);
	gpio_set_value(par->gpio_gmode, 0);
	gpio_set_value(par->gpio_spv, 0);

	gpio_set_value(par->gpio_vdd3, 0);
	gpio_set_value(par->gpio_vdd5, 0);
}

static void ed060sc4fb_dpy_update(struct ed060sc4fb_par *par)
{
	int i, x, y;
	unsigned char *buf = (unsigned char __force *)par->info->screen_base;
	unsigned char data, pixel, gpio1, gpio2;

	ed060sc4_vscan_start(par);
	for (y = 0; y < DPY_H; y++) {
		ed060sc4_hscan_start(par);
		for (x = 0; x < DPY_W/8; x++) {
			data = *(buf++);
			for (i = 0; i < 8; i++){
				pixel = data & (1 << (8 - i + 1));
				gpio1 = par->gpio_data[(i % 4) * 2];
				gpio2 = par->gpio_data[(i % 4) * 2 + 1];
				if (pixel) {
					gpio_set_value(gpio1, 1);
					gpio_set_value(gpio2, 0);
				} else {
					gpio_set_value(gpio1, 0);
					gpio_set_value(gpio2, 0);
				}
				if (i == 3 || i == 7) {
					ed060sc4_hclk(par);
				}
			}
		}
		ed060sc4_hscan_stop(par);

		ed060sc4_vclk(par);
	}
	ed060sc4_vscan_stop(par);
}

/* this is called back from the deferred io workqueue */
static void ed060sc4fb_dpy_deferred_io(struct fb_info *info,
				struct list_head *pagelist)
{
	printk("#### %s\n", __func__);

	ed060sc4fb_dpy_update(info->par);
}

static void ed060sc4fb_fillrect(struct fb_info *info,
				   const struct fb_fillrect *rect)
{
	struct ed060sc4fb_par *par = info->par;

	printk("#### %s\n", __func__);

	sys_fillrect(info, rect);

	ed060sc4fb_dpy_update(par);
}

static void ed060sc4fb_copyarea(struct fb_info *info,
				   const struct fb_copyarea *area)
{
	struct ed060sc4fb_par *par = info->par;

	printk("#### %s\n", __func__);

	sys_copyarea(info, area);

	ed060sc4fb_dpy_update(par);
}

static void ed060sc4fb_imageblit(struct fb_info *info,
				const struct fb_image *image)
{
	struct ed060sc4fb_par *par = info->par;

	printk("#### %s\n", __func__);

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

	printk("#### %s\n", __func__);

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

	printk("#### err=%d, count=%d\n", err, count);

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

#if CONFIG_OF
int ed060sc4_of_init(struct ed060sc4fb_par *par)
{
	/* struct fb_info *info = par->info; */
	struct device *dev = &par->pdev->dev;
	struct device_node *np = dev->of_node;
	int i;
	int ret = -ENOENT;

	par->gpio_ckv = of_get_named_gpio(np, "gpio-ckv", 0);
	if (!gpio_is_valid(par->gpio_ckv)){
		ret = -EINVAL;
		goto exit;
	}
	par->gpio_cl = of_get_named_gpio(np, "gpio-cl", 0);
	if (!gpio_is_valid(par->gpio_cl)){
		ret = -EINVAL;
		goto exit;
	}
	par->gpio_gmode = of_get_named_gpio(np, "gpio-gmode", 0);
	if (!gpio_is_valid(par->gpio_gmode)){
		ret = -EINVAL;
		goto exit;
	}
	par->gpio_oe = of_get_named_gpio(np, "gpio-oe", 0);
	if (!gpio_is_valid(par->gpio_oe)){
		ret = -EINVAL;
		goto exit;
	}
	par->gpio_le = of_get_named_gpio(np, "gpio-le", 0);
	if (!gpio_is_valid(par->gpio_le)){
		ret = -EINVAL;
		goto exit;
	}
	par->gpio_sph = of_get_named_gpio(np, "gpio-sph", 0);
	if (!gpio_is_valid(par->gpio_sph)){
		ret = -EINVAL;
		goto exit;
	}
	par->gpio_spv = of_get_named_gpio(np, "gpio-spv", 0);
	if (!gpio_is_valid(par->gpio_spv)){
		ret = -EINVAL;
		goto exit;
	}

	for (i = 0; i < 8; i++) {
		par->gpio_data[i] = of_get_named_gpio(np, "gpio-data", i);
		if (!gpio_is_valid(par->gpio_data[i])){
			ret = -EINVAL;
			goto exit;
		}
	}

	par->gpio_vdd3 = of_get_named_gpio(np, "gpio-vdd3", 0);
	if (!gpio_is_valid(par->gpio_vdd3)){
		ret = -EINVAL;
		goto exit;
	}
	par->gpio_vdd5 = of_get_named_gpio(np, "gpio-vdd5", 0);
	if (!gpio_is_valid(par->gpio_vdd5)){
		ret = -EINVAL;
		goto exit;
	}
	par->gpio_vpos = of_get_named_gpio(np, "gpio-vpos", 0);
	if (!gpio_is_valid(par->gpio_vpos)){
		ret = -EINVAL;
		goto exit;
	}
	par->gpio_vneg = of_get_named_gpio(np, "gpio-vneg", 0);
	if (!gpio_is_valid(par->gpio_vneg)){
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}
#endif

static int ed060sc4fb_probe(struct platform_device *dev)
{
	struct fb_info *info;
	int retval = -ENOMEM;
	int videomemorysize;
	unsigned char *videomemory;
	struct ed060sc4fb_par *par;

	pr_err("ED060SC4 probe start\n");
#if 0
	/* pick up board specific routines */
	board = dev->dev.platform_data;
	if (!board)
		return -EINVAL;

	/* try to count device specific driver, if can't, platform recalls */
	if (!try_module_get(board->owner))
		return -ENODEV;
#endif

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
	par->pdev = dev;
	/* TODO: fill par->gpios */

#if 1
	par->gpio_ckv = 5;
	par->gpio_cl = 4;
	par->gpio_gmode = 11;
	par->gpio_oe = 8;
	par->gpio_le = 7;
	par->gpio_sph = 10;
	par->gpio_spv = 13;

	par->gpio_data[0] = 20;
	par->gpio_data[1] = 21;
	par->gpio_data[2] = 22;
	par->gpio_data[3] = 23;
	par->gpio_data[4] = 24;
	par->gpio_data[5] = 25;
	par->gpio_data[6] = 26;
	par->gpio_data[7] = 27;

	par->gpio_vdd3 = 19;
	par->gpio_vdd5 = 18;
	par->gpio_vpos = 16;
	par->gpio_vneg = 17;
#else
#if CONFIG_OF
	retval = ed060sc4_of_init(par);
	if (retval < 0)
		goto err_fbreg;
#endif
#endif

	ed060sc4_power_on(par);

#if 0
	par->board = board;
#endif

	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;

	info->fbdefio = &ed060sc4fb_defio;
	fb_deferred_io_init(info);

	retval = register_framebuffer(info);
	if (retval < 0)
		goto err_fbreg;
	platform_set_drvdata(dev, info);

	fb_info(info, "ED060SC4 frame buffer device, using %dK of video memory\n",
		videomemorysize >> 10);

#if 0
	/* this inits the dpy */
	retval = par->board->init(par);
	if (retval < 0)
		goto err_fbreg;
#endif

	return 0;
err_fbreg:
	pr_err("ED060SC4 init fail of err_fbreg\n");
	framebuffer_release(info);
err_fballoc:
	pr_err("ED060SC4 init fail of err_fballoc\n");
	vfree(videomemory);
err_videomem_alloc:
	pr_err("ED060SC4 init fail of err_videomem_alloc\n");
#if 0
	module_put(board->owner);
#endif
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
#if 0
		if (par->board->remove)
			par->board->remove(par);
		module_put(par->board->owner);
#endif
		framebuffer_release(info);

		ed060sc4_power_off(par);
	}
	return 0;
}

#if CONFIG_OF
static const struct of_device_id ed060sc4fb_dt_ids[] = {
	{ .compatible = "primeview,ed060sc4", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ed060sc4fb_dt_ids);
#endif

static struct platform_driver ed060sc4fb_driver = {
	.probe	= ed060sc4fb_probe,
	.remove = ed060sc4fb_remove,
	.driver	= {
		.name = "ed060sc4fb",
#if 1
		.of_match_table = ed060sc4fb_dt_ids,
#endif
	},
};

module_platform_driver(ed060sc4fb_driver);

MODULE_DESCRIPTION("fbdev driver for ED060SC4 panel");
MODULE_AUTHOR("Homin Lee");
MODULE_LICENSE("GPL");
