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
#include <mach/rk29_camera.h>                          /* ddl@rock-chips.com : camera support */
#include <media/soc_camera.h>                               /* ddl@rock-chips.com : camera support */

#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include "devices.h"
#include "../../../drivers/input/touchscreen/xpt2046_cbn_ts.h"


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
 * i2c devices
 * author: kfx@rock-chips.com
*****************************************************************************************/
static int rk29_i2c0_io_init(void)
{
	rk29_mux_api_set(GPIO2B7_I2C0SCL_NAME, GPIO2L_I2C0_SCL);
	rk29_mux_api_set(GPIO2B6_I2C0SDA_NAME, GPIO2L_I2C0_SDA);
	return 0;
}

static int rk29_i2c1_io_init(void)
{
	rk29_mux_api_set(GPIO1A7_I2C1SCL_NAME, GPIO1L_I2C1_SCL);
	rk29_mux_api_set(GPIO1A6_I2C1SDA_NAME, GPIO1L_I2C1_SDA);
	return 0;
}
static int rk29_i2c2_io_init(void)
{
	rk29_mux_api_set(GPIO5D4_I2C2SCL_NAME, GPIO5H_I2C2_SCL);
	rk29_mux_api_set(GPIO5D3_I2C2SDA_NAME, GPIO5H_I2C2_SDA);
	return 0;
}

static int rk29_i2c3_io_init(void)
{
	rk29_mux_api_set(GPIO2B5_UART3RTSN_I2C3SCL_NAME, GPIO2L_I2C3_SCL);
	rk29_mux_api_set(GPIO2B4_UART3CTSN_I2C3SDA_NAME, GPIO2L_I2C3_SDA);
	return 0;
}

struct rk29_i2c_platform_data default_i2c0_data = { 
	.bus_num    = 0,
	.flags      = 0,
	.slave_addr = 0xff,
	.scl_rate  = 400*1000,
	.mode 		= I2C_MODE_IRQ,
	.io_init = rk29_i2c0_io_init,
};

struct rk29_i2c_platform_data default_i2c1_data = { 
	.bus_num    = 1,
	.flags      = 0,
	.slave_addr = 0xff,
	.scl_rate  = 400*1000,
	.mode 		= I2C_MODE_POLL,
	.io_init = rk29_i2c1_io_init,
};

struct rk29_i2c_platform_data default_i2c2_data = { 
	.bus_num    = 2,
	.flags      = 0,
	.slave_addr = 0xff,
	.scl_rate  = 400*1000,
	.mode 		= I2C_MODE_IRQ,
	.io_init = rk29_i2c2_io_init,
};

struct rk29_i2c_platform_data default_i2c3_data = { 
	.bus_num    = 3,
	.flags      = 0,
	.slave_addr = 0xff,
	.scl_rate  = 400*1000,
	.mode 		= I2C_MODE_POLL,
	.io_init = rk29_i2c3_io_init,
};

#ifdef CONFIG_I2C0_RK29
static struct i2c_board_info __initdata board_i2c0_devices[] = {
#if defined (CONFIG_RK1000_CONTROL)
	{
		.type    		= "rk1000_control",
		.addr           = 0x40,
		.flags			= 0,
	},
#endif
#if defined (CONFIG_SND_SOC_RK1000)
	{
		.type    		= "rk1000_i2c_codec",
		.addr           = 0x60,
		.flags			= 0,
	},
#endif
#if defined (CONFIG_BATTERY_STC3100)
	{
		.type    		= "stc3100-battery",
		.addr           = 0x70,
		.flags			= 0,
	},
#endif
#if defined (CONFIG_BATTERY_BQ27510)
	{
		.type    		= "bq27510-battery",
		.addr           = 0x55,
		.flags			= 0,
	},
#endif
};
#endif

#ifdef CONFIG_I2C1_RK29
static struct i2c_board_info __initdata board_i2c1_devices[] = {
#if defined (CONFIG_RK1000_CONTROL1)
	{
		.type			= "rk1000_control",
		.addr			= 0x40,
		.flags			= 0,
	},
#endif
#if defined (CONFIG_SENSORS_AK8973)
	{
		.type    		= "ak8973",
		.addr           = 0x1c,
		.flags			= 0,
	},
#endif
#if defined (CONFIG_SENSORS_AK8973)
	{
		.type    		= "ak8975",
		.addr           = 0x1c,
		.flags			= 0,
	},
#endif
};
#endif

#ifdef CONFIG_I2C2_RK29
static struct i2c_board_info __initdata board_i2c2_devices[] = {
};
#endif

#ifdef CONFIG_I2C3_RK29
static struct i2c_board_info __initdata board_i2c3_devices[] = {
};
#endif

/*****************************************************************************************
 * camera  devices
 * author: ddl@rock-chips.com
 *****************************************************************************************/
#ifdef CONFIG_VIDEO_RK29
#define SENSOR_NAME_0 RK29_CAM_SENSOR_NAME_OV2655
#define SENSOR_IIC_ADDR_0 	    0x60
#define SENSOR_IIC_ADAPTER_ID_0    1
#define SENSOR_POWER_PIN_0         INVALID_GPIO
#define SENSOR_RESET_PIN_0         RK29_PIN0_PA2
#define SENSOR_POWERACTIVE_LEVEL_0 RK29_CAM_POWERACTIVE_L
#define SENSOR_RESETACTIVE_LEVEL_0 RK29_CAM_RESETACTIVE_L


#define SENSOR_NAME_1 NULL
#define SENSOR_IIC_ADDR_1 	    0x00
#define SENSOR_IIC_ADAPTER_ID_1    0xff
#define SENSOR_POWER_PIN_1         INVALID_GPIO
#define SENSOR_RESET_PIN_1         INVALID_GPIO
#define SENSOR_POWERACTIVE_LEVEL_1 RK29_CAM_POWERACTIVE_L
#define SENSOR_RESETACTIVE_LEVEL_1 RK29_CAM_RESETACTIVE_L

static int rk29_sensor_io_init(void);
static int rk29_sensor_io_deinit(void);

struct rk29camera_platform_data rk29_camera_platform_data = {
    .io_init = rk29_sensor_io_init,
    .io_deinit = rk29_sensor_io_deinit,
    .gpio_res = {
        {
            .gpio_reset = SENSOR_RESET_PIN_0,
            .gpio_power = SENSOR_POWER_PIN_0,
            .gpio_flag = (SENSOR_POWERACTIVE_LEVEL_0|SENSOR_RESETACTIVE_LEVEL_0),
            .dev_name = SENSOR_NAME_0,
        }, {
            .gpio_reset = SENSOR_RESET_PIN_1,
            .gpio_power = SENSOR_POWER_PIN_1,
            .gpio_flag = (SENSOR_POWERACTIVE_LEVEL_1|SENSOR_RESETACTIVE_LEVEL_1),
            .dev_name = SENSOR_NAME_1,
        }
    }
};

static int rk29_sensor_io_init(void)
{
    int ret = 0, i;
    unsigned int camera_reset = INVALID_GPIO, camera_power = INVALID_GPIO;
	unsigned int camera_ioflag;

    for (i=0; i<2; i++) {
        camera_reset = rk29_camera_platform_data.gpio_res[i].gpio_reset;
        camera_power = rk29_camera_platform_data.gpio_res[i].gpio_power;
		camera_ioflag = rk29_camera_platform_data.gpio_res[i].gpio_flag;

        if (camera_power != INVALID_GPIO) {
            ret = gpio_request(camera_power, "camera power");
            if (ret)
                continue;

            gpio_set_value(camera_reset, (((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
            gpio_direction_output(camera_power, (((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));

			//printk("\n%s....%d  %x   \n",__FUNCTION__,__LINE__,(((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));

        }

        if (camera_reset != INVALID_GPIO) {
            ret = gpio_request(camera_reset, "camera reset");
            if (ret) {
                if (camera_power != INVALID_GPIO)
                    gpio_free(camera_power);

                continue;
            }

            gpio_set_value(camera_reset, ((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
            gpio_direction_output(camera_reset, ((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));

			//printk("\n%s....%d  %x \n",__FUNCTION__,__LINE__,((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));

        }
    }

    return 0;
}

static int rk29_sensor_io_deinit(void)
{
    unsigned int i;
    unsigned int camera_reset = INVALID_GPIO, camera_power = INVALID_GPIO;

    //printk("\n%s....%d    ******** ddl *********\n",__FUNCTION__,__LINE__);

    for (i=0; i<2; i++) {
        camera_reset = rk29_camera_platform_data.gpio_res[i].gpio_reset;
        camera_power = rk29_camera_platform_data.gpio_res[i].gpio_power;

        if (camera_power != INVALID_GPIO){
            gpio_direction_input(camera_power);
            gpio_free(camera_power);
        }

        if (camera_reset != INVALID_GPIO)  {
            gpio_direction_input(camera_reset);
            gpio_free(camera_reset);
        }
    }

    return 0;
}


static int rk29_sensor_power(struct device *dev, int on)
{
    unsigned int camera_reset = INVALID_GPIO, camera_power = INVALID_GPIO;
	unsigned int camera_ioflag;

    if(rk29_camera_platform_data.gpio_res[0].dev_name &&  (strcmp(rk29_camera_platform_data.gpio_res[0].dev_name, dev_name(dev)) == 0)) {
        camera_reset = rk29_camera_platform_data.gpio_res[0].gpio_reset;
        camera_power = rk29_camera_platform_data.gpio_res[0].gpio_power;
		camera_ioflag = rk29_camera_platform_data.gpio_res[0].gpio_flag;
    } else if (rk29_camera_platform_data.gpio_res[1].dev_name && (strcmp(rk29_camera_platform_data.gpio_res[1].dev_name, dev_name(dev)) == 0)) {
        camera_reset = rk29_camera_platform_data.gpio_res[1].gpio_reset;
        camera_power = rk29_camera_platform_data.gpio_res[1].gpio_power;
		camera_ioflag = rk29_camera_platform_data.gpio_res[1].gpio_flag;
    }

    if (camera_reset != INVALID_GPIO) {
        gpio_set_value(camera_reset, ((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
        //printk("\n%s..%s..ResetPin=%d ..PinLevel = %x \n",__FUNCTION__,dev_name(dev),camera_reset, ((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
    }
    if (camera_power != INVALID_GPIO)  {
        if (on) {
        	gpio_set_value(camera_power, ((camera_ioflag&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
			//printk("\n%s..%s..PowerPin=%d ..PinLevel = %x   \n",__FUNCTION__,dev_name(dev), camera_power, ((camera_ioflag&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
		} else {
			gpio_set_value(camera_power, (((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
			//printk("\n%s..%s..PowerPin=%d ..PinLevel = %x   \n",__FUNCTION__,dev_name(dev), camera_power, (((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
		}
	}

    if (camera_reset != INVALID_GPIO)  {
		if (on) {
	        msleep(3);          /* delay 3 ms */
	        gpio_set_value(camera_reset,(((~camera_ioflag)&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
	        //printk("\n%s..%s..ResetPin= %d..PinLevel = %x   \n",__FUNCTION__,dev_name(dev), camera_reset, (((~camera_ioflag)&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
		}
    }
    return 0;
}

static struct i2c_board_info rk29_i2c_cam_info[] = {
	{
		I2C_BOARD_INFO(SENSOR_NAME_0, SENSOR_IIC_ADDR_0>>1)
	},
};

struct soc_camera_link rk29_iclink = {
	.bus_id		= RK29_CAM_PLATFORM_DEV_ID,
	.power		= rk29_sensor_power,
	.board_info	= &rk29_i2c_cam_info[0],
	.i2c_adapter_id	= SENSOR_IIC_ADAPTER_ID_0,
	.module_name	= SENSOR_NAME_0,
};

/*platform_device : soc-camera need  */
struct platform_device rk29_soc_camera_pdrv = {
	.name	= "soc-camera-pdrv",
	.id	= -1,
	.dev	= {
		.init_name = SENSOR_NAME_0,
		.platform_data = &rk29_iclink,
	},
};

extern struct platform_device rk29_device_camera;
#endif
/*****************************************************************************************
 * backlight  devices
 * author: nzy@rock-chips.com
 *****************************************************************************************/
#ifdef CONFIG_BACKLIGHT_RK29_BL
 /*
 GPIO1B5_PWM0_NAME,       GPIO1L_PWM0
 GPIO5D2_PWM1_UART1SIRIN_NAME,  GPIO5H_PWM1
 GPIO2A3_SDMMC0WRITEPRT_PWM2_NAME,   GPIO2L_PWM2
 GPIO1A5_EMMCPWREN_PWM3_NAME,     GPIO1L_PWM3
 */
 
#define PWM_ID            0  
#define PWM_MUX_NAME      GPIO1B5_PWM0_NAME
#define PWM_MUX_MODE      GPIO1L_PWM0
#define PWM_MUX_MODE_GPIO GPIO1L_GPIO1B5
#define PWM_EFFECT_VALUE  0

//#define LCD_DISP_ON_PIN

#ifdef  LCD_DISP_ON_PIN
#define BL_EN_MUX_NAME    GPIOF34_UART3_SEL_NAME
#define BL_EN_MUX_MODE    IOMUXB_GPIO1_B34

#define BL_EN_PIN         GPIO0L_GPIO0A5
#define BL_EN_VALUE       GPIO_HIGH
#endif
static int rk29_backlight_io_init(void)
{
    int ret = 0;
    
    rk29_mux_api_set(PWM_MUX_NAME, PWM_MUX_MODE);
	#ifdef  LCD_DISP_ON_PIN
    rk29_mux_api_set(BL_EN_MUX_NAME, BL_EN_MUX_MODE); 
	
    ret = gpio_request(BL_EN_PIN, NULL); 
    if(ret != 0)
    {
        gpio_free(BL_EN_PIN);   
    }
    
    gpio_direction_output(BL_EN_PIN, 0);
    gpio_set_value(BL_EN_PIN, BL_EN_VALUE);
	#endif
    return ret;
}

static int rk29_backlight_io_deinit(void)
{
    int ret = 0;
    #ifdef  LCD_DISP_ON_PIN
    gpio_free(BL_EN_PIN);
    #endif
    rk29_mux_api_set(PWM_MUX_NAME, PWM_MUX_MODE_GPIO);
    return ret;
}
struct rk29_bl_info rk29_bl_info = {
    .pwm_id   = PWM_ID,
    .bl_ref   = PWM_EFFECT_VALUE,
    .io_init   = rk29_backlight_io_init,
    .io_deinit = rk29_backlight_io_deinit, 
};
#endif
/*****************************************************************************************
 * SDMMC devices
*****************************************************************************************/
#ifdef CONFIG_SDMMC0_RK29
static int rk29_sdmmc0_cfg_gpio(void)
{
	rk29_mux_api_set(GPIO1D1_SDMMC0CMD_NAME, GPIO1H_SDMMC0_CMD);
	rk29_mux_api_set(GPIO1D0_SDMMC0CLKOUT_NAME, GPIO1H_SDMMC0_CLKOUT);
	rk29_mux_api_set(GPIO1D2_SDMMC0DATA0_NAME, GPIO1H_SDMMC0_DATA0);
	rk29_mux_api_set(GPIO1D3_SDMMC0DATA1_NAME, GPIO1H_SDMMC0_DATA1);
	rk29_mux_api_set(GPIO1D4_SDMMC0DATA2_NAME, GPIO1H_SDMMC0_DATA2);
	rk29_mux_api_set(GPIO1D5_SDMMC0DATA3_NAME, GPIO1H_SDMMC0_DATA3);
	rk29_mux_api_set(GPIO2A2_SDMMC0DETECTN_NAME, GPIO2L_SDMMC0_DETECT_N);
	return 0;
}

#define CONFIG_SDMMC0_USE_DMA
struct rk29_sdmmc_platform_data default_sdmmc0_data = {
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
//#define CONFIG_SDMMC1_USE_DMA
static int rk29_sdmmc1_cfg_gpio(void)
{
	rk29_mux_api_set(GPIO1C2_SDMMC1CMD_NAME, GPIO1H_SDMMC1_CMD);
	rk29_mux_api_set(GPIO1C7_SDMMC1CLKOUT_NAME, GPIO1H_SDMMC1_CLKOUT);
	rk29_mux_api_set(GPIO1C3_SDMMC1DATA0_NAME, GPIO1H_SDMMC1_DATA0);
	rk29_mux_api_set(GPIO1C4_SDMMC1DATA1_NAME, GPIO1H_SDMMC1_DATA1);
	rk29_mux_api_set(GPIO1C5_SDMMC1DATA2_NAME, GPIO1H_SDMMC1_DATA2);
	rk29_mux_api_set(GPIO1C6_SDMMC1DATA3_NAME, GPIO1H_SDMMC1_DATA3);
	rk29_mux_api_set(GPIO1C0_UART0CTSN_SDMMC1DETECTN_NAME, GPIO1H_SDMMC1_DETECT_N);
	return 0;
}

struct rk29_sdmmc_platform_data default_sdmmc1_data = {
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

#ifdef CONFIG_VIVANTE
static struct resource resources_gpu[] = {
    [0] = {
		.name 	= "gpu_irq",
        .start 	= IRQ_GPU,
        .end    = IRQ_GPU,
        .flags  = IORESOURCE_IRQ,
    },
    [1] = {
		.name = "gpu_base",
        .start  = RK29_GPU_PHYS,
        .end    = RK29_GPU_PHYS + (256 << 10),
        .flags  = IORESOURCE_MEM,
    },
    [2] = {
		.name = "gpu_mem",
        .start  = PMEM_GPU_BASE,
        .end    = PMEM_GPU_BASE + PMEM_GPU_SIZE,
        .flags  = IORESOURCE_MEM,
    },
};
struct platform_device rk29_device_gpu = {
    .name             = "galcore",
    .id               = 0,
    .num_resources    = ARRAY_SIZE(resources_gpu),
    .resource         = resources_gpu,
};
#endif
#ifdef CONFIG_KEYS_RK29
extern struct rk29_keys_platform_data rk29_keys_pdata; 
static struct platform_device rk29_device_keys = {
	.name		= "rk29-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &rk29_keys_pdata,
	},
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
#ifdef CONFIG_SPIM0_RK29
    &rk29xx_device_spi0m,
#endif
#ifdef CONFIG_SPIM1_RK29
    &rk29xx_device_spi1m,
#endif
#ifdef CONFIG_ADC_RK29
	&rk29_device_adc,
#endif
#ifdef CONFIG_I2C0_RK29
	&rk29_device_i2c0,
#endif
#ifdef CONFIG_I2C1_RK29
	&rk29_device_i2c1,
#endif
#ifdef CONFIG_I2C2_RK29
	&rk29_device_i2c2,
#endif
#ifdef CONFIG_I2C3_RK29
	&rk29_device_i2c3,
#endif

#ifdef CONFIG_SND_RK29_SOC_I2C_2CH
        &rk29_device_iis_2ch,
#endif
#ifdef CONFIG_SND_RK29_SOC_I2S_8CH
        &rk29_device_iis_8ch,
#endif

#ifdef CONFIG_KEYS_RK29
	&rk29_device_keys,
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
#ifdef CONFIG_BACKLIGHT_RK29_BL
	&rk29_device_backlight,
#endif
#ifdef CONFIG_VIVANTE
	&rk29_device_gpu,
#endif
#ifdef CONFIG_VIDEO_RK29
 	&rk29_device_camera,      /* ddl@rock-chips.com : camera support  */
 	&rk29_soc_camera_pdrv,
 #endif
	&android_pmem_device,
	&android_pmem_vpu_device,
};

/*****************************************************************************************
 * spi devices
 * author: cmc@rock-chips.com
 *****************************************************************************************/
#define SPI_CHIPSELECT_NUM 2
struct spi_cs_gpio rk29xx_spi0_cs_gpios[SPI_CHIPSELECT_NUM] = {
    {
		.name = "spi0 cs0",
		.cs_gpio = RK29_PIN2_PC1,
		.cs_iomux_name = NULL,
	},
	{
		.name = "spi0 cs1",
		.cs_gpio = RK29_PIN1_PA4,
		.cs_iomux_name = GPIO1A4_EMMCWRITEPRT_SPI0CS1_NAME,//if no iomux,set it NULL
		.cs_iomux_mode = GPIO1L_SPI0_CSN1,
	}
};

struct spi_cs_gpio rk29xx_spi1_cs_gpios[SPI_CHIPSELECT_NUM] = {
    {
		.name = "spi1 cs0",
		.cs_gpio = RK29_PIN2_PC5,
		.cs_iomux_name = NULL,
	},
	{
		.name = "spi1 cs1",
		.cs_gpio = RK29_PIN1_PA3,
		.cs_iomux_name = GPIO1A3_EMMCDETECTN_SPI1CS1_NAME,//if no iomux,set it NULL
		.cs_iomux_mode = GPIO1L_SPI0_CSN1,
	}
};

static int spi_io_init(struct spi_cs_gpio *cs_gpios, int cs_num)
{	
#if 0
	int i,j,ret;
	
	//cs
	if (cs_gpios) {
		for (i=0; i<cs_num; i++) {
			rk29_mux_api_set(cs_gpios[i].cs_iomux_name, cs_gpios[i].cs_iomux_mode);
			ret = gpio_request(cs_gpios[i].cs_gpio, cs_gpios[i].name);
			if (ret) {
				for (j=0;j<i;j++) {
					gpio_free(cs_gpios[j].cs_gpio);
					//rk29_mux_api_mode_resume(cs_gpios[j].cs_iomux_name);
				}
				printk("[fun:%s, line:%d], gpio request err\n", __func__, __LINE__);
				return -1;
			}			
			gpio_direction_output(cs_gpios[i].cs_gpio, GPIO_HIGH);
		}
	}
#endif
	return 0;
}

static int spi_io_deinit(struct spi_cs_gpio *cs_gpios, int cs_num)
{
#if 0
	int i;
	
	if (cs_gpios) {
		for (i=0; i<cs_num; i++) {
			gpio_free(cs_gpios[i].cs_gpio);
			//rk29_mux_api_mode_resume(cs_gpios[i].cs_iomux_name);
		}
	}
#endif
	return 0;
}

static int spi_io_fix_leakage_bug(void)
{
#if 0
	gpio_direction_output(RK29_PIN2_PC1, GPIO_LOW); 
#endif
	return 0;
}

static int spi_io_resume_leakage_bug(void)
{
#if 0
	gpio_direction_output(RK29_PIN2_PC1, GPIO_HIGH);
#endif
	return 0;
}

struct rk29xx_spi_platform_data rk29xx_spi0_platdata = {
	.num_chipselect = SPI_CHIPSELECT_NUM,
	.chipselect_gpios = rk29xx_spi0_cs_gpios,
	.io_init = spi_io_init,
	.io_deinit = spi_io_deinit,
	.io_fix_leakage_bug = spi_io_fix_leakage_bug,
	.io_resume_leakage_bug = spi_io_resume_leakage_bug,
};

struct rk29xx_spi_platform_data rk29xx_spi1_platdata = {
	.num_chipselect = SPI_CHIPSELECT_NUM,
	.chipselect_gpios = rk29xx_spi1_cs_gpios,
	.io_init = spi_io_init,
	.io_deinit = spi_io_deinit,
	.io_fix_leakage_bug = spi_io_fix_leakage_bug,
	.io_resume_leakage_bug = spi_io_resume_leakage_bug,
};

/*****************************************************************************************
 * xpt2046 touch panel
 * author: cmc@rock-chips.com
 *****************************************************************************************/
#define XPT2046_GPIO_INT           RK29_PIN0_PA3
#define DEBOUNCE_REPTIME  3

#if defined(CONFIG_TOUCHSCREEN_XPT2046_320X480_SPI) 
static struct xpt2046_platform_data xpt2046_info = {
	.model			= 2046,
	.keep_vref_on 	= 1,
	.swap_xy		= 0,
	.x_min			= 0,
	.x_max			= 320,
	.y_min			= 0,
	.y_max			= 480,
	.debounce_max		= 7,
	.debounce_rep		= DEBOUNCE_REPTIME,
	.debounce_tol		= 20,
	.gpio_pendown		= XPT2046_GPIO_INT,
	.penirq_recheck_delay_usecs = 1,
};
#elif defined(CONFIG_TOUCHSCREEN_XPT2046_320X480_CBN_SPI)
static struct xpt2046_platform_data xpt2046_info = {
	.model			= 2046,
	.keep_vref_on 	= 1,
	.swap_xy		= 0,
	.x_min			= 0,
	.x_max			= 320,
	.y_min			= 0,
	.y_max			= 480,
	.debounce_max		= 7,
	.debounce_rep		= DEBOUNCE_REPTIME,
	.debounce_tol		= 20,
	.gpio_pendown		= XPT2046_GPIO_INT,
	.penirq_recheck_delay_usecs = 1,
};
#elif defined(CONFIG_TOUCHSCREEN_XPT2046_SPI) 
static struct xpt2046_platform_data xpt2046_info = {
	.model			= 2046,
	.keep_vref_on 	= 1,
	.swap_xy		= 1,
	.x_min			= 0,
	.x_max			= 800,
	.y_min			= 0,
	.y_max			= 480,
	.debounce_max		= 7,
	.debounce_rep		= DEBOUNCE_REPTIME,
	.debounce_tol		= 20,
	.gpio_pendown		= XPT2046_GPIO_INT,
	
	.penirq_recheck_delay_usecs = 1,
};
#elif defined(CONFIG_TOUCHSCREEN_XPT2046_CBN_SPI)
static struct xpt2046_platform_data xpt2046_info = {
	.model			= 2046,
	.keep_vref_on 	= 1,
	.swap_xy		= 1,
	.x_min			= 0,
	.x_max			= 800,
	.y_min			= 0,
	.y_max			= 480,
	.debounce_max		= 7,
	.debounce_rep		= DEBOUNCE_REPTIME,
	.debounce_tol		= 20,
	.gpio_pendown		= XPT2046_GPIO_INT,
	
	.penirq_recheck_delay_usecs = 1,
};
#endif

static struct spi_board_info board_spi_devices[] = {
#if defined(CONFIG_TOUCHSCREEN_XPT2046_320X480_SPI) || defined(CONFIG_TOUCHSCREEN_XPT2046_320X480_CBN_SPI)\
    ||defined(CONFIG_TOUCHSCREEN_XPT2046_SPI) || defined(CONFIG_TOUCHSCREEN_XPT2046_CBN_SPI)
	{
		.modalias	= "xpt2046_ts",
		.chip_select	= 0,
		.max_speed_hz	= 125 * 1000 * 8,/* (max sample rate @ 3V) * (cmd + data + overhead) */
		.bus_num	= 0,
		.irq = XPT2046_GPIO_INT,
		.platform_data = &xpt2046_info,
	},
#endif
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
        rk29_board_iomux_init();
		platform_add_devices(devices, ARRAY_SIZE(devices));	
#ifdef CONFIG_I2C0_RK29
	i2c_register_board_info(default_i2c0_data.bus_num, board_i2c0_devices,
			ARRAY_SIZE(board_i2c0_devices));
#endif
#ifdef CONFIG_I2C1_RK29
	i2c_register_board_info(default_i2c1_data.bus_num, board_i2c1_devices,
			ARRAY_SIZE(board_i2c1_devices));
#endif
#ifdef CONFIG_I2C2_RK29
	i2c_register_board_info(default_i2c2_data.bus_num, board_i2c2_devices,
			ARRAY_SIZE(board_i2c2_devices));
#endif
#ifdef CONFIG_I2C3_RK29
	i2c_register_board_info(default_i2c3_data.bus_num, board_i2c3_devices,
			ARRAY_SIZE(board_i2c3_devices));
#endif

	spi_register_board_info(board_spi_devices, ARRAY_SIZE(board_spi_devices));
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
