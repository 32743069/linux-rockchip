#include <linux/fb.h>
#include <linux/delay.h>
#include "../../rk2818_fb.h"
#include <mach/gpio.h>
#include <mach/iomux.h>
#include "screen.h"

 
/* Base */
#define OUT_TYPE		SCREEN_RGB
#define OUT_FACE		OUT_P888
#define OUT_CLK			23

/* Timing */
#define H_PW			1
#define H_BP			120
#define H_VD			800
#define H_FP			20

#define V_PW			1
#define V_BP			20
#define V_VD			480
#define V_FP			4

/* Other */
#define DCLK_POL		1
#define SWAP_RB			0

#define CS_OUT()        gpio_direction_output(RK2818_PIN_PA4, 0)
#define CS_SET()        gpio_set_value(RK2818_PIN_PA4, GPIO_HIGH)
#define CS_CLR()        gpio_set_value(RK2818_PIN_PA4, GPIO_LOW)
#define CLK_OUT()       gpio_direction_output(RK2818_PIN_PE7, 0)  //I2C0_SCL
#define CLK_SET()       gpio_set_value(RK2818_PIN_PE7, GPIO_HIGH)
#define CLK_CLR()       gpio_set_value(RK2818_PIN_PE7, GPIO_LOW)
#define TXD_OUT()       gpio_direction_output(RK2818_PIN_PE6, 0)  //I2C0_SDA
#define TXD_SET()       gpio_set_value(RK2818_PIN_PE6, GPIO_HIGH)
#define TXD_CLR()       gpio_set_value(RK2818_PIN_PE6, GPIO_LOW)

#define DRVDelayUs(i)   udelay(i*2)

int lcd_init(void);
int lcd_standby(u8 enable);

void screen_set_iomux(u8 enable)
{
    int ret=-1;
    if(enable)
    {
        rk2818_mux_api_set(CXGPIO_HSADC_SEL_NAME, 0);
        ret = gpio_request(RK2818_PIN_PA4, NULL); 
        if(0)//(ret != 0)
        {
            gpio_free(RK2818_PIN_PA4);
            printk(">>>>>> lcd cs gpio_request err \n ");           
            goto pin_err;
        }  
        
        rk2818_mux_api_set(GPIOE_U1IR_I2C1_NAME, 0);                   

        ret = gpio_request(RK2818_PIN_PE7, NULL); 
        if(0)//(ret != 0)
        {
            gpio_free(RK2818_PIN_PE7);
            printk(">>>>>> lcd clk gpio_request err \n "); 
            goto pin_err;
        }  
        
        ret = gpio_request(RK2818_PIN_PE6, NULL); 
        if(0)//(ret != 0)
        {
            gpio_free(RK2818_PIN_PE6);
            printk(">>>>>> lcd txd gpio_request err \n "); 
            goto pin_err;
        }        
    }
    else
    {
         gpio_free(RK2818_PIN_PA4); 
         rk2818_mux_api_set(CXGPIO_HSADC_SEL_NAME, 1);

         gpio_free(RK2818_PIN_PE7);   
         gpio_free(RK2818_PIN_PE6); 
         rk2818_mux_api_set(GPIOE_U1IR_I2C1_NAME, 2);
   }
    return ;
pin_err:
    return ;

}

void set_lcd_info(struct rk28fb_screen *screen)
{
    /* screen type & face */
    screen->type = OUT_TYPE;
    screen->face = OUT_FACE;

    /* Screen size */
    screen->x_res = H_VD;
    screen->y_res = V_VD;

    /* Timing */
    screen->pixclock = OUT_CLK;
	screen->left_margin = H_BP;
	screen->right_margin = H_FP;
	screen->hsync_len = H_PW;
	screen->upper_margin = V_BP;
	screen->lower_margin = V_FP;
	screen->vsync_len = V_PW;

	/* Pin polarity */
	screen->pin_hsync = 0;
	screen->pin_vsync = 0;
	screen->pin_den = 0;
	screen->pin_dclk = DCLK_POL;

	/* Swap rule */
    screen->swap_rb = SWAP_RB;
    screen->swap_rg = 0;
    screen->swap_gb = 0;
    screen->swap_delta = 0;
    screen->swap_dumy = 0;

    /* Operation function*/
    //screen->init = lcd_init;
    screen->standby = lcd_standby;

}

void spi_screenreg_set(u32 Addr, u32 Data)
{
    u32 i;

    TXD_OUT();
    CLK_OUT();
    CS_OUT();
    DRVDelayUs(2);
    DRVDelayUs(2);

    CS_SET();
    TXD_CLR();
    CLK_CLR();
    DRVDelayUs(2);

	CS_CLR();
	for(i = 0; i < 7; i++)  //reg
	{
		if(Addr &(1<<(6-i)))
			TXD_SET();
		else
			TXD_CLR();

		// \u6a21\u62dfCLK
		CLK_CLR();
		DRVDelayUs(2);
		CLK_SET();
		DRVDelayUs(2);
	}

	TXD_CLR();  //write

	// \u6a21\u62dfCLK
    CLK_CLR();
    DRVDelayUs(2);
    CLK_SET();
    DRVDelayUs(2);

	for(i = 0; i < 8; i++)  //data
	{
		if(Data &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		// \u6a21\u62dfCLK
        CLK_CLR();
		DRVDelayUs(2);
		CLK_SET();
		DRVDelayUs(2);
	}

	CS_SET();
	CLK_CLR();
	TXD_CLR();
	DRVDelayUs(2);

}


int lcd_init(void)
{
    screen_set_iomux(1);
	//R(0xess (A5~A0) Data(D7~D0)
#if 0
    spi_screenreg_set(0x03, 0x86);
    spi_screenreg_set(0x05, 0x33);
    spi_screenreg_set(0x09, 0xFF);
    spi_screenreg_set(0x3A, 0x95);
    spi_screenreg_set(0x3C, 0xE0);
    spi_screenreg_set(0x3D, 0xF4);
    spi_screenreg_set(0x3E, 0x21);
    spi_screenreg_set(0x3F, 0x87);
    spi_screenreg_set(0x15, 0x55);
    spi_screenreg_set(0x16, 0xAF);
    spi_screenreg_set(0x17, 0xFC);
    spi_screenreg_set(0x18, 0x00);
    spi_screenreg_set(0x19, 0x4B);
    spi_screenreg_set(0x1A, 0x80);
    spi_screenreg_set(0x1B, 0xFF);
    spi_screenreg_set(0x1C, 0x39);
    spi_screenreg_set(0x1D, 0x69);
    spi_screenreg_set(0x1E, 0x9F);
    spi_screenreg_set(0x1F, 0x09);
    spi_screenreg_set(0x20, 0x8F);
    spi_screenreg_set(0x21, 0xF0);
    spi_screenreg_set(0x22, 0x2B);
    spi_screenreg_set(0x23, 0x58);
    spi_screenreg_set(0x24, 0x7C);
    spi_screenreg_set(0x25, 0xA5);
    spi_screenreg_set(0x26, 0xFF);
#endif

    screen_set_iomux(0);
    return 0;
}

int lcd_standby(u8 enable)
{
    screen_set_iomux(1);
	if(enable) {
		spi_screenreg_set(0x43, 0x20);
	} else {
		spi_screenreg_set(0x43, 0xE0);
	}
    screen_set_iomux(0);
    return 0;
}


