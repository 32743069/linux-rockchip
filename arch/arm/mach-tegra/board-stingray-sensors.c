/*
 * Copyright (c) 2010, Motorola, All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kxtf9.h>
#include <linux/l3g4200d.h>
#include <linux/led-lm3559.h>
#include <linux/max9635.h>
#include <linux/moto_bmp085.h>
#include <media/ov5650.h>
#include <media/soc2030.h>
#include <linux/platform_device.h>

#include <linux/regulator/consumer.h>

#include <mach/gpio.h>

#include "board-stingray.h"
#include "gpio-names.h"

#define KXTF9_IRQ_GPIO		TEGRA_GPIO_PV3
#define MAX9635_IRQ_GPIO	TEGRA_GPIO_PV1
#define BMP085_IRQ_GPIO		TEGRA_GPIO_PW0
#define L3G4200D_IRQ_GPIO	TEGRA_GPIO_PH2
#define AKM8975_IRQ_GPIO	TEGRA_GPIO_PQ2
#define LM3559_RESETN_GPIO	TEGRA_GPIO_PT4
#define OV5650_RESETN_GPIO	TEGRA_GPIO_PD2
#define OV5650_PWRDN_GPIO	TEGRA_GPIO_PBB1
#define SOC2030_RESETN_GPIO	TEGRA_GPIO_PD5
#define SOC2030_PWRDN_GPIO	TEGRA_GPIO_PBB5

static int stingray_ov5650_power_on(void)
{
	msleep(20);

	gpio_direction_output(OV5650_PWRDN_GPIO, 0);
	msleep(10);

	gpio_direction_output(OV5650_RESETN_GPIO, 1);
	msleep(5);
	gpio_direction_output(OV5650_RESETN_GPIO, 0);
	msleep(5);
	gpio_direction_output(OV5650_RESETN_GPIO, 1);
	msleep(5);

	return 0;
}

static int stingray_ov5650_power_off(void)
{
	gpio_direction_output(OV5650_PWRDN_GPIO, 1);
	gpio_direction_output(OV5650_RESETN_GPIO, 0);

	return 0;
}

struct ov5650_platform_data stingray_ov5650_data = {
	.power_on = stingray_ov5650_power_on,
	.power_off = stingray_ov5650_power_off,
	.ignore_otp = false
};

static int stingray_ov5650_init(void)
{
	tegra_gpio_enable(OV5650_RESETN_GPIO);
	gpio_request(OV5650_RESETN_GPIO, "ov5650_reset");
	gpio_direction_output(OV5650_RESETN_GPIO, 0);
	gpio_export(OV5650_RESETN_GPIO, false);

	tegra_gpio_enable(OV5650_PWRDN_GPIO);
	gpio_request(OV5650_PWRDN_GPIO, "ov5650_pwrdn");
	gpio_direction_output(OV5650_PWRDN_GPIO, 1);
	gpio_export(OV5650_PWRDN_GPIO, false);

	if (stingray_revision() <= STINGRAY_REVISION_P1) {
		stingray_ov5650_data.ignore_otp = true;
		pr_info("running on old hardware, ignoring OTP data\n");
	}

	pr_info("initialize the ov5650 sensor\n");

	return 0;
}

static int stingray_soc2030_init(void)
{
	tegra_gpio_enable(SOC2030_RESETN_GPIO);
	gpio_request(SOC2030_RESETN_GPIO, "soc2030_reset");
	gpio_direction_output(SOC2030_RESETN_GPIO, 0);
	gpio_export(SOC2030_RESETN_GPIO, false);

	tegra_gpio_enable(SOC2030_PWRDN_GPIO);
	gpio_request(SOC2030_PWRDN_GPIO, "soc2030_pwrdn");
	gpio_direction_output(SOC2030_PWRDN_GPIO, 1);
	gpio_export(SOC2030_PWRDN_GPIO, false);

	pr_info("initialize the soc2030 sensor\n");

	return 0;
}

static int stingray_soc2030_power_on(void)
{
	gpio_direction_output(SOC2030_PWRDN_GPIO, 0);
	msleep(10);

	gpio_direction_output(SOC2030_RESETN_GPIO, 1);
	msleep(5);
	gpio_direction_output(SOC2030_RESETN_GPIO, 0);
	msleep(5);
	gpio_direction_output(SOC2030_RESETN_GPIO, 1);
	msleep(5);

	return 0;
}

static int stingray_soc2030_power_off(void)
{
	gpio_direction_output(SOC2030_RESETN_GPIO, 0);
	gpio_direction_output(SOC2030_PWRDN_GPIO, 1);
	return 0;
}

struct soc2030_platform_data stingray_soc2030_data = {
	.power_on = stingray_soc2030_power_on,
	.power_off = stingray_soc2030_power_off,
};

static int stingray_bmp085_init(void)
{
	/*struct regulator *reg;*/

	tegra_gpio_enable(BMP085_IRQ_GPIO);
	gpio_request(BMP085_IRQ_GPIO, "bmp085_irq");
	gpio_direction_input(BMP085_IRQ_GPIO);

	return 0;
}

struct bmp085_platform_data stingray_barom_pdata = {
	.poll_interval = 200,
	.min_interval = 20,
	.min_p = 95000,
	.max_p = 125000,
	.fuzz = 5,
	.flat = 5,
};

static int stingray_kxtf9_gpio_level(void)
{
	/* TO DO: Fill in with GPIO level check functions */
	return 0;
}


struct kxtf9_platform_data stingray_kxtf9_pdata = {
	.min_interval	= 2,
	.poll_interval	= 200,

	.g_range	= KXTF9_G_8G,

	.axis_map_x	= 0,
	.axis_map_y	= 1,
	.axis_map_z	= 2,

	.negate_x	= 0,
	.negate_y	= 0,
	.negate_z	= 0,


	.data_odr_init		= ODR12_5,
	.ctrl_reg1_init		= RES_12BIT | KXTF9_G_2G | WUFE,
	.int_ctrl_init		= IEA | IEN,
	.tilt_timer_init	= 0x03,
	.engine_odr_init	= OTP12_5 | OWUF50 | OTDT400,
	.wuf_timer_init		= 0x0A,
	.wuf_thresh_init	= 0x20,
	.tdt_timer_init		= 0x78,
	.tdt_h_thresh_init	= 0xB6,
	.tdt_l_thresh_init	= 0x1A,
	.tdt_tap_timer_init	= 0xA2,
	.tdt_total_timer_init	= 0x24,
	.tdt_latency_timer_init	= 0x28,
	.tdt_window_timer_init	= 0xA0,

	.gpio = stingray_kxtf9_gpio_level,
	.gesture = 0,
	.sensitivity_low = {
		  0x50, 0xFF, 0xB8, 0x32, 0x09, 0x0A, 0xA0,
	},
	.sensitivity_medium = {
		  0x50, 0xFF, 0x68, 0x32, 0x09, 0x0A, 0xA0,
	},
	.sensitivity_high = {
		  0x78, 0xB6, 0x1A, 0xA2, 0x24, 0x28, 0xA0,
	},
};
static void stingray_kxtf9_init(void)
{
	tegra_gpio_enable(KXTF9_IRQ_GPIO);
	gpio_request(KXTF9_IRQ_GPIO, "kxtf9_irq");
	gpio_direction_input(KXTF9_IRQ_GPIO);
}

struct max9635_platform_data stingray_max9635_pdata = {
	.configure = 0x80,
	.threshold_timer = 0x19,
	.def_low_threshold = 0xFE,
	.def_high_threshold = 0xFF,
	.lens_coeff_h = 7,
	.lens_coeff_l = 10,
};

static int stingray_max9635_init(void)
{
	tegra_gpio_enable(MAX9635_IRQ_GPIO);
	gpio_request(MAX9635_IRQ_GPIO, "max9635_irq");
	gpio_direction_input(MAX9635_IRQ_GPIO);
	return 0;
}

static int stingray_l3g4200d_init(void)
{
	tegra_gpio_enable(L3G4200D_IRQ_GPIO);
	gpio_request(L3G4200D_IRQ_GPIO, "l3g4200d_irq");
	gpio_direction_input(L3G4200D_IRQ_GPIO);
	return 0;
}

struct l3g4200d_platform_data stingray_gyro_pdata = {
	.poll_interval = 200,
	.min_interval = 20,

	.ctrl_reg_1 = 0x3f,
	.ctrl_reg_2 = 0x00,
	.ctrl_reg_3 = 0x00,
	.ctrl_reg_4 = 0x20,
	.ctrl_reg_5 = 0x00,
	.int_config = 0x00,
	.int_source = 0x00,
	.int_th_x_h = 0x00,
	.int_th_x_l = 0x00,
	.int_th_y_h = 0x00,
	.int_th_y_l = 0x00,
	.int_th_z_h = 0x00,
	.int_th_z_l = 0x00,
	.int_duration = 0x00,

	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,

	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,
};

static int stingray_akm8975_init(void)
{
	tegra_gpio_enable(AKM8975_IRQ_GPIO);
	gpio_request(AKM8975_IRQ_GPIO, "akm8975");
	gpio_direction_input(AKM8975_IRQ_GPIO);
	return 0;
}

struct lm3559_platform_data stingray_lm3559_data = {
	.flags = 0,
	.flash_duration_def = 0x04, /* 160ms timeout */
	.vin_monitor_def = 0xC0,
};

static void stingray_lm3559_init(void)
{
	tegra_gpio_enable(LM3559_RESETN_GPIO);
	gpio_request(LM3559_RESETN_GPIO, "lm3559_hwenable");
	gpio_direction_output(LM3559_RESETN_GPIO, 1);
	gpio_export(LM3559_RESETN_GPIO, false);

	/* define LM3559_STROBE_GPIO for debug, usually controlled by VGP3 */
	#ifdef LM3559_STROBE_GPIO
	tegra_gpio_enable(LM3559_STROBE_GPIO);
	gpio_request(LM3559_STROBE_GPIO, "lm3559_strobe");
	gpio_direction_output(LM3559_STROBE_GPIO, 0);
	gpio_export(LM3559_STROBE_GPIO, false);
	#endif
}

static struct i2c_board_info __initdata stingray_i2c_bus4_sensor_info[] = {
	{
		I2C_BOARD_INFO("akm8975", 0x0C),
		.irq = TEGRA_GPIO_TO_IRQ(AKM8975_IRQ_GPIO),
	},
	{
		I2C_BOARD_INFO("kxtf9", 0x0F),
		.platform_data = &stingray_kxtf9_pdata,
		.irq = TEGRA_GPIO_TO_IRQ(KXTF9_IRQ_GPIO),
	},
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
	},
};

static struct i2c_board_info __initdata stingray_i2c_bus1_sensor_info[] = {
	{
		I2C_BOARD_INFO(BMP085_NAME, 0x77),
		.platform_data = &stingray_barom_pdata,
		.irq = TEGRA_GPIO_TO_IRQ(BMP085_IRQ_GPIO),
	 },
	{
		 I2C_BOARD_INFO(MAX9635_NAME, 0x4b),
		.platform_data = &stingray_max9635_pdata,
		.irq = TEGRA_GPIO_TO_IRQ(MAX9635_IRQ_GPIO),
	 },
};

static struct i2c_board_info __initdata stingray_i2c_bus3_sensor_info[] = {
	 {
		I2C_BOARD_INFO(L3G4200D_NAME, 0x68),
		.platform_data = &stingray_gyro_pdata,
		.irq = TEGRA_GPIO_TO_IRQ(L3G4200D_IRQ_GPIO),
	 },

	 {
		I2C_BOARD_INFO(LM3559_NAME, 0x53),
		.platform_data = &stingray_lm3559_data,
	 },

	 {
		 I2C_BOARD_INFO("ov5650", 0x36),
		 .platform_data = &stingray_ov5650_data,
	 },

	 {
		 I2C_BOARD_INFO("dw9714l", 0x0C),
	 },

	 {
		 I2C_BOARD_INFO("soc2030", 0x3c),
		 .platform_data = &stingray_soc2030_data,
	 },
};

int __init stingray_sensors_init(void)
{
	stingray_bmp085_init();
	stingray_kxtf9_init();
	stingray_max9635_init();
	stingray_l3g4200d_init();
	stingray_akm8975_init();
	stingray_lm3559_init();
	stingray_ov5650_init();
	stingray_soc2030_init();

	i2c_register_board_info(3, stingray_i2c_bus4_sensor_info,
		ARRAY_SIZE(stingray_i2c_bus4_sensor_info));
	i2c_register_board_info(2, stingray_i2c_bus3_sensor_info,
		ARRAY_SIZE(stingray_i2c_bus3_sensor_info));
	return i2c_register_board_info(0, stingray_i2c_bus1_sensor_info,
		ARRAY_SIZE(stingray_i2c_bus1_sensor_info));
}
