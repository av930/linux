struct ed060sc4_par {
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


static void ed060sc4_vclk(struct ed060sc4_par *par)
{
	gpio_set_value(par->gpio_ckv, 1);
	udelay(1)
	gpio_set_value(par->gpio_ckv, 0);
	udelay(4)
}

static void ed060sc4_hclk(struct ed060sc4_par *par)
{
	ndelay(40);
	gpio_set_value(par->gpio_cl, 1);
	ndelay(40);
	gpio_set_value(par->gpio_cl, 0);
}

static void ed060sc4_vscan_start(struct ed060sc4_par *par)
{
	gpio_set_value(par->gpio_gmode, 1);
	ed060sc4_vclk(par);
	gpio_set_value(par->gpio_spv, 0);
	ed060sc4_vclk(par);
	gpio_set_value(par->gpio_spv, 1);
	ed060sc4_vclk(par);
}

static void ed060sc4_vscan_write(struct ed060sc4_par *par)
{
	gpio_set_value(par->gpio_ckv, 1);
	gpio_set_value(par->gpio_oe, 1);
	udelay(5);
	gpio_set_value(par->gpio_oe, 0);
	gpio_set_value(par->gpio_ckv, 0);
	udelay(200);
}

static void ed060sc4_vscan_bulkwrite(struct ed060sc4_par *par)
{
	gpio_set_value(par->gpio_ckv, 1);
	udelay(20);
	gpio_set_value(par->gpio_ckv, 0);
	udelay(200);
}

static void ed060sc4_vscan_skip(struct ed060sc4_par *par)
{
	gpio_set_value(par->gpio_ckv, 1);
	udelay(1);
	gpio_set_value(par->gpio_ckv, 0);
	udelay(100);
}

static void ed060sc4_vscan_stop(struct ed060sc4_par *par)
{
	gpio_set_value(par->gpio_gmode, 0);
	ed060sc4_vclk(par);
	ed060sc4_vclk(par);
	ed060sc4_vclk(par);
	ed060sc4_vclk(par);
	ed060sc4_vclk(par);
}

static void ed060sc4_hscan_start(struct ed060sc4_par *par)
{
	gpio_set_value(par->gpio_le, 0);
	gpio_set_value(par->gpio_oe, 0);

	gpio_set_value(par->gpio_sph, 0);
}

static void ed060sc4_hscan_write(struct ed060sc4_par *par,
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

static void ed060sc4_hscan_stop(struct ed060sc4_par *par)
{
	gpio_set_value(par->gpio_sph, 1);
	ed060sc4_hclk(par);

	gpio_set_value(par->gpio_le, 1);
	ndelay(40);
	gpio_set_value(par->gpio_le, 0);
}

static void ed060sc4_power_on(struct ed060sc4_par *par)
{
	int i;

	gpio_set_value(par->gpio_vdd, 1);

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


static void ed060sc4_power_off(struct ed060sc4_par *par)
{
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

	gpio_set_value(par->gpio_vdd, 0);
}


