/* arch/arm/mach-rk2818/include/mach/board.h
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

#ifndef __ASM_ARCH_RK2818_BOARD_H
#define __ASM_ARCH_RK2818_BOARD_H

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/notifier.h>



#define INVALID_GPIO        -1


struct rk2818_io_cfg {
	int (*io_init)(void *);
	int (*io_deinit)(void *);
};

/* platform device data structures */
struct platform_device;
struct i2c_client;
struct rk2818_sdmmc_platform_data {
	unsigned int host_caps;
	unsigned int host_ocr_avail;
	unsigned int use_dma:1;
	unsigned int no_detect:1;
	char dma_name[8];
	int (*io_init)(void);
	int (*io_deinit)(void);
};

struct rk2818_i2c_spi_data {
	int     bus_num;        
	unsigned int    flags;     
	unsigned int    slave_addr; 
	unsigned long   scl_rate;   
};
struct rk2818_i2c_platform_data {
	int     bus_num;        
	unsigned int    flags;     
	unsigned int    slave_addr; 
	unsigned long   scl_rate;   
#define I2C_MODE_IRQ    0
#define I2C_MODE_POLL   1
	unsigned int    mode:1;
	int (*io_init)(void);
	int (*io_deinit)(void);
};

struct rk2818lcd_info{
    u32 lcd_id;
    u32 txd_pin;
    u32 clk_pin;
    u32 cs_pin;
    int (*io_init)(void);
    int (*io_deinit)(void);
};

struct rk2818_fb_setting_info{
    u8 data_num;
    u8 vsync_en;
    u8 den_en;
    u8 mcu_fmk_en;
    u8 disp_on_en;
    u8 standby_en;
};

struct rk2818fb_info{
    u32 fb_id;
    u32 disp_on_pin;
    u8 disp_on_value;
    u32 standby_pin;
    u8 standby_value;
    u32 mcu_fmk_pin;
    struct rk2818lcd_info *lcd_info;
    int (*io_init)(struct rk2818_fb_setting_info *fb_setting);
    int (*io_deinit)(void);
};

struct rk2818_bl_info{
    u32 pwm_id;
    u32 bl_ref;
    int (*io_init)(void);
    int (*io_deinit)(void);
    struct timer_list timer;  
    struct notifier_block freq_transition;
};

struct rk2818_gpio_expander_info {
	unsigned int gpio_num;// ��ʼ����pin �ź궨�� �磺RK2818_PIN_PI0
	unsigned int pin_type;//��ʼ����pin Ϊ����pin�������pin �磺GPIO_IN
	unsigned int pin_value;//���Ϊ output pin ���õ�ƽ���磺GPIO_HIGH
};


struct pca9554_platform_data {
	/*  the first extern gpio number in all of gpio groups */
	unsigned gpio_base;
	unsigned	gpio_pin_num;
	/*  the first gpio irq  number in all of irq source */

	unsigned gpio_irq_start;
	unsigned irq_pin_num;        //�жϵĸ���
	unsigned    pca9954_irq_pin;        //��չIO���жϹ����ĸ�gpio
	/* initial polarity inversion setting */
	uint16_t	invert;
	struct rk2818_gpio_expander_info  *settinginfo;
	int  settinginfolen;
	void		*context;	/* param to setup/teardown */

	int		(*setup)(struct i2c_client *client,unsigned gpio, unsigned ngpio,void *context);
	int		(*teardown)(struct i2c_client *client,unsigned gpio, unsigned ngpio,void *context);
	char		**names;
};

struct tca6424_platform_data {
	/*  the first extern gpio number in all of gpio groups */
	unsigned gpio_base;
	unsigned	gpio_pin_num;
	/*  the first gpio irq  number in all of irq source */

	unsigned gpio_irq_start;
	unsigned irq_pin_num;        //�жϵĸ���
	unsigned tca6424_irq_pin;        //��չIO���жϹ����ĸ�gpio
	/* initial polarity inversion setting */
	uint16_t	invert;
	struct rk2818_gpio_expander_info  *settinginfo;
	int  settinginfolen;
	void		*context;	/* param to setup/teardown */

	int		(*setup)(struct i2c_client *client,unsigned gpio, unsigned ngpio,void *context);
	int		(*teardown)(struct i2c_client *client,unsigned gpio, unsigned ngpio,void *context);
	char		**names;
	void    (*reseti2cpin)(void);
};

/*battery*/
struct rk2818_battery_platform_data {
	int (*io_init)(void);
	int (*io_deinit)(void);
	int charge_ok_pin;
	int charge_ok_level;
};

/*g_sensor*/
struct rk2818_gs_platform_data {
	int (*io_init)(void);
	int (*io_deinit)(void);
	int gsensor_irq_pin;
};

/*serial*/
struct rk2818_serial_platform_data {
	int (*io_init)(void);
	int (*io_deinit)(void);
};

/*i2s*/
struct rk2818_i2s_platform_data {
	int (*io_init)(void);
	int (*io_deinit)(void);
};

/*spi*/
struct spi_cs_gpio {
	const char *name;
	unsigned int cs_gpio;
	char *cs_iomux_name;
	unsigned int cs_iomux_mode;
};

struct rk2818_spi_platform_data {
	int (*io_init)(struct spi_cs_gpio*, int);
	int (*io_deinit)(struct spi_cs_gpio*, int);
	struct spi_cs_gpio *chipselect_gpios;	
	u16 num_chipselect;
};

//ROCKCHIP AD KEY CODE ,for demo board
//      key		--->	EV	
#define AD2KEY1                 114   ///VOLUME_DOWN
#define AD2KEY2                 115   ///VOLUME_UP
#define AD2KEY3                 59    ///MENU
#define AD2KEY4                 102   ///HOME
#define AD2KEY5                 158   ///BACK
#define AD2KEY6                 61    ///CALL
#define AD2KEY7                 127   ///SEARCH
#define ENDCALL					62
#define	KEYSTART				28			//ENTER
#define KEYMENU					AD2KEY6		///CALL
#define	KEY_PLAY_SHORT_PRESS	KEYSTART	//code for short press the play key
#define	KEY_PLAY_LONG_PRESS		ENDCALL		//code for long press the play key

//ADC Registers
typedef  struct tagADC_keyst
{
	unsigned int adc_value;
	unsigned int adc_keycode;
}ADC_keyst,*pADC_keyst;

/*ad key*/
struct adc_key_data{
    u32 pin_playon;
    u32 playon_level;
    u32 adc_empty;
    u32 adc_invalid;
    u32 adc_drift;
    u32 adc_chn;
    ADC_keyst * adc_key_table;
    unsigned char *initKeyCode;
    u32 adc_key_cnt;
};

struct rk2818_adckey_platform_data {
	int (*io_init)(void);
	int (*io_deinit)(void);
	struct adc_key_data *adc_key;
};

/* common init routines for use by arch/arm/mach-msm/board-*.c */
void __init rk2818_add_devices(void);
void __init rk2818_map_common_io(void);
void __init rk2818_init_irq(void);
void __init rk2818_init_gpio(void);
void __init rk2818_clock_init(void);

#endif
