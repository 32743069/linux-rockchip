/* drivers/video/rk30_fb.h
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

#ifndef __ARCH_ARM_MACH_RK30_FB_H
#define __ARCH_ARM_MACH_RK30_FB_H

#define RK30_MAX_LCDC_SUPPORT	4
#define RK30_MAX_LAYER_SUPPORT	4
#define RK_MAX_FB_SUPPORT     4



#define FB0_IOCTL_STOP_TIMER_FLUSH		0x6001
#define FB0_IOCTL_SET_PANEL				0x6002

#ifdef CONFIG_FB_WIMO
#define FB_WIMO_FLAG
#endif
#ifdef FB_WIMO_FLAG
#define FB0_IOCTL_SET_BUF					0x6017
#define FB0_IOCTL_COPY_CURBUF				0x6018
#define FB0_IOCTL_CLOSE_BUF				0x6019
#endif

#define FB1_IOCTL_GET_PANEL_SIZE		0x5001
#define FB1_IOCTL_SET_YUV_ADDR			0x5002
//#define FB1_TOCTL_SET_MCU_DIR			0x5003
#define FB1_IOCTL_SET_ROTATE            0x5003
#define FB1_IOCTL_SET_I2P_ODD_ADDR      0x5005
#define FB1_IOCTL_SET_I2P_EVEN_ADDR     0x5006
#define FB1_IOCTL_SET_WIN0_TOP          0x5018

/********************************************************************
**              display output interface supported by rk lcdc                       *
********************************************************************/
/* */
#define OUT_P888            0
#define OUT_P666            1    //
#define OUT_P565            2    //
#define OUT_S888x           4
#define OUT_CCIR656         6
#define OUT_S888            8
#define OUT_S888DUMY        12
#define OUT_P16BPP4         24  //
#define OUT_D888_P666       0x21  //
#define OUT_D888_P565       0x22  //


/**
 * pixel format definitions,this is copy from android/system/core/include/system/graphics.h
 */

enum {
    HAL_PIXEL_FORMAT_RGBA_8888          = 1,
    HAL_PIXEL_FORMAT_RGBX_8888          = 2,
    HAL_PIXEL_FORMAT_RGB_888            = 3,
    HAL_PIXEL_FORMAT_RGB_565            = 4,
    HAL_PIXEL_FORMAT_BGRA_8888          = 5,
    HAL_PIXEL_FORMAT_RGBA_5551          = 6,
    HAL_PIXEL_FORMAT_RGBA_4444          = 7,

    /* 0x8 - 0xFF range unavailable */

    /*
     * 0x100 - 0x1FF
     *
     * This range is reserved for pixel formats that are specific to the HAL
     * implementation.  Implementations can use any value in this range to
     * communicate video pixel formats between their HAL modules.  These formats
     * must not have an alpha channel.  Additionally, an EGLimage created from a
     * gralloc buffer of one of these formats must be supported for use with the
     * GL_OES_EGL_image_external OpenGL ES extension.
     */

    /*
     * Android YUV format:
     *
     * This format is exposed outside of the HAL to software decoders and
     * applications.  EGLImageKHR must support it in conjunction with the
     * OES_EGL_image_external extension.
     *
     * YV12 is a 4:2:0 YCrCb planar format comprised of a WxH Y plane followed
     * by (W/2) x (H/2) Cr and Cb planes.
     *
     * This format assumes
     * - an even width
     * - an even height
     * - a horizontal stride multiple of 16 pixels
     * - a vertical stride equal to the height
     *
     *   y_size = stride * height
     *   c_size = ALIGN(stride/2, 16) * height/2
     *   size = y_size + c_size * 2
     *   cr_offset = y_size
     *   cb_offset = y_size + c_size
     *
     */
    HAL_PIXEL_FORMAT_YV12   = 0x32315659, // YCrCb 4:2:0 Planar



    /* Legacy formats (deprecated), used by ImageFormat.java */
    HAL_PIXEL_FORMAT_YCbCr_422_SP       = 0x10, // NV16
    HAL_PIXEL_FORMAT_YCrCb_420_SP       = 0x11, // NV21
    HAL_PIXEL_FORMAT_YCbCr_422_I        = 0x14, // YUY2
    HAL_PIXEL_FORMAT_YCrCb_NV12         = 0x20, // YUY2
    HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO   = 0x21, // YUY2
    HAL_PIXEL_FORMAT_YCrCb_444          = 0x22, //yuv444


};


//display data format
enum data_format{
	ARGB888 = 0,
	RGB888,
	RGB565,
	YUV420 = 4,
	YUV422,
	YUV444,
};

struct rk_fb_rgb {
	struct fb_bitfield	red;
	struct fb_bitfield	green;
	struct fb_bitfield	blue;
	struct fb_bitfield	transp;
};

typedef enum _TRSP_MODE
{
    TRSP_CLOSE = 0,
    TRSP_FMREG,
    TRSP_FMREGEX,
    TRSP_FMRAM,
    TRSP_FMRAMEX,
    TRSP_MASK,
    TRSP_INVAL
} TRSP_MODE;

struct layer_par {
    const char *name;
    int id;
    u32	pseudo_pal[16];
    u32 y_offset;       //yuv/rgb offset  -->LCDC_WINx_YRGB_MSTx
    u32 c_offset;     //cb cr offset--->LCDC_WINx_CBR_MSTx
    u32 xpos;         //start point in panel  --->LCDC_WINx_DSP_ST
    u32 ypos;
    u16 xsize;        // display window width/height  -->LCDC_WINx_DSP_INFO
    u16 ysize;          
    u16 xact;        //origin display window size -->LCDC_WINx_ACT_INFO
    u16 yact;
    u16 xvir;       //virtual width/height     -->LCDC_WINx_VIR
    u16 yvir;
    unsigned long smem_start;
    unsigned long cbr_start;  // Cbr memory start address
    enum data_format format;
	
    bool support_3d;
    
};

struct rk_lcdc_device_driver{
	char name[6];
	int id;
	struct device  *dev;
	
	struct layer_par *layer_par;
	int num_layer;
	rk_screen *screen;
	u32 pixclock;
	int (*ioctl)(struct rk_lcdc_device_driver *dev_drv, unsigned int cmd,unsigned long arg,int layer_id);
	int (*suspend)(struct layer_par *layer_par);
	int (*resume)(struct layer_par *layer_par);
	int (*blank)(struct rk_lcdc_device_driver *dev_drv,int layer_id,int blank_mode);
	int (*set_par)(struct rk_lcdc_device_driver *dev_drv,int layer_id);
	int (*pan_display)(struct rk_lcdc_device_driver *dev_drv,int layer_id);
	
};

struct rk_fb_inf {
    struct fb_info *fb[RK_MAX_FB_SUPPORT];
    int num_fb;
    
    struct rk_lcdc_device_driver *lcdc_dev_drv[RK30_MAX_LCDC_SUPPORT];
    int num_lcdc;

    int video_mode;  //when play video set it to 1
};
extern int rk_fb_register(struct rk_lcdc_device_driver *fb_device_driver);
extern int rk_fb_unregister(struct rk_lcdc_device_driver *fb_device_driver);
extern int init_lcdc_device_driver(struct rk_lcdc_device_driver *def_drv,
	struct rk_lcdc_device_driver *dev_drv,int id);
#endif
