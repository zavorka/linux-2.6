/*
 *  linux/arch/arm/mach-pxa/z2.c
 *
 *  Support for the Zipit Z2 Handheld device.
 *
 *  Copyright (C) 2009-2010 Marek Vasut <marek.vasut@gmail.com>
 *  Copyright (C) 2016 Vasily Khoruzhick <anarsoul@gmail.com>
 *
 *  Based on research and code by: Ken McGuire
 *  Based on mainstone.c as modified for the Zipit Z2.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>
#include <linux/spi/pxa2xx_spi.h>
#include <linux/spi/libertas_spi.h>
#include <linux/spi/lms283gf05.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#include <linux/i2c/pxa-i2c.h>
#include <linux/of_platform.h>
#include <linux/dma/pxa-dma.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/pxa27x.h>
#include <mach/mfp-pxa27x.h>
#include <mach/z2.h>
#include <linux/platform_data/video-pxafb.h>
#include <mach/pm.h>

#include "generic.h"
#include "devices.h"

/******************************************************************************
 * Pin configuration
 ******************************************************************************/
static unsigned long z2_pin_config[] = {

	/* LCD - 16bpp Active TFT */
	GPIO58_LCD_LDD_0,
	GPIO59_LCD_LDD_1,
	GPIO60_LCD_LDD_2,
	GPIO61_LCD_LDD_3,
	GPIO62_LCD_LDD_4,
	GPIO63_LCD_LDD_5,
	GPIO64_LCD_LDD_6,
	GPIO65_LCD_LDD_7,
	GPIO66_LCD_LDD_8,
	GPIO67_LCD_LDD_9,
	GPIO68_LCD_LDD_10,
	GPIO69_LCD_LDD_11,
	GPIO70_LCD_LDD_12,
	GPIO71_LCD_LDD_13,
	GPIO72_LCD_LDD_14,
	GPIO73_LCD_LDD_15,
	GPIO74_LCD_FCLK,
	GPIO75_LCD_LCLK,
	GPIO76_LCD_PCLK,
	GPIO77_LCD_BIAS,
	GPIO19_GPIO,		/* LCD reset */
	GPIO88_GPIO,		/* LCD chipselect */

	/* PWM */
	GPIO115_PWM1_OUT,	/* Keypad Backlight */
	GPIO11_PWM2_OUT,	/* LCD Backlight */

	/* MMC */
	GPIO32_MMC_CLK,
	GPIO112_MMC_CMD,
	GPIO92_MMC_DAT_0,
	GPIO109_MMC_DAT_1,
	GPIO110_MMC_DAT_2,
	GPIO111_MMC_DAT_3,
	GPIO96_GPIO,		/* SD detect */

	/* STUART */
	GPIO46_STUART_RXD,
	GPIO47_STUART_TXD,

	/* Keypad */
	GPIO100_KP_MKIN_0,
	GPIO101_KP_MKIN_1,
	GPIO102_KP_MKIN_2,
	GPIO34_KP_MKIN_3,
	GPIO38_KP_MKIN_4,
	GPIO16_KP_MKIN_5,
	GPIO17_KP_MKIN_6,
	GPIO103_KP_MKOUT_0,
	GPIO104_KP_MKOUT_1,
	GPIO105_KP_MKOUT_2,
	GPIO106_KP_MKOUT_3,
	GPIO107_KP_MKOUT_4,
	GPIO108_KP_MKOUT_5,
	GPIO35_KP_MKOUT_6,
	GPIO41_KP_MKOUT_7,

	/* I2C */
	GPIO117_I2C_SCL,
	GPIO118_I2C_SDA,

	/* SSP1 */
	GPIO23_SSP1_SCLK,	/* SSP1_SCK */
	GPIO25_SSP1_TXD,	/* SSP1_TXD */
	GPIO26_SSP1_RXD,	/* SSP1_RXD */

	/* SSP2 */
	GPIO22_SSP2_SCLK,	/* SSP2_SCK */
	GPIO13_SSP2_TXD,	/* SSP2_TXD */
	GPIO40_SSP2_RXD,	/* SSP2_RXD */

	/* LEDs */
	GPIO10_GPIO,		/* WiFi LED */
	GPIO83_GPIO,		/* Charging LED */
	GPIO85_GPIO,		/* Charged LED */

	/* I2S */
	GPIO28_I2S_BITCLK_OUT,
	GPIO29_I2S_SDATA_IN,
	GPIO30_I2S_SDATA_OUT,
	GPIO31_I2S_SYNC,
	GPIO113_I2S_SYSCLK,

	/* MISC */
	GPIO0_GPIO,		/* AC power detect */
	GPIO1_GPIO,		/* Power button */
	GPIO37_GPIO,		/* Headphone detect */
	GPIO98_GPIO,		/* Lid switch */
	GPIO14_GPIO,		/* WiFi Power */
	GPIO24_GPIO,		/* WiFi CS */
	GPIO36_GPIO,		/* WiFi IRQ */
	GPIO88_GPIO,		/* LCD CS */
};

/******************************************************************************
 * Framebuffer
 ******************************************************************************/
#if defined(CONFIG_FB_PXA) || defined(CONFIG_FB_PXA_MODULE)
static struct pxafb_mode_info z2_lcd_modes[] = {
{
	.pixclock	= 192000,
	.xres		= 240,
	.yres		= 320,
	.bpp		= 16,

	.left_margin	= 4,
	.right_margin	= 8,
	.upper_margin	= 4,
	.lower_margin	= 8,

	.hsync_len	= 4,
	.vsync_len	= 4,
},
};

static struct pxafb_mach_info z2_lcd_screen = {
	.modes		= z2_lcd_modes,
	.num_modes      = ARRAY_SIZE(z2_lcd_modes),
	.lcd_conn	= LCD_COLOR_TFT_16BPP | LCD_BIAS_ACTIVE_LOW |
			  LCD_ALTERNATE_MAPPING,
};

static void __init z2_lcd_init(void)
{
	pxa_set_fb_info(NULL, &z2_lcd_screen);
}
#else
static inline void z2_lcd_init(void) {}
#endif

/******************************************************************************
 * SSP Devices - WiFi and LCD control
 ******************************************************************************/
#if defined(CONFIG_SPI_PXA2XX) || defined(CONFIG_SPI_PXA2XX_MODULE)
/* WiFi */
static int z2_lbs_spi_setup(struct spi_device *spi)
{
	int ret = 0;

	ret = gpio_request(GPIO14_ZIPITZ2_WIFI_POWER, "WiFi Power");
	if (ret)
		goto err;

	ret = gpio_direction_output(GPIO14_ZIPITZ2_WIFI_POWER, 1);
	if (ret)
		goto err2;

	/* Wait until card is powered on */
	mdelay(180);

	spi->bits_per_word = 16;
	spi->mode = SPI_MODE_2,

	spi_setup(spi);

	return 0;

err2:
	gpio_free(GPIO14_ZIPITZ2_WIFI_POWER);
err:
	return ret;
};

static int z2_lbs_spi_teardown(struct spi_device *spi)
{
	gpio_set_value(GPIO14_ZIPITZ2_WIFI_POWER, 0);
	gpio_free(GPIO14_ZIPITZ2_WIFI_POWER);

	return 0;
};

static struct pxa2xx_spi_chip z2_lbs_chip_info = {
	.rx_threshold	= 8,
	.tx_threshold	= 8,
	.dma_burst_size = 8,
	.timeout	= 1000,
	.gpio_cs	= GPIO24_ZIPITZ2_WIFI_CS,
};

static struct libertas_spi_platform_data z2_lbs_pdata = {
	.use_dummy_writes	= 1,
	.setup			= z2_lbs_spi_setup,
	.teardown		= z2_lbs_spi_teardown,
};

/* LCD */
static struct pxa2xx_spi_chip lms283_chip_info = {
	.rx_threshold	= 1,
	.tx_threshold	= 1,
	.timeout	= 64,
	.gpio_cs	= GPIO88_ZIPITZ2_LCD_CS,
};

static const struct lms283gf05_pdata lms283_pdata = {
	.reset_gpio	= GPIO19_ZIPITZ2_LCD_RESET,
};

static struct spi_board_info spi_board_info[] __initdata = {
{
	.modalias		= "libertas_spi",
	.platform_data		= &z2_lbs_pdata,
	.controller_data	= &z2_lbs_chip_info,
	.max_speed_hz		= 16000000,
	.bus_num		= 1,
	.chip_select		= 0,
},
{
	.modalias		= "lms283gf05",
	.controller_data	= &lms283_chip_info,
	.platform_data		= &lms283_pdata,
	.max_speed_hz		= 400000,
	.bus_num		= 2,
	.chip_select		= 0,
},
};

static struct pxad_param ssp1_tx = {
	.drcmr = 14,
	.prio = 0,
};
static struct pxad_param ssp1_rx = {
	.drcmr = 13,
	.prio = 0,
};

static struct pxa2xx_spi_master pxa_ssp1_master_info = {
	.num_chipselect	= 1,
	.enable_dma	= 1,
	.dma_filter	= pxad_filter_fn,
	.tx_param	= &ssp1_tx,
	.rx_param	= &ssp1_rx,
};

static struct pxa2xx_spi_master pxa_ssp2_master_info = {
	.num_chipselect	= 1,
};

static void __init z2_spi_init(void)
{
	pxa2xx_set_spi_info(1, &pxa_ssp1_master_info);
	pxa2xx_set_spi_info(2, &pxa_ssp2_master_info);
	spi_board_info[0].irq = gpio_to_irq(GPIO36_ZIPITZ2_WIFI_IRQ);
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
}
#else
static inline void z2_spi_init(void) {}
#endif

#ifdef CONFIG_PM
static void z2_power_off(void)
{
	/* We're using deep sleep as poweroff, so clear PSPR to ensure that
	 * bootloader will jump to its entry point in resume handler
	 */
	PSPR = 0x0;
	local_irq_disable();
	pxa27x_set_pwrmode(PWRMODE_DEEPSLEEP);
	pxa27x_cpu_pm_enter(PM_SUSPEND_MEM);
}
#else
#define z2_power_off   NULL
#endif

/******************************************************************************
 * Machine init
 ******************************************************************************/
static void __init z2_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
                                        NULL, NULL);
	pxa2xx_mfp_config(ARRAY_AND_SIZE(z2_pin_config));

	z2_lcd_init();
	z2_spi_init();

	pm_power_off = z2_power_off;
}

static const char * const z2_dt_board_compat[] __initconst = {
	"zipitz2",
	NULL,
};

DT_MACHINE_START(ZIPIT2, "Zipit Z2")
	.atag_offset	= 0x100,
	.map_io		= pxa27x_map_io,
	.init_irq	= pxa27x_dt_init_irq,
	.handle_irq	= pxa27x_handle_irq,
	.init_machine	= z2_init,
	.restart	= pxa_restart,
	.dt_compat	= z2_dt_board_compat,
MACHINE_END
