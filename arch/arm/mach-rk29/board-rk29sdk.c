/* arch/arm/mach-rk29/board-rk29.c
 *
 * Copyright (C) 2010 ROCKCHIP, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/mmc/host.h>
#include <linux/android_pmem.h>

#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>
#include <asm/hardware/gic.h>

#include <mach/iomux.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <mach/rk29_iomap.h>
#include <mach/board.h>
#include <mach/rk29_nand.h>


#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include "devices.h"

/* Set memory size of pmem */
#define SDRAM_SIZE          SZ_128M
#define PMEM_GPU_SIZE       (12 * SZ_1M)
#define PMEM_UI_SIZE        SZ_16M
#define PMEM_VPU_SIZE       SZ_32M

#define PMEM_GPU_BASE       (RK29_SDRAM_PHYS + SDRAM_SIZE - PMEM_GPU_SIZE)
#define PMEM_UI_BASE        (PMEM_GPU_BASE - PMEM_UI_SIZE)
#define PMEM_VPU_BASE       (PMEM_UI_BASE - PMEM_VPU_SIZE)
#define LINUX_SIZE          (PMEM_VPU_BASE - RK29_SDRAM_PHYS)

extern struct sys_timer rk29_timer;

int rk29_nand_io_init(void)
{
    return 0;
}

struct rk29_nand_platform_data rk29_nand_data = {
    .width      = 1,     /* data bus width in bytes */
    .hw_ecc     = 1,     /* hw ecc 0: soft ecc */
    .num_flash    = 1,
    .io_init   = rk29_nand_io_init,
};

static struct rk29_gpio_bank rk29_gpiobankinit[] = {
	{
		.id		= RK29_ID_GPIO0,
		.offset	= RK29_GPIO0_BASE,
	},
	{
		.id		= RK29_ID_GPIO1,
		.offset	= RK29_GPIO1_BASE,
	}, 
	{
		.id		= RK29_ID_GPIO2,
		.offset	= RK29_GPIO2_BASE,
	}, 
	{
		.id		= RK29_ID_GPIO3,
		.offset	= RK29_GPIO3_BASE,
	}, 
	{
		.id		= RK29_ID_GPIO4,
		.offset	= RK29_GPIO4_BASE,
	}, 
	{
		.id		= RK29_ID_GPIO5,
		.offset	= RK29_GPIO5_BASE,
	}, 
	{
		.id		= RK29_ID_GPIO6,
		.offset	= RK29_GPIO6_BASE,
	},  	
};

/*****************************************************************************************
 * lcd  devices
 * author: zyw@rock-chips.com
 *****************************************************************************************/
//#ifdef  CONFIG_LCD_TD043MGEA1
#define LCD_TXD_PIN          RK29_PIN0_PA6   // ����,���޸�
#define LCD_CLK_PIN          RK29_PIN0_PA7   // ����,���޸�
#define LCD_CS_PIN           RK29_PIN0_PB6   // ����,���޸�
#define LCD_TXD_MUX_NAME     GPIOE_U1IR_I2C1_NAME
#define LCD_CLK_MUX_NAME     NULL
#define LCD_CS_MUX_NAME      GPIOH6_IQ_SEL_NAME
#define LCD_TXD_MUX_MODE     0
#define LCD_CLK_MUX_MODE     0
#define LCD_CS_MUX_MODE      0
//#endif
static int rk29_lcd_io_init(void)
{
    int ret = 0;

#if 0
    rk29_mux_api_set(LCD_CS_MUX_NAME, LCD_CS_MUX_MODE);
    if (LCD_CS_PIN != INVALID_GPIO) {
        ret = gpio_request(LCD_CS_PIN, NULL);
        if(ret != 0)
        {
            goto err1;
            printk(">>>>>> lcd cs gpio_request err \n ");
        }
    }

    rk29_mux_api_set(LCD_CLK_MUX_NAME, LCD_CLK_MUX_MODE);
    if (LCD_CLK_PIN != INVALID_GPIO) {
        ret = gpio_request(LCD_CLK_PIN, NULL);
        if(ret != 0)
        {
            goto err2;
            printk(">>>>>> lcd clk gpio_request err \n ");
        }
    }

    rk29_mux_api_set(LCD_TXD_MUX_NAME, LCD_TXD_MUX_MODE);
    if (LCD_TXD_PIN != INVALID_GPIO) {
        ret = gpio_request(LCD_TXD_PIN, NULL);
        if(ret != 0)
        {
            goto err3;
            printk(">>>>>> lcd txd gpio_request err \n ");
        }
    }

    return 0;

err3:
    if (LCD_CLK_PIN != INVALID_GPIO) {
        gpio_free(LCD_CLK_PIN);
    }
err2:
    if (LCD_CS_PIN != INVALID_GPIO) {
        gpio_free(LCD_CS_PIN);
    }
err1:
#endif
    return ret;
}

static int rk29_lcd_io_deinit(void)
{
    int ret = 0;
#if 0
    gpio_direction_output(LCD_CLK_PIN, 0);
    gpio_set_value(LCD_CLK_PIN, GPIO_HIGH);
    gpio_direction_output(LCD_TXD_PIN, 0);
    gpio_set_value(LCD_TXD_PIN, GPIO_HIGH);

    gpio_free(LCD_CS_PIN);
    rk29_mux_api_mode_resume(LCD_CS_MUX_NAME);
    gpio_free(LCD_CLK_PIN);
    gpio_free(LCD_TXD_PIN);
    rk29_mux_api_mode_resume(LCD_TXD_MUX_NAME);
    rk29_mux_api_mode_resume(LCD_CLK_MUX_NAME);
#endif
    return ret;
}

struct rk29lcd_info rk29_lcd_info = {
    //.txd_pin  = LCD_TXD_PIN,
    //.clk_pin = LCD_CLK_PIN,
    //.cs_pin = LCD_CS_PIN,
    .io_init   = rk29_lcd_io_init,
    .io_deinit = rk29_lcd_io_deinit,
};


/*****************************************************************************************
 * frame buffe  devices
 * author: zyw@rock-chips.com
 *****************************************************************************************/

#define FB_ID                       0
#define FB_DISPLAY_ON_PIN           RK29_PIN0_PB1   // ����,���޸�
#define FB_LCD_STANDBY_PIN          INVALID_GPIO
#define FB_MCU_FMK_PIN              INVALID_GPIO

#if 0
#define FB_DISPLAY_ON_VALUE         GPIO_LOW
#define FB_LCD_STANDBY_VALUE        0

#define FB_DISPLAY_ON_MUX_NAME      GPIOB1_SMCS1_MMC0PCA_NAME
#define FB_DISPLAY_ON_MUX_MODE      IOMUXA_GPIO0_B1

#define FB_LCD_STANDBY_MUX_NAME     NULL
#define FB_LCD_STANDBY_MUX_MODE     1

#define FB_MCU_FMK_PIN_MUX_NAME     NULL
#define FB_MCU_FMK_MUX_MODE         0

#define FB_DATA0_16_MUX_NAME       GPIOC_LCDC16BIT_SEL_NAME
#define FB_DATA0_16_MUX_MODE        1

#define FB_DATA17_18_MUX_NAME      GPIOC_LCDC18BIT_SEL_NAME
#define FB_DATA17_18_MUX_MODE       1

#define FB_DATA19_24_MUX_NAME      GPIOC_LCDC24BIT_SEL_NAME
#define FB_DATA19_24_MUX_MODE       1

#define FB_DEN_MUX_NAME            CXGPIO_LCDDEN_SEL_NAME
#define FB_DEN_MUX_MODE             1

#define FB_VSYNC_MUX_NAME          CXGPIO_LCDVSYNC_SEL_NAME
#define FB_VSYNC_MUX_MODE           1

#define FB_MCU_FMK_MUX_NAME        NULL
#define FB_MCU_FMK_MUX_MODE         0
#endif
static int rk29_fb_io_init(struct rk29_fb_setting_info *fb_setting)
{
    int ret = 0;
#if 0
    if(fb_setting->data_num <=16)
        rk29_mux_api_set(FB_DATA0_16_MUX_NAME, FB_DATA0_16_MUX_MODE);
    if(fb_setting->data_num >16 && fb_setting->data_num<=18)
        rk29_mux_api_set(FB_DATA17_18_MUX_NAME, FB_DATA17_18_MUX_MODE);
    if(fb_setting->data_num >18)
        rk29_mux_api_set(FB_DATA19_24_MUX_NAME, FB_DATA19_24_MUX_MODE);

    if(fb_setting->vsync_en)
        rk29_mux_api_set(FB_VSYNC_MUX_NAME, FB_VSYNC_MUX_MODE);

    if(fb_setting->den_en)
        rk29_mux_api_set(FB_DEN_MUX_NAME, FB_DEN_MUX_MODE);

    if(fb_setting->mcu_fmk_en && FB_MCU_FMK_MUX_NAME && (FB_MCU_FMK_PIN != INVALID_GPIO))
    {
        rk29_mux_api_set(FB_MCU_FMK_MUX_NAME, FB_MCU_FMK_MUX_MODE);
        ret = gpio_request(FB_MCU_FMK_PIN, NULL);
        if(ret != 0)
        {
            gpio_free(FB_MCU_FMK_PIN);
            printk(">>>>>> FB_MCU_FMK_PIN gpio_request err \n ");
        }
        gpio_direction_input(FB_MCU_FMK_PIN);
    }

    if(fb_setting->disp_on_en && FB_DISPLAY_ON_MUX_NAME && (FB_DISPLAY_ON_PIN != INVALID_GPIO))
    {
        rk29_mux_api_set(FB_DISPLAY_ON_MUX_NAME, FB_DISPLAY_ON_MUX_MODE);
        ret = gpio_request(FB_DISPLAY_ON_PIN, NULL);
        if(ret != 0)
        {
            gpio_free(FB_DISPLAY_ON_PIN);
            printk(">>>>>> FB_DISPLAY_ON_PIN gpio_request err \n ");
        }
    }

    if(fb_setting->disp_on_en && FB_LCD_STANDBY_MUX_NAME && (FB_LCD_STANDBY_PIN != INVALID_GPIO))
    {
        rk29_mux_api_set(FB_LCD_STANDBY_MUX_NAME, FB_LCD_STANDBY_MUX_MODE);
        ret = gpio_request(FB_LCD_STANDBY_PIN, NULL);
        if(ret != 0)
        {
            gpio_free(FB_LCD_STANDBY_PIN);
            printk(">>>>>> FB_LCD_STANDBY_PIN gpio_request err \n ");
        }
    }
#endif
    return ret;
}

struct rk29fb_info rk29_fb_info = {
    .fb_id   = FB_ID,
    //.disp_on_pin = FB_DISPLAY_ON_PIN,
    //.disp_on_value = FB_DISPLAY_ON_VALUE,
    //.standby_pin = FB_LCD_STANDBY_PIN,
    //.standby_value = FB_LCD_STANDBY_VALUE,
    //.mcu_fmk_pin = FB_MCU_FMK_PIN,
    .lcd_info = &rk29_lcd_info,
    .io_init   = rk29_fb_io_init,
};

static struct android_pmem_platform_data android_pmem_pdata = {
	.name		= "pmem",
	.start		= PMEM_UI_BASE,
	.size		= PMEM_UI_SIZE,
	.no_allocator	= 0,
	.cached		= 1,
};

static struct platform_device android_pmem_device = {
	.name		= "android_pmem",
	.id		= 0,
	.dev		= {
		.platform_data = &android_pmem_pdata,
	},
};

static struct android_pmem_platform_data android_pmem_gpu_pdata = {
	.name		= "pmem_gpu",
	.start		= PMEM_GPU_BASE,
	.size		= PMEM_GPU_SIZE,
	.no_allocator	= 0,
	.cached		= 0,
};

static struct platform_device android_pmem_gpu_device = {
	.name		= "android_pmem",
	.id		= 1,
	.dev		= {
		.platform_data = &android_pmem_gpu_pdata,
	},
};

static struct android_pmem_platform_data android_pmem_vpu_pdata = {
	.name		= "pmem_vpu",
	.start		= PMEM_VPU_BASE,
	.size		= PMEM_VPU_SIZE,
	.no_allocator	= 0,
	.cached		= 1,
};

static struct platform_device android_pmem_vpu_device = {
	.name		= "android_pmem",
	.id		= 2,
	.dev		= {
		.platform_data = &android_pmem_vpu_pdata,
	},
};

/*****************************************************************************************
 * SDMMC devices
*****************************************************************************************/
#ifdef CONFIG_SDMMC0_RK29
void rk29_sdmmc0_cfg_gpio(struct platform_device *dev)
{
	rk29_mux_api_set(GPIO1D1_SDMMC0CMD_NAME, GPIO1H_SDMMC0_CMD);
	rk29_mux_api_set(GPIO1D0_SDMMC0CLKOUT_NAME, GPIO1H_SDMMC0_CLKOUT);
	rk29_mux_api_set(GPIO1D2_SDMMC0DATA0_NAME, GPIO1H_SDMMC0_DATA0);
	rk29_mux_api_set(GPIO1D3_SDMMC0DATA1_NAME, GPIO1H_SDMMC0_DATA1);
	rk29_mux_api_set(GPIO1D4_SDMMC0DATA2_NAME, GPIO1H_SDMMC0_DATA2);
	rk29_mux_api_set(GPIO1D5_SDMMC0DATA3_NAME, GPIO1H_SDMMC0_DATA3);
	rk29_mux_api_set(GPIO2A2_SDMMC0DETECTN_NAME, GPIO2L_SDMMC0_DETECT_N);
}

#define CONFIG_SDMMC0_USE_DMA
struct rk29_sdmmc_platform_data default_sdmmc0_data = {
	.num_slots		= 1,
	.host_ocr_avail = (MMC_VDD_27_28|MMC_VDD_28_29|MMC_VDD_29_30|
					   MMC_VDD_30_31|MMC_VDD_31_32|MMC_VDD_32_33| 
					   MMC_VDD_33_34|MMC_VDD_34_35| MMC_VDD_35_36),
	.host_caps 	= (MMC_CAP_4_BIT_DATA|MMC_CAP_MMC_HIGHSPEED|MMC_CAP_SD_HIGHSPEED),
	.io_init = rk29_sdmmc0_cfg_gpio,
	.dma_name = "sd_mmc",
#ifdef CONFIG_SDMMC0_USE_DMA
	.use_dma  = 1,
#else
	.use_dma = 0,
#endif
};
#endif
#ifdef CONFIG_SDMMC1_RK29
#define CONFIG_SDMMC1_USE_DMA
void rk29_sdmmc1_cfg_gpio(struct platform_device *dev)
{
	rk29_mux_api_set(GPIO1C2_SDMMC1CMD_NAME, GPIO1H_SDMMC1_CMD);
	rk29_mux_api_set(GPIO1C7_SDMMC1CLKOUT_NAME, GPIO1H_SDMMC1_CLKOUT);
	rk29_mux_api_set(GPIO1C3_SDMMC1DATA0_NAME, GPIO1H_SDMMC1_DATA0);
	rk29_mux_api_set(GPIO1C4_SDMMC1DATA1_NAME, GPIO1H_SDMMC1_DATA1);
	rk29_mux_api_set(GPIO1C5_SDMMC1DATA2_NAME, GPIO1H_SDMMC1_DATA2);
	rk29_mux_api_set(GPIO1C6_SDMMC1DATA3_NAME, GPIO1H_SDMMC1_DATA3);
}

struct rk29_sdmmc_platform_data default_sdmmc1_data = {
	.num_slots		= 1,
	.host_ocr_avail = (MMC_VDD_26_27|MMC_VDD_27_28|MMC_VDD_28_29|
					   MMC_VDD_29_30|MMC_VDD_30_31|MMC_VDD_31_32|
					   MMC_VDD_32_33|MMC_VDD_33_34),
	.host_caps 	= (MMC_CAP_4_BIT_DATA|MMC_CAP_SDIO_IRQ|
				   MMC_CAP_MMC_HIGHSPEED|MMC_CAP_SD_HIGHSPEED),
	.io_init = rk29_sdmmc1_cfg_gpio,
	.dma_name = "sdio",
#ifdef CONFIG_SDMMC1_USE_DMA
	.use_dma  = 1,
#else
	.use_dma = 0,
#endif
};
#endif

static void __init rk29_board_iomux_init(void)
{
	#ifdef CONFIG_UART0_RK29	
	rk29_mux_api_set(GPIO1B7_UART0SOUT_NAME, GPIO1L_UART0_SOUT);
	rk29_mux_api_set(GPIO1B6_UART0SIN_NAME, GPIO1L_UART0_SIN);
	#ifdef CONFIG_UART0_CTS_RTS_RK29
	rk29_mux_api_set(GPIO1C1_UART0RTSN_SDMMC1WRITEPRT_NAME, GPIO1H_UART0_RTS_N);
	rk29_mux_api_set(GPIO1C0_UART0CTSN_SDMMC1DETECTN_NAME, GPIO1H_UART0_CTS_N);
	#endif
	#endif
	#ifdef CONFIG_UART1_RK29	
	rk29_mux_api_set(GPIO2A5_UART1SOUT_NAME, GPIO2L_UART1_SOUT);
	rk29_mux_api_set(GPIO2A4_UART1SIN_NAME, GPIO2L_UART1_SIN);
	#endif
	#ifdef CONFIG_UART2_RK29	
	rk29_mux_api_set(GPIO2B1_UART2SOUT_NAME, GPIO2L_UART2_SOUT);
	rk29_mux_api_set(GPIO2B0_UART2SIN_NAME, GPIO2L_UART2_SIN);
	#ifdef CONFIG_UART2_CTS_RTS_RK29
	rk29_mux_api_set(GPIO2A7_UART2RTSN_NAME, GPIO2L_UART2_RTS_N);
	rk29_mux_api_set(GPIO2A6_UART2CTSN_NAME, GPIO2L_UART2_CTS_N);
	#endif
	#endif
	#ifdef CONFIG_UART3_RK29	
	rk29_mux_api_set(GPIO2B3_UART3SOUT_NAME, GPIO2L_UART3_SOUT);
	rk29_mux_api_set(GPIO2B2_UART3SIN_NAME, GPIO2L_UART3_SIN);
	#ifdef CONFIG_UART3_CTS_RTS_RK29
	rk29_mux_api_set(GPIO2B5_UART3RTSN_I2C3SCL_NAME, GPIO2L_UART3_RTS_N);
	rk29_mux_api_set(GPIO2B4_UART3CTSN_I2C3SDA_NAME, GPIO2L_UART3_CTS_N);
	#endif
	#endif
}

static struct platform_device *devices[] __initdata = {
#ifdef CONFIG_UART1_RK29
	&rk29_device_uart1,
#endif	
#ifdef CONFIG_SDMMC0_RK29	
	&rk29_device_sdmmc0,
#endif
#ifdef CONFIG_SDMMC1_RK29
	&rk29_device_sdmmc1,
#endif
#ifdef CONFIG_MTD_NAND_RK29
	&rk29_device_nand,
#endif

#ifdef CONFIG_FB_RK29
	&rk29_device_fb,
#endif
#ifdef CONFIG_VIVANTE
	&rk29_device_gpu,
#endif
	&android_pmem_device,
	&android_pmem_gpu_device,
	&android_pmem_vpu_device,
};

static void __init rk29_gic_init_irq(void)
{
	gic_dist_init(0, (void __iomem *)RK29_GICPERI_BASE, 32);
	gic_cpu_init(0, (void __iomem *)RK29_GICCPU_BASE);
}

static void __init machine_rk29_init_irq(void)
{
	rk29_gic_init_irq();
	rk29_gpio_init(rk29_gpiobankinit, MAX_BANK);
	rk29_gpio_irq_setup();
}

static void __init machine_rk29_board_init(void)
{ 
	platform_add_devices(devices, ARRAY_SIZE(devices));	
	rk29_board_iomux_init();
}

static void __init machine_rk29_fixup(struct machine_desc *desc, struct tag *tags,
					char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 1;
	mi->bank[0].start = RK29_SDRAM_PHYS;
	mi->bank[0].node = PHYS_TO_NID(RK29_SDRAM_PHYS);
	mi->bank[0].size = LINUX_SIZE;
}

static void __init machine_rk29_mapio(void)
{
	rk29_map_common_io();
	rk29_clock_init();
	rk29_iomux_init();	
}

MACHINE_START(RK29, "RK29board")
	/* UART for LL DEBUG */
	.phys_io	= RK29_UART1_PHYS, 
	.io_pg_offst	= ((RK29_UART1_BASE) >> 18) & 0xfffc,
	.boot_params	= RK29_SDRAM_PHYS + 0x88000,
	.fixup		= machine_rk29_fixup,
	.map_io		= machine_rk29_mapio,
	.init_irq	= machine_rk29_init_irq,
	.init_machine	= machine_rk29_board_init,
	.timer		= &rk29_timer,
MACHINE_END
