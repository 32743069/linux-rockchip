/* arch/arm/mach-rk30/board-rk30-sdk.c
 *
 * Copyright (C) 2012 ROCKCHIP, Inc.
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
#include <linux/skbuff.h>
#include <linux/spi/spi.h>
#include <linux/mmc/host.h>
#include <linux/ion.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>
#include <asm/hardware/gic.h>

#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/io.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
/*set touchscreen different type header*/
#if defined(CONFIG_TOUCHSCREEN_XPT2046_NORMAL_SPI)
#include "../../../drivers/input/touchscreen/xpt2046_ts.h"
#elif defined(CONFIG_TOUCHSCREEN_XPT2046_TSLIB_SPI)
#include "../../../drivers/input/touchscreen/xpt2046_tslib_ts.h"
#elif defined(CONFIG_TOUCHSCREEN_XPT2046_CBN_SPI)
#include "../../../drivers/input/touchscreen/xpt2046_cbn_ts.h"
#endif
#if defined(CONFIG_SPIM_RK29)
#include "../../../drivers/spi/rk29_spim.h"
#endif
#if defined(CONFIG_ANDROID_TIMED_GPIO)
#include "../../../drivers/staging/android/timed_gpio.h"
#endif

#define RK30_FB0_MEM_SIZE 8*SZ_1M

#ifdef CONFIG_VIDEO_RK
/*---------------- Camera Sensor Macro Define Begin  ------------------------*/
/*---------------- Camera Sensor Configuration Macro Begin ------------------------*/
#define CONFIG_SENSOR_0 RK_CAM_SENSOR_OV2659//RK_CAM_SENSOR_OV5642						/* back camera sensor */
#define CONFIG_SENSOR_IIC_ADDR_0		0x60//0x78
#define CONFIG_SENSOR_IIC_ADAPTER_ID_0	  1
#define CONFIG_SENSOR_CIF_INDEX_0                    0
#define CONFIG_SENSOR_ORIENTATION_0 	  90
#define CONFIG_SENSOR_POWER_PIN_0		  INVALID_GPIO
#define CONFIG_SENSOR_RESET_PIN_0		  INVALID_GPIO
#define CONFIG_SENSOR_POWERDN_PIN_0 	  INVALID_GPIO
#define CONFIG_SENSOR_FALSH_PIN_0		  INVALID_GPIO
#define CONFIG_SENSOR_POWERACTIVE_LEVEL_0 RK_CAM_POWERACTIVE_L
#define CONFIG_SENSOR_RESETACTIVE_LEVEL_0 RK_CAM_RESETACTIVE_L
#define CONFIG_SENSOR_POWERDNACTIVE_LEVEL_0 RK_CAM_POWERDNACTIVE_H
#define CONFIG_SENSOR_FLASHACTIVE_LEVEL_0 RK_CAM_FLASHACTIVE_L

#define CONFIG_SENSOR_QCIF_FPS_FIXED_0		15000
#define CONFIG_SENSOR_QVGA_FPS_FIXED_0		15000
#define CONFIG_SENSOR_CIF_FPS_FIXED_0		15000
#define CONFIG_SENSOR_VGA_FPS_FIXED_0		15000
#define CONFIG_SENSOR_480P_FPS_FIXED_0		15000
#define CONFIG_SENSOR_SVGA_FPS_FIXED_0		15000
#define CONFIG_SENSOR_720P_FPS_FIXED_0		30000

#define CONFIG_SENSOR_1 RK_CAM_SENSOR_OV2659						/* front camera sensor */
#define CONFIG_SENSOR_IIC_ADDR_1		0x60
#define CONFIG_SENSOR_IIC_ADAPTER_ID_1	  1
#define CONFIG_SENSOR_CIF_INDEX_1				  1
#define CONFIG_SENSOR_ORIENTATION_1 	  270
#define CONFIG_SENSOR_POWER_PIN_1		  INVALID_GPIO
#define CONFIG_SENSOR_RESET_PIN_1		  INVALID_GPIO
#define CONFIG_SENSOR_POWERDN_PIN_1 	  INVALID_GPIO
#define CONFIG_SENSOR_FALSH_PIN_1		  INVALID_GPIO
#define CONFIG_SENSOR_POWERACTIVE_LEVEL_1 RK_CAM_POWERACTIVE_L
#define CONFIG_SENSOR_RESETACTIVE_LEVEL_1 RK_CAM_RESETACTIVE_L
#define CONFIG_SENSOR_POWERDNACTIVE_LEVEL_1 RK_CAM_POWERDNACTIVE_H
#define CONFIG_SENSOR_FLASHACTIVE_LEVEL_1 RK_CAM_FLASHACTIVE_L

#define CONFIG_SENSOR_QCIF_FPS_FIXED_1		15000
#define CONFIG_SENSOR_QVGA_FPS_FIXED_1		15000
#define CONFIG_SENSOR_CIF_FPS_FIXED_1		15000
#define CONFIG_SENSOR_VGA_FPS_FIXED_1		15000
#define CONFIG_SENSOR_480P_FPS_FIXED_1		15000
#define CONFIG_SENSOR_SVGA_FPS_FIXED_1		15000
#define CONFIG_SENSOR_720P_FPS_FIXED_1		30000

#define CONFIG_USE_CIF_0	1
#define CONFIG_USE_CIF_1      0
#endif	//#ifdef CONFIG_VIDEO_RK29
/*---------------- Camera Sensor Configuration Macro End------------------------*/
#include "../../../drivers/media/video/rk_camera.c"
/*---------------- Camera Sensor Macro Define End  ---------*/

#define PMEM_CAM_SIZE		PMEM_CAM_NECESSARY
#ifdef CONFIG_VIDEO_RK_WORK_IPP
#define MEM_CAMIPP_SIZE 	PMEM_CAMIPP_NECESSARY
#else
#define MEM_CAMIPP_SIZE 	0
#endif
/*****************************************************************************************
 * camera  devices
 * author: ddl@rock-chips.com
 *****************************************************************************************/
#ifdef CONFIG_VIDEO_RK
#define CONFIG_SENSOR_POWER_IOCTL_USR	   0
#define CONFIG_SENSOR_RESET_IOCTL_USR	   0
#define CONFIG_SENSOR_POWERDOWN_IOCTL_USR	   0
#define CONFIG_SENSOR_FLASH_IOCTL_USR	   0

#if CONFIG_SENSOR_POWER_IOCTL_USR
static int sensor_power_usr_cb (struct rkcamera_gpio_res *res,int on)
{
	#error "CONFIG_SENSOR_POWER_IOCTL_USR is 1, sensor_power_usr_cb function must be writed!!";
}
#endif

#if CONFIG_SENSOR_RESET_IOCTL_USR
static int sensor_reset_usr_cb (struct rkcamera_gpio_res *res,int on)
{
	#error "CONFIG_SENSOR_RESET_IOCTL_USR is 1, sensor_reset_usr_cb function must be writed!!";
}
#endif

#if CONFIG_SENSOR_POWERDOWN_IOCTL_USR
static int sensor_powerdown_usr_cb (struct rkcamera_gpio_res *res,int on)
{
	#error "CONFIG_SENSOR_POWERDOWN_IOCTL_USR is 1, sensor_powerdown_usr_cb function must be writed!!";
}
#endif

#if CONFIG_SENSOR_FLASH_IOCTL_USR
static int sensor_flash_usr_cb (struct rkcamera_gpio_res *res,int on)
{
	#error "CONFIG_SENSOR_FLASH_IOCTL_USR is 1, sensor_flash_usr_cb function must be writed!!";
}
#endif

static struct rkcamera_platform_ioctl_cb	sensor_ioctl_cb = {
	#if CONFIG_SENSOR_POWER_IOCTL_USR
	.sensor_power_cb = sensor_power_usr_cb,
	#else
	.sensor_power_cb = NULL,
	#endif

	#if CONFIG_SENSOR_RESET_IOCTL_USR
	.sensor_reset_cb = sensor_reset_usr_cb,
	#else
	.sensor_reset_cb = NULL,
	#endif

	#if CONFIG_SENSOR_POWERDOWN_IOCTL_USR
	.sensor_powerdown_cb = sensor_powerdown_usr_cb,
	#else
	.sensor_powerdown_cb = NULL,
	#endif

	#if CONFIG_SENSOR_FLASH_IOCTL_USR
	.sensor_flash_cb = sensor_flash_usr_cb,
	#else
	.sensor_flash_cb = NULL,
	#endif
};
static struct reginfo_t rk_init_data_sensor_reg_0[] =
{
		{0x3000, 0x0f,0,0},
		{0x3001, 0xff,0,0},
		{0x3002, 0xff,0,0},
		//{0x0100, 0x01},	//software sleep : Sensor vsync singal may not output if haven't sleep the sensor when transfer the array
		{0x3633, 0x3d,0,0},
		{0x3620, 0x02,0,0},
		{0x3631, 0x11,0,0},
		{0x3612, 0x04,0,0},
		{0x3630, 0x20,0,0},
		{0x4702, 0x02,0,0},
		{0x370c, 0x34,0,0},
		{0x3004, 0x10,0,0},
		{0x3005, 0x18,0,0},
		{0x3800, 0x00,0,0},
		{0x3801, 0x00,0,0},
		{0x3802, 0x00,0,0},
		{0x3803, 0x00,0,0},
		{0x3804, 0x06,0,0},
		{0x3805, 0x5f,0,0},
		{0x3806, 0x04,0,0},
		{0x3807, 0xb7,0,0},
		{0x3808, 0x03,0,0},
		{0x3809, 0x20,0,0},
		{0x380a, 0x02,0,0},
		{0x380b, 0x58,0,0},
		{0x380c, 0x05,0,0},
		{0x380d, 0x14,0,0},
		{0x380e, 0x02,0,0},
		{0x380f, 0x68,0,0},
		{0x3811, 0x08,0,0},
		{0x3813, 0x02,0,0},
		{0x3814, 0x31,0,0},
		{0x3815, 0x31,0,0},
		{0x3a02, 0x02,0,0},
		{0x3a03, 0x68,0,0},
		{0x3a08, 0x00,0,0},
		{0x3a09, 0x5c,0,0},
		{0x3a0a, 0x00,0,0},
		{0x3a0b, 0x4d,0,0},
		{0x3a0d, 0x08,0,0},
		{0x3a0e, 0x06,0,0},
		{0x3a14, 0x02,0,0},
		{0x3a15, 0x28,0,0},
			{0x4708, 0x01,0,0},
		{0x3623, 0x00,0,0},
		{0x3634, 0x76,0,0},
		{0x3701, 0x44,0,0},
		{0x3702, 0x18,0,0},
		{0x3703, 0x24,0,0},
		{0x3704, 0x24,0,0},
		{0x3705, 0x0c,0,0},
		{0x3820, 0x81,0,0},
		{0x3821, 0x01,0,0},
		{0x370a, 0x52,0,0},
		{0x4608, 0x00,0,0},
		{0x4609, 0x80,0,0},
		{0x4300, 0x32,0,0},
		{0x5086, 0x02,0,0},
		{0x5000, 0xfb,0,0},
		{0x5001, 0x1f,0,0},
		{0x5002, 0x00,0,0},
		{0x5025, 0x0e,0,0},
		{0x5026, 0x18,0,0},
		{0x5027, 0x34,0,0},
		{0x5028, 0x4c,0,0},
		{0x5029, 0x62,0,0},
		{0x502a, 0x74,0,0},
		{0x502b, 0x85,0,0},
		{0x502c, 0x92,0,0},
		{0x502d, 0x9e,0,0},
		{0x502e, 0xb2,0,0},
		{0x502f, 0xc0,0,0},
		{0x5030, 0xcc,0,0},
		{0x5031, 0xe0,0,0},
		{0x5032, 0xee,0,0},
		{0x5033, 0xf6,0,0},
		{0x5034, 0x11,0,0},
		{0x5070, 0x1c,0,0},
		{0x5071, 0x5b,0,0},
		{0x5072, 0x05,0,0},
		{0x5073, 0x20,0,0},
		{0x5074, 0x94,0,0},
		{0x5075, 0xb4,0,0},
		{0x5076, 0xb4,0,0},
		{0x5077, 0xaf,0,0},
		{0x5078, 0x05,0,0},
		{0x5079, 0x98,0,0},
		{0x507a, 0x21,0,0},
		{0x5035, 0x6a,0,0},
		{0x5036, 0x11,0,0},
		{0x5037, 0x92,0,0},
		{0x5038, 0x21,0,0},
	
		{0x5039, 0xe1,0,0},
		{0x503a, 0x01,0,0},
		{0x503c, 0x05,0,0},
		{0x503d, 0x08,0,0},
		{0x503e, 0x08,0,0},
		{0x503f, 0x64,0,0},
		{0x5040, 0x58,0,0},
		{0x5041, 0x2a,0,0},
		{0x5042, 0xc5,0,0},
		{0x5043, 0x2e,0,0},
		{0x5044, 0x3a,0,0},
		{0x5045, 0x3c,0,0},
		{0x5046, 0x44,0,0},
		{0x5047, 0xf8,0,0},
		{0x5048, 0x08,0,0},
		{0x5049, 0x70,0,0},
		{0x504a, 0xf0,0,0},
		{0x504b, 0xf0,0,0},
		{0x500c, 0x03,0,0},
		{0x500d, 0x20,0,0},
		{0x500e, 0x02,0,0},
		{0x500f, 0x5c,0,0},
		{0x5010, 0x48,0,0},
		{0x5011, 0x00,0,0},
		{0x5012, 0x66,0,0},
		{0x5013, 0x03,0,0},
		{0x5014, 0x30,0,0},
		{0x5015, 0x02,0,0},
		{0x5016, 0x7c,0,0},
		{0x5017, 0x40,0,0},
		{0x5018, 0x00,0,0},
		{0x5019, 0x66,0,0},
		{0x501a, 0x03,0,0},
		{0x501b, 0x10,0,0},
		{0x501c, 0x02,0,0},
		{0x501d, 0x7c,0,0},
		{0x501e, 0x3a,0,0},
		{0x501f, 0x00,0,0},
		{0x5020, 0x66,0,0},
		{0x506e, 0x44,0,0},
		{0x5064, 0x08,0,0},
		{0x5065, 0x10,0,0},
		{0x5066, 0x12,0,0},
		{0x5067, 0x02,0,0},
		{0x506c, 0x08,0,0},
		{0x506d, 0x10,0,0},
		{0x506f, 0xa6,0,0},
		{0x5068, 0x08,0,0},
	
	
		{0x5069, 0x10,0,0},
		{0x506a, 0x04,0,0},
		{0x506b, 0x12,0,0},
		{0x507e, 0x40,0,0},
		{0x507f, 0x20,0,0},
		{0x507b, 0x02,0,0},
		{0x507a, 0x01,0,0},
		{0x5084, 0x0c,0,0},
		{0x5085, 0x3e,0,0},
		{0x5005, 0x80,0,0},
		{0x3a0f, 0x30,0,0},
		{0x3a10, 0x28,0,0},
		{0x3a1b, 0x32,0,0},
		{0x3a1e, 0x26,0,0},
		{0x3a11, 0x60,0,0},
		{0x3a1f, 0x14,0,0},
		{0x5060, 0x69,0,0},
		{0x5061, 0x7d,0,0},
		{0x5062, 0x7d,0,0},
		{0x5063, 0x69,0,0},
		{0x3004, 0x20,0,0},
			{0x0100, 0x01,0,0},
		{0x0000, 0x00,0,0}
	};
static struct reginfo_t rk_init_data_sensor_winseqreg_0[] ={
	{0x0000, 0x00,0,0}
	};
static rk_sensor_user_init_data_s rk_init_data_sensor_0 = 
{	
	.rk_sensor_init_width = INVALID_VALUE,
	.rk_sensor_init_height = INVALID_VALUE,
	.rk_sensor_init_bus_param = INVALID_VALUE,
	.rk_sensor_init_pixelcode = INVALID_VALUE,
	.rk_sensor_init_data = rk_init_data_sensor_reg_0,
	.rk_sensor_init_winseq = NULL,//rk_init_data_sensor_winseqreg_0,
	.rk_sensor_winseq_size = sizeof(rk_init_data_sensor_winseqreg_0) / sizeof(struct reginfo_t),
	
};
static rk_sensor_user_init_data_s* rk_init_data_sensor_0_p = &rk_init_data_sensor_0;
static rk_sensor_user_init_data_s* rk_init_data_sensor_1_p = NULL;
#include "../../../drivers/media/video/rk_camera.c"

#endif

#if defined(CONFIG_TOUCHSCREEN_GT8XX)
#define TOUCH_RESET_PIN  RK30_PIN4_PD0
#define TOUCH_PWR_PIN    INVALID_GPIO
int goodix_init_platform_hw(void)
{
	int ret;
	printk("goodix_init_platform_hw\n");
	if(TOUCH_PWR_PIN != INVALID_GPIO)
	{
		ret = gpio_request(TOUCH_PWR_PIN, "goodix power pin");
		if(ret != 0){
			gpio_free(TOUCH_PWR_PIN);
			printk("goodix power error\n");
			return -EIO;
		}
		gpio_direction_output(TOUCH_PWR_PIN, 0);
		gpio_set_value(TOUCH_PWR_PIN,GPIO_LOW);
		msleep(100);
	}
	
	if(TOUCH_RESET_PIN != INVALID_GPIO)
	{
		ret = gpio_request(TOUCH_RESET_PIN, "goodix reset pin");
		if(ret != 0){
			gpio_free(TOUCH_RESET_PIN);
			printk("goodix gpio_request error\n");
			return -EIO;
		}
		gpio_direction_output(TOUCH_RESET_PIN, 0);
		gpio_set_value(TOUCH_RESET_PIN,GPIO_LOW);
		msleep(10);
		gpio_set_value(TOUCH_RESET_PIN,GPIO_HIGH);
		msleep(500);
	}
	return 0;
}

struct goodix_platform_data goodix_info = {
	  .model= 8105,
	  .irq_pin = RK30_PIN4_PC2,
	  .rest_pin  = TOUCH_RESET_PIN,
	  .init_platform_hw = goodix_init_platform_hw,
};
#endif


/*****************************************************************************************
 * xpt2046 touch panel
 * author: hhb@rock-chips.com
 *****************************************************************************************/
#if defined(CONFIG_TOUCHSCREEN_XPT2046_NORMAL_SPI) || defined(CONFIG_TOUCHSCREEN_XPT2046_TSLIB_SPI)
#define XPT2046_GPIO_INT	RK30_PIN4_PC2 
#define DEBOUNCE_REPTIME  	3


static struct xpt2046_platform_data xpt2046_info = {
	.model			= 2046,
	.keep_vref_on 		= 1,
	.swap_xy		= 0,
	.debounce_max		= 7,
	.debounce_rep		= DEBOUNCE_REPTIME,
	.debounce_tol		= 20,
	.gpio_pendown		= XPT2046_GPIO_INT,
	.pendown_iomux_name = GPIO4C2_SMCDATA2_TRACEDATA2_NAME,	
	.pendown_iomux_mode = GPIO4C_GPIO4C2,	
	.touch_virtualkey_length = 60,
	.penirq_recheck_delay_usecs = 1,
#if defined(CONFIG_TOUCHSCREEN_480X800)
	.x_min			= 0,
	.x_max			= 480,
	.y_min			= 0,
	.y_max			= 800,
	.touch_ad_top = 3940,
	.touch_ad_bottom = 310,
	.touch_ad_left = 3772,
	.touch_ad_right = 340,
#elif defined(CONFIG_TOUCHSCREEN_800X480)
	.x_min			= 0,
	.x_max			= 800,
	.y_min			= 0,
	.y_max			= 480,
	.touch_ad_top = 2447,
	.touch_ad_bottom = 207,
	.touch_ad_left = 5938,
	.touch_ad_right = 153,
#elif defined(CONFIG_TOUCHSCREEN_320X480)
	.x_min			= 0,
	.x_max			= 320,
	.y_min			= 0,
	.y_max			= 480,
	.touch_ad_top = 3166,
	.touch_ad_bottom = 256,
	.touch_ad_left = 3658,
	.touch_ad_right = 380,
#endif	
};
#elif defined(CONFIG_TOUCHSCREEN_XPT2046_CBN_SPI)
static struct xpt2046_platform_data xpt2046_info = {
	.model			= 2046,
	.keep_vref_on 	= 1,
	.swap_xy		= 0,
	.debounce_max		= 7,
	.debounce_rep		= DEBOUNCE_REPTIME,
	.debounce_tol		= 20,
	.gpio_pendown		= XPT2046_GPIO_INT,
	.pendown_iomux_name = GPIO4C2_SMCDATA2_TRACEDATA2_NAME,	
	.pendown_iomux_mode = GPIO4C_GPIO4C2,	
	.touch_virtualkey_length = 60,
	.penirq_recheck_delay_usecs = 1,
	
#if defined(CONFIG_TOUCHSCREEN_480X800)
	.x_min			= 0,
	.x_max			= 480,
	.y_min			= 0,
	.y_max			= 800,
	.screen_x = { 70,  410, 70, 410, 240},
	.screen_y = { 50, 50,  740, 740, 400},
	.uncali_x_default = {  3267,  831, 3139, 715, 1845 },
	.uncali_y_default = { 3638,  3664, 564,  591, 2087 },
#elif defined(CONFIG_TOUCHSCREEN_800X480)
	.x_min			= 0,
	.x_max			= 800,
	.y_min			= 0,
	.y_max			= 480,
	.screen_x[5] = { 50, 750,  50, 750, 400};
  	.screen_y[5] = { 40,  40, 440, 440, 240};
	.uncali_x_default[5] = { 438,  565, 3507,  3631, 2105 };
	.uncali_y_default[5] = {  3756,  489, 3792, 534, 2159 };
#elif defined(CONFIG_TOUCHSCREEN_320X480)
	.x_min			= 0,
	.x_max			= 320,
	.y_min			= 0,
	.y_max			= 480,
	.screen_x[5] = { 50, 270,  50, 270, 160}; 
	.screen_y[5] = { 40,  40, 440, 440, 240}; 
	.uncali_x_default[5] = { 812,  3341, 851,  3371, 2183 };
	.uncali_y_default[5] = {  442,  435, 3193, 3195, 2004 };
#endif	
};
#endif
#if defined(CONFIG_TOUCHSCREEN_XPT2046_SPI)
static struct rk29xx_spi_chip xpt2046_chip = {
	//.poll_mode = 1,
	.enable_dma = 1,
};
#endif
static struct spi_board_info board_spi_devices[] = {
#if defined(CONFIG_TOUCHSCREEN_XPT2046_SPI)
	{
		.modalias	= "xpt2046_ts",
		.chip_select	= 1,// 2,
		.max_speed_hz	= 1 * 1000 * 800,/* (max sample rate @ 3V) * (cmd + data + overhead) */
		.bus_num	= 0,
		.irq 		= XPT2046_GPIO_INT,
		.platform_data = &xpt2046_info,
		.controller_data = &xpt2046_chip,
	},
#endif

};


/***********************************************************
*	rk30  backlight
************************************************************/
#ifdef CONFIG_BACKLIGHT_RK29_BL
#define PWM_ID            0
#define PWM_MUX_NAME      GPIO0A3_PWM0_NAME
#define PWM_MUX_MODE      GPIO0A_PWM0
#define PWM_MUX_MODE_GPIO GPIO0A_GPIO0A3
#define PWM_GPIO 	  RK30_PIN0_PA3
#define PWM_EFFECT_VALUE  1

#define LCD_DISP_ON_PIN

#ifdef  LCD_DISP_ON_PIN
//#define BL_EN_MUX_NAME    GPIOF34_UART3_SEL_NAME
//#define BL_EN_MUX_MODE    IOMUXB_GPIO1_B34

#define BL_EN_PIN         RK30_PIN6_PB3
#define BL_EN_VALUE       GPIO_HIGH
#endif
static int rk29_backlight_io_init(void)
{
	int ret = 0;
	rk30_mux_api_set(PWM_MUX_NAME, PWM_MUX_MODE);
#ifdef  LCD_DISP_ON_PIN
	// rk30_mux_api_set(BL_EN_MUX_NAME, BL_EN_MUX_MODE);

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
	rk30_mux_api_set(PWM_MUX_NAME, PWM_MUX_MODE_GPIO);   
    return ret;
}

static int rk29_backlight_pwm_suspend(void)
{
	int ret = 0;
	rk30_mux_api_set(PWM_MUX_NAME, PWM_MUX_MODE_GPIO);
	if (gpio_request(PWM_GPIO, NULL)) {
		printk("func %s, line %d: request gpio fail\n", __FUNCTION__, __LINE__);
		return -1;
	}
	gpio_direction_output(PWM_GPIO, GPIO_LOW);
#ifdef  LCD_DISP_ON_PIN
	gpio_direction_output(BL_EN_PIN, 0);
	gpio_set_value(BL_EN_PIN, !BL_EN_VALUE);
#endif
	return ret;
}

static int rk29_backlight_pwm_resume(void)
{
	gpio_free(PWM_GPIO);
	rk30_mux_api_set(PWM_MUX_NAME, PWM_MUX_MODE);
#ifdef  LCD_DISP_ON_PIN
	msleep(30);
	gpio_direction_output(BL_EN_PIN, 1);
	gpio_set_value(BL_EN_PIN, BL_EN_VALUE);
#endif
	return 0;
}

static struct rk29_bl_info rk29_bl_info = {
    .pwm_id   = PWM_ID,
    .bl_ref   = PWM_EFFECT_VALUE,
    .io_init   = rk29_backlight_io_init,
    .io_deinit = rk29_backlight_io_deinit,
    .pwm_suspend = rk29_backlight_pwm_suspend,
    .pwm_resume = rk29_backlight_pwm_resume,
};


static struct platform_device rk29_device_backlight = {
	.name	= "rk29_backlight",
	.id 	= -1,
        .dev    = {
           .platform_data  = &rk29_bl_info,
        }
};

#endif

/*MMA8452 gsensor*/
#if defined (CONFIG_GS_MMA8452)
#define MMA8452_INT_PIN   RK30_PIN4_PC0

static int mma8452_init_platform_hw(void)
{
	rk30_mux_api_set(GPIO4C0_SMCDATA0_TRACEDATA0_NAME, GPIO4C_GPIO4C0);

	if(gpio_request(MMA8452_INT_PIN,NULL) != 0){
		gpio_free(MMA8452_INT_PIN);
		printk("mma8452_init_platform_hw gpio_request error\n");
		return -EIO;
	}
	gpio_pull_updown(MMA8452_INT_PIN, 1);
	return 0;
}


static struct mma8452_platform_data mma8452_info = {
	.model= 8452,
	.swap_xy = 0,
	.swap_xyz = 1,
	.init_platform_hw= mma8452_init_platform_hw,
	.orientation = { -1, 0, 0, 0, 0, 1, 0, -1, 0},
};
#endif
#if defined (CONFIG_COMPASS_AK8975)
static struct akm8975_platform_data akm8975_info =
{
	.m_layout = 
	{
		{
			{1, 0, 0 },
			{0, -1, 0 },
			{0,	0, -1 },
		},

		{
			{1, 0, 0 },
			{0, 1, 0 },
			{0,	0, 1 },
		},

		{
			{1, 0, 0 },
			{0, 1, 0 },
			{0,	0, 1 },
		},

		{
			{1, 0, 0 },
			{0, 1, 0 },
			{0,	0, 1 },
		},
	}

};

#endif

#if defined(CONFIG_GYRO_L3G4200D)

#include <linux/l3g4200d.h>
#define L3G4200D_INT_PIN  RK30_PIN4_PC3

static int l3g4200d_init_platform_hw(void)
{
	if (gpio_request(L3G4200D_INT_PIN, NULL) != 0) {
		gpio_free(L3G4200D_INT_PIN);
		printk("%s: request l3g4200d int pin error\n", __func__);
		return -EIO;
	}
	gpio_pull_updown(L3G4200D_INT_PIN, 1);
	return 0;
}

static struct l3g4200d_platform_data l3g4200d_info = {
	.fs_range = 1,

	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,

	.negate_x = 1,
	.negate_y = 1,
	.negate_z = 0,

	.init = l3g4200d_init_platform_hw,
};

#endif

#ifdef CONFIG_LS_CM3217

#define CM3217_POWER_PIN 	INVALID_GPIO
#define CM3217_IRQ_PIN		INVALID_GPIO
static int cm3217_init_hw(void)
{
#if 0
	if (gpio_request(CM3217_POWER_PIN, NULL) != 0) {
	gpio_free(CM3217_POWER_PIN);
	printk("%s: request cm3217 power pin error\n", __func__);
	return -EIO;
	}
	gpio_pull_updown(CM3217_POWER_PIN, PullDisable);

	if (gpio_request(CM3217_IRQ_PIN, NULL) != 0) {
	gpio_free(CM3217_IRQ_PIN);
	printk("%s: request cm3217 int pin error\n", __func__);
	return -EIO;
	}
	gpio_pull_updown(CM3217_IRQ_PIN, PullDisable);
#endif
	return 0;
}

static void cm3217_exit_hw(void)
{
#if 0
	gpio_free(CM3217_POWER_PIN);
	gpio_free(CM3217_IRQ_PIN);
#endif
	return;
}

struct cm3217_platform_data cm3217_info = {
	.irq_pin = CM3217_IRQ_PIN,
	.power_pin = CM3217_POWER_PIN,
	.init_platform_hw = cm3217_init_hw,
	.exit_platform_hw = cm3217_exit_hw,
};
#endif



#ifdef CONFIG_FB_ROCKCHIP
static struct resource resource_fb[] = {
	[0] = {
		.name  = "fb0 buf",
		.start = 0,
		.end   = 0,//RK30_FB0_MEM_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name  = "ipp buf",  //for rotate
		.start = 0,
		.end   = 0,//RK30_FB0_MEM_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.name  = "fb2 buf",
		.start = 0,
		.end   = 0,//RK30_FB0_MEM_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device device_fb = {
	.name		  = "rk-fb",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(resource_fb),
	.resource	  = resource_fb,
};
#endif

#ifdef CONFIG_ANDROID_TIMED_GPIO
static struct timed_gpio timed_gpios[] = {
	{
		.name = "vibrator",
		.gpio = RK30_PIN0_PA4,
		.max_timeout = 1000,
		.active_low = 0,
		.adjust_time =20,      //adjust for diff product
	},
};

struct timed_gpio_platform_data rk29_vibrator_info = {
	.num_gpios = 1,
	.gpios = timed_gpios,
};

struct platform_device rk29_device_vibrator ={
	.name = "timed-gpio",
	.id = -1,
	.dev = {
		.platform_data = &rk29_vibrator_info,
		},

};
#endif 

#ifdef CONFIG_LEDS_GPIO_PLATFORM
struct gpio_led rk29_leds[] = {
		{
			.name = "button-backlight",
			.gpio = RK30_PIN4_PD7,
			.default_trigger = "timer",
			.active_low = 0,
			.retain_state_suspended = 0,
			.default_state = LEDS_GPIO_DEFSTATE_OFF,
		},
};

struct gpio_led_platform_data rk29_leds_pdata = {
	.leds = &rk29_leds,
	.num_leds	= ARRAY_SIZE(rk29_leds),
};

struct platform_device rk29_device_gpio_leds = {
	.name	= "leds-gpio",
	.id 	= -1,
	.dev	= {
	   .platform_data  = &rk29_leds_pdata,
	},
};
#endif

#ifdef CONFIG_RK_IRDA
#define IRDA_IRQ_PIN           RK30_PIN6_PA1

int irda_iomux_init(void)
{
	int ret = 0;

	//irda irq pin
	ret = gpio_request(IRDA_IRQ_PIN, NULL);
	if(ret != 0)
	{
	gpio_free(IRDA_IRQ_PIN);
	printk(">>>>>> IRDA_IRQ_PIN gpio_request err \n ");
	}
	gpio_pull_updown(IRDA_IRQ_PIN, PullDisable);
	gpio_direction_input(IRDA_IRQ_PIN);

	return 0;
}

int irda_iomux_deinit(void)
{
	gpio_free(IRDA_IRQ_PIN);
	return 0;
}

static struct irda_info rk29_irda_info = {
	.intr_pin = IRDA_IRQ_PIN,
	.iomux_init = irda_iomux_init,
	.iomux_deinit = irda_iomux_deinit,
	//.irda_pwr_ctl = bu92747guw_power_ctl,
};

static struct platform_device irda_device = {
#ifdef CONFIG_RK_IRDA_NET
			.name	= "rk_irda",
#else
			.name = "bu92747_irda",
#endif
    .id		  = -1,
	.dev            = {
		.platform_data  = &rk29_irda_info,
	}
};
#endif




static struct platform_device *devices[] __initdata = {
#ifdef CONFIG_BACKLIGHT_RK29_BL
	&rk29_device_backlight,
#endif	
#ifdef CONFIG_FB_ROCKCHIP
	&device_fb,
#endif
#ifdef CONFIG_ANDROID_TIMED_GPIO
	&rk29_device_vibrator,
#endif
#ifdef CONFIG_LEDS_GPIO_PLATFORM
	&rk29_device_gpio_leds,
#endif
#ifdef CONFIG_RK_IRDA
	&irda_device,
#endif


};

// i2c
#ifdef CONFIG_I2C0_RK30
static struct i2c_board_info __initdata i2c0_info[] = {
#if defined (CONFIG_GS_MMA8452)
	    {
	      .type	      = "gs_mma8452",
	      .addr	      = 0x1c,
	      .flags	      = 0,
	      .irq	      = MMA8452_INT_PIN,
	      .platform_data  = &mma8452_info,
	    },
#endif
#if defined (CONFIG_COMPASS_AK8975)
	{
		.type    	= "ak8975",
		.addr           = 0x0d,
		.flags		= 0,
		.irq		= RK30_PIN4_PC1,
		.platform_data  = &akm8975_info,
	},
#endif
#if defined (CONFIG_GYRO_L3G4200D)
	{
		.type           = "l3g4200d_gryo",
		.addr           = 0x69,
		.flags          = 0,
		.irq            = L3G4200D_INT_PIN,
		.platform_data  = &l3g4200d_info,
	},
#endif

#if defined (CONFIG_SND_SOC_RK1000)
	{
		.type    		= "rk1000_i2c_codec",
		.addr           = 0x60,
		.flags			= 0,
	},
	{
		.type			= "rk1000_control",
		.addr			= 0x40,
		.flags			= 0,
	},	
#endif
};
#endif

#ifdef CONFIG_I2C1_RK30
static struct i2c_board_info __initdata i2c1_info[] = {
};
#endif

#ifdef CONFIG_I2C2_RK30
static struct i2c_board_info __initdata i2c2_info[] = {
#if defined (CONFIG_TOUCHSCREEN_GT8XX)
		    {
				.type	= "Goodix-TS",
				.addr	= 0x55,
				.flags	    =0,
				.irq		=RK30_PIN4_PC2,
				.platform_data = &goodix_info,
		    },
#endif
#if defined (CONFIG_LS_CM3217)
	{
		.type           = "lightsensor",
		.addr           = 0x20,
		.flags          = 0,
		.irq            = CM3217_IRQ_PIN,
		.platform_data  = &cm3217_info,
	},
#endif

};
#endif

#ifdef CONFIG_I2C3_RK30
static struct i2c_board_info __initdata i2c3_info[] = {
};
#endif

#ifdef CONFIG_I2C4_RK30
static struct i2c_board_info __initdata i2c4_info[] = {
};
#endif

static void __init rk30_i2c_register_board_info(void)
{
#ifdef CONFIG_I2C0_RK30
	i2c_register_board_info(0, i2c0_info, ARRAY_SIZE(i2c0_info));
#endif
#ifdef CONFIG_I2C1_RK30
	i2c_register_board_info(1, i2c1_info, ARRAY_SIZE(i2c1_info));
#endif
#ifdef CONFIG_I2C2_RK30
	i2c_register_board_info(2, i2c2_info, ARRAY_SIZE(i2c2_info));
#endif
#ifdef CONFIG_I2C3_RK30
	i2c_register_board_info(3, i2c3_info, ARRAY_SIZE(i2c3_info));
#endif
#ifdef CONFIG_I2C4_RK30
	i2c_register_board_info(4, i2c4_info, ARRAY_SIZE(i2c4_info));
#endif
}
//end of i2c

static void __init machine_rk30_board_init(void)
{
	rk30_i2c_register_board_info();
	spi_register_board_info(board_spi_devices, ARRAY_SIZE(board_spi_devices));
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

static void __init rk30_reserve(void)
{
#ifdef CONFIG_FB_ROCKCHIP
	resource_fb[0].start = board_mem_reserve_add("fb0",RK30_FB0_MEM_SIZE);
	resource_fb[0].end = resource_fb[0].start + RK30_FB0_MEM_SIZE - 1;
	resource_fb[1].start = board_mem_reserve_add("ipp buf",RK30_FB0_MEM_SIZE);
	resource_fb[1].end = resource_fb[1].start + RK30_FB0_MEM_SIZE - 1;
	resource_fb[2].start = board_mem_reserve_add("fb2",RK30_FB0_MEM_SIZE);
	resource_fb[2].end = resource_fb[2].start + RK30_FB0_MEM_SIZE - 1;	
#endif

#if (MEM_CAMIPP_SIZE != 0)
	#if CONFIG_USE_CIF_0
	rk_camera_platform_data_host_0.meminfo.name = "camera_ipp_mem_0";
	rk_camera_platform_data_host_0.meminfo.start = board_mem_reserve_add("camera_ipp_mem_0",MEM_CAMIPP_SIZE);
	rk_camera_platform_data_host_0.meminfo.size= MEM_CAMIPP_SIZE;
	#endif
	#if CONFIG_USE_CIF_1
	rk_camera_platform_data_host_1.meminfo.name = "camera_ipp_mem_1";
	rk_camera_platform_data_host_1.meminfo.start =board_mem_reserve_add("camera_ipp_mem_1",MEM_CAMIPP_SIZE);
	rk_camera_platform_data_host_1.meminfo.size= MEM_CAMIPP_SIZE;
	#endif
#endif

#if (PMEM_CAM_SIZE != 0)
	android_pmem_cam_pdata.start = board_mem_reserve_add("camera_pmem",PMEM_CAM_SIZE);
	android_pmem_cam_pdata.size = PMEM_CAM_SIZE;
#endif

	board_mem_reserved();
}

MACHINE_START(RK30, "RK30board")
	.boot_params	= PLAT_PHYS_OFFSET + 0x800,
	.fixup		= rk30_fixup,
	.reserve	= &rk30_reserve,
	.map_io		= rk30_map_io,
	.init_irq	= rk30_init_irq,
	.timer		= &rk30_timer,
	.init_machine	= machine_rk30_board_init,
MACHINE_END
