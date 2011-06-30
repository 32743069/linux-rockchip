/*
 * wm8994.c -- WM8994 ALSA SoC audio driver
 *
 * Copyright 2009 Wolfson Microelectronics plc
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include <mach/iomux.h>
#include <mach/gpio.h>

#include "wm8994.h"
#include <linux/miscdevice.h>
#include <linux/circ_buf.h>
#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>

#define WM8994_PROC
#ifdef WM8994_PROC
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
char debug_write_read = 0;
#endif


/* If digital BB is used,open this define. 
 Define what kind of digital BB is used. */
//#define PCM_BB
#ifdef PCM_BB
#define TD688_MODE  
//#define MU301_MODE
//#define CHONGY_MODE
//#define THINKWILL_M800_MODE
#endif //PCM_BB

#if 1
#define DBG(x...) printk(KERN_INFO x)
#else
#define DBG(x...) do { } while (0)
#endif

static struct snd_soc_codec *wm8994_codec;



enum wm8994_codec_mode
{
  wm8994_AP_to_headset,
  wm8994_AP_to_speakers,
  wm8994_AP_to_speakers_and_headset,
  wm8994_recorder_and_AP_to_headset,
  wm8994_recorder_and_AP_to_speakers,
  wm8994_FM_to_headset,
  wm8994_FM_to_headset_and_record,
  wm8994_FM_to_speakers,
  wm8994_FM_to_speakers_and_record,
  wm8994_handsetMIC_to_baseband_to_headset,
  wm8994_mainMIC_to_baseband_to_headset,
  wm8994_handsetMIC_to_baseband_to_headset_and_record,
  wm8994_mainMIC_to_baseband_to_earpiece,
  wm8994_mainMIC_to_baseband_to_earpiece_and_record,
  wm8994_mainMIC_to_baseband_to_speakers,
  wm8994_mainMIC_to_baseband_to_speakers_and_record,
  wm8994_BT_baseband,
  wm8994_BT_baseband_and_record,
  null
};
/* wm8994_current_mode:save current wm8994 mode */
unsigned char wm8994_current_mode=null;
enum stream_type_wm8994
{
	VOICE_CALL	=0,
	BLUETOOTH_SCO	=6,
};
/* For voice device route set, add by phc  */
enum VoiceDeviceSwitch
{
	SPEAKER_INCALL,
	SPEAKER_NORMAL,
	
	HEADSET_INCALL,
	HEADSET_NORMAL,

	EARPIECE_INCALL,
	EARPIECE_NORMAL,
	
	BLUETOOTH_SCO_INCALL,
	BLUETOOTH_SCO_NORMAL,

	BLUETOOTH_A2DP_INCALL,
	BLUETOOTH_A2DP_NORMAL,
	
	MIC_CAPTURE,

	EARPIECE_RINGTONE,
	SPEAKER_RINGTONE,
	HEADSET_RINGTONE,
	
	ALL_OPEN,
	ALL_CLOSED
};

//5:0 000000 0x3F
unsigned short headset_vol_table[6]	={0x012D,0x0133,0x0136,0x0139,0x013B,0x013D};
unsigned short speakers_vol_table[6]	={0x012D,0x0133,0x0136,0x0139,0x013B,0x013D};
unsigned short earpiece_vol_table[6]	={0x0127,0x012D,0x0130,0x0135,0x0139,0x013D};//normal
unsigned short BT_vol_table[16]		={0x01DB,0x01DC,0x01DD,0x01DE,0x01DF,0x01E0,
										0x01E1,0x01E2,0x01E3,0x01E4,0x01E5,0x01E6,
										0x01E7,0x01E8,0x01E9,0x01EA};


/* codec private data */
struct wm8994_priv {
	struct mutex io_lock;
	struct mutex route_lock;
	unsigned int sysclk;
	struct snd_soc_codec codec;
	struct snd_kcontrol *kcontrol;//The current working path
	char RW_status;				//ERROR = -1, TRUE = 0;
	struct wm8994_pdata *pdata;
	
	struct delayed_work wm8994_delayed_work;
	int work_type;
	char First_Poweron;

	unsigned int playback_active:1;
	unsigned int capture_active:1;
	/* call_vol:  save all kinds of system volume value. */
	unsigned char call_vol;
	unsigned char BT_call_vol;	
};

int reg_send_data(struct i2c_client *client, unsigned short *reg, unsigned short *data, u32 scl_rate)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	char tx_buf[4];

	memcpy(tx_buf, reg, 2);
	memcpy(tx_buf+2, data, 2);
	msg.addr = client->addr;
	msg.buf = tx_buf;
	msg.len = 4;
	msg.flags = client->flags;
	msg.scl_rate = scl_rate;
	msg.read_type = 0;
	ret = i2c_transfer(adap, &msg, 1);

	return ret;
}

int reg_recv_data(struct i2c_client *client, unsigned short *reg, unsigned short *buf, u32 scl_rate)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msgs[2];

	msgs[0].addr = client->addr;
	msgs[0].buf = (char *)reg;
	msgs[0].flags = client->flags;
	msgs[0].len = 2;
	msgs[0].scl_rate = scl_rate;
	msgs[0].read_type = 2;

	msgs[1].addr = client->addr;
	msgs[1].buf = (char *)buf;
	msgs[1].flags = client->flags | I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].scl_rate = scl_rate;
	msgs[1].read_type = 2;

	ret = i2c_transfer(adap, msgs, 2);

	return ret;
}

int wm8994_set_status(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;	
	int ret = 1;
	mutex_lock(&wm8994->route_lock);
	
	switch(wm8994_current_mode)
	{
	case wm8994_AP_to_headset:
	case wm8994_recorder_and_AP_to_headset:
	case wm8994_handsetMIC_to_baseband_to_headset:
	case wm8994_handsetMIC_to_baseband_to_headset_and_record:
		ret = 1;
		break;
	default:
		ret = -1;
		break;
	}

	mutex_unlock(&wm8994->route_lock);	
	return ret;
}
EXPORT_SYMBOL_GPL(wm8994_set_status);


static int wm8994_read(unsigned short reg,unsigned short *value)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	
	unsigned short regs=((reg>>8)&0x00FF)|((reg<<8)&0xFF00),values;
	char i = 2;
	mutex_lock(&wm8994->io_lock);
	if(wm8994->RW_status == ERROR) goto out;

	while(i > 0)
	{
		i--;
		if (reg_recv_data(wm8994_codec->control_data,&regs,&values,400000) > 0)
		{
			*value=((values>>8)& 0x00FF)|((values<<8)&0xFF00);
		#ifdef WM8994_PROC	
			if(debug_write_read != 0)
				DBG("%s:0x%04x = 0x%04x",__FUNCTION__,reg,*value);
		#endif
			mutex_unlock(&wm8994->io_lock);
			return 0;
		}
	}
	
	wm8994->RW_status = ERROR;	
	printk("%s---line->%d:Codec read error! reg = 0x%x , value = 0x%x\n",__FUNCTION__,__LINE__,reg,*value);
out:	
	mutex_unlock(&wm8994->io_lock);
	return -EIO;
}
	
static int wm8994_write(unsigned short reg,unsigned short value)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;

	unsigned short regs=((reg>>8)&0x00FF)|((reg<<8)&0xFF00),values=((value>>8)&0x00FF)|((value<<8)&0xFF00);
	char i = 2;
	
	mutex_lock(&wm8994->io_lock);

	if(wm8994->RW_status == ERROR) goto out;

#ifdef WM8994_PROC	
	if(debug_write_read != 0)
		DBG("%s:0x%04x = 0x%04x\n",__FUNCTION__,reg,value);
#endif		
	while(i > 0)
	{
		i--;
		if (reg_send_data(wm8994_codec->control_data,&regs,&values,400000) > 0) 
		{
			mutex_unlock(&wm8994->io_lock);
			return 0;
		}	
	}
	
	wm8994->RW_status = ERROR;
	printk("%s---line->%d:Codec write error! reg = 0x%x , value = 0x%x\n",__FUNCTION__,__LINE__,reg,value);

out:	
	mutex_unlock(&wm8994->io_lock);
	return -EIO;
}



static void wm8994_set_volume(unsigned char wm8994_mode,unsigned char volume,unsigned char max_volume)
{
	unsigned short lvol=0,rvol=0;
//	DBG("%s::volume = %d \n",__FUNCTION__,volume);

	if(volume>max_volume)
		volume=max_volume;
	
	switch(wm8994_mode)
	{
		case wm8994_handsetMIC_to_baseband_to_headset_and_record:
		case wm8994_handsetMIC_to_baseband_to_headset:
		case wm8994_mainMIC_to_baseband_to_headset:
			wm8994_read(0x001C, &lvol);
			wm8994_read(0x001D, &rvol);
			//HPOUT1L_VOL bit 0~5 /-57dB to +6dB in 1dB steps
			wm8994_write(0x001C, (lvol&~0x003f)|headset_vol_table[volume]); 
			//HPOUT1R_VOL bit 0~5 /-57dB to +6dB in 1dB steps
			wm8994_write(0x001D, (rvol&~0x003f)|headset_vol_table[volume]); 
			break;
		case wm8994_mainMIC_to_baseband_to_speakers_and_record:
		case wm8994_mainMIC_to_baseband_to_speakers:
			wm8994_read(0x0026, &lvol);
			wm8994_read(0x0027, &rvol);
			//SPKOUTL_VOL bit 0~5 /-57dB to +6dB in 1dB steps
			wm8994_write(0x0026, (lvol&~0x003f)|speakers_vol_table[volume]);
			//SPKOUTR_VOL bit 0~5 /-57dB to +6dB in 1dB steps
			wm8994_write(0x0027, (rvol&~0x003f)|speakers_vol_table[volume]);
			break;
		case wm8994_mainMIC_to_baseband_to_earpiece:
		case wm8994_mainMIC_to_baseband_to_earpiece_and_record:
			wm8994_read(0x0020, &lvol);
			wm8994_read(0x0021, &rvol);
			//MIXOUTL_VOL bit 0~5 /-57dB to +6dB in 1dB steps
			wm8994_write(0x0020, (lvol&~0x003f)|earpiece_vol_table[volume]);
			//MIXOUTR_VOL bit 0~5 /-57dB to +6dB in 1dB steps
			wm8994_write(0x0021, (rvol&~0x003f)|earpiece_vol_table[volume]);
			break;
		case wm8994_BT_baseband:
		case wm8994_BT_baseband_and_record:
			//bit 0~4 /-16.5dB to +30dB in 1.5dB steps
			DBG("BT_vol_table[volume] = 0x%x\n",BT_vol_table[volume]);
			wm8994_write(0x0500, BT_vol_table[volume]);
			wm8994_write(0x0501, 0x0100);		
			break;
		default:
		//	DBG("Set all volume\n");
			wm8994_read(0x001C, &lvol);
			wm8994_read(0x001D, &rvol);
			wm8994_write(0x001C, (lvol&~0x003f)|headset_vol_table[volume]); 
			wm8994_write(0x001D, (rvol&~0x003f)|headset_vol_table[volume]);	
			wm8994_read(0x0026, &lvol);
			wm8994_read(0x0027, &rvol);
			wm8994_write(0x0026, (lvol&~0x003f)|speakers_vol_table[volume]);
			wm8994_write(0x0027, (rvol&~0x003f)|speakers_vol_table[volume]);	
			wm8994_read(0x0020, &lvol);
			wm8994_read(0x0021, &rvol);
			wm8994_write(0x0020, (lvol&~0x003f)|earpiece_vol_table[volume]);
			wm8994_write(0x0021, (rvol&~0x003f)|earpiece_vol_table[volume]);
			break;
	}
}

static void wm8994_set_all_mute(void)
{
	int i;
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;

	if(wm8994->call_vol < 0)
		return;

	for (i = wm8994->call_vol; i >= 0; i--)        
		wm8994_set_volume(null,i,call_maxvol);

}

static void wm8994_set_level_volume(void)
{
	int i;
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	
	for (i = 0; i <= wm8994->call_vol; i++)
		wm8994_set_volume(wm8994_current_mode,i,call_maxvol);
	
}

static void PA_ctrl(unsigned char ctrl)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	struct wm8994_pdata *pdata = wm8994->pdata;
	
	if(pdata->PA_control_pin > 0)
	{
		if(ctrl == GPIO_HIGH)
		{
			DBG("enable PA_control\n");
			gpio_request(pdata->PA_control_pin, NULL);		//AUDIO_PA_ON	 
			gpio_direction_output(pdata->PA_control_pin,GPIO_HIGH); 		
			gpio_free(pdata->PA_control_pin);
		}
		else
		{
			DBG("disable PA_control\n");
			gpio_request(pdata->PA_control_pin, NULL);		//AUDIO_PA_ON	 
			gpio_direction_output(pdata->PA_control_pin,GPIO_LOW); 		
			gpio_free(pdata->PA_control_pin);			
		}
	}
}

static int wm8994_sysclk_config(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;

	if(wm8994->sysclk == 12288000)
		return wm8994_SYSCLK_12288M;
	else
		return wm8994_SYSCLK_3072M;

	printk("wm8994_sysclk_config error\n");	
	return -1;
}

static void wm8994_set_AIF1DAC_EQ(void)
{
	int bank_vol[6] = {0,0,-3,3,-6,3};
	
	wm8994_write(0x0480, 0x0001|((bank_vol[1]+12)<<11)|
		((bank_vol[2]+12)<<6)|((bank_vol[3]+12)<<1));
	wm8994_write(0x0481, 0x0000|((bank_vol[4]+12)<<11)|
		((bank_vol[5]+12)<<6));
}

static int wm8994_reset_ldo(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;	
	struct wm8994_pdata *pdata = wm8994->pdata;
	unsigned short value;
	
	if(wm8994->RW_status == TRUE)
		return 0;
		
	gpio_request(pdata->Power_EN_Pin, NULL);
	gpio_direction_output(pdata->Power_EN_Pin,GPIO_LOW);
	gpio_free(pdata->Power_EN_Pin);	
	msleep(50);
	gpio_request(pdata->Power_EN_Pin, NULL);
	gpio_direction_output(pdata->Power_EN_Pin,GPIO_HIGH);
	gpio_free(pdata->Power_EN_Pin);		
	msleep(50);
	
	wm8994->RW_status = TRUE;
	wm8994_read(0x00,  &value);

	if(value == 0x8994)
		DBG("wm8994_reset_ldo Read ID = 0x%x\n",value);
	else
	{
		wm8994->RW_status = ERROR;
		printk("wm8994_reset_ldo Read ID error value = 0x%x\n",value);
		return -1;
	}	
		
	return 0;	
}
//Set the volume of each channel (including recording)
static void wm8994_set_channel_vol(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	struct wm8994_pdata *pdata = wm8994->pdata;	
	int vol;

	switch(wm8994_current_mode){
	case wm8994_AP_to_speakers_and_headset:
		MAX_MIN(-57,pdata->speaker_normal_vol,6);
		MAX_MIN(-57,pdata->headset_normal_vol,6);
		DBG("headset_normal_vol = %ddB \n",pdata->headset_normal_vol);
		DBG("speaker_normal_vol = %ddB \n",pdata->speaker_normal_vol);
		
		vol = pdata->speaker_normal_vol;
		wm8994_write(0x26,  320+vol+57);  //-57dB~6dB
		wm8994_write(0x27,  320+vol+57);  //-57dB~6dB

		vol = pdata->headset_normal_vol-4;
		//for turn off headset volume when ringtone
		if(vol >= -48)
			vol -= 14;
		else
			vol = -57;

		wm8994_write(0x1C,  320+vol+57);  //-57dB~6dB
		wm8994_write(0x1D,  320+vol+57);  //-57dB~6dB

		wm8994_set_AIF1DAC_EQ();
		break;

	case wm8994_recorder_and_AP_to_headset:
		MAX_MIN(-57,pdata->headset_normal_vol,6);
		MAX_MIN(-16,pdata->recorder_vol,60);
		DBG("recorder_vol = %ddB \n",pdata->recorder_vol);
		DBG("headset_normal_vol = %ddB \n",pdata->headset_normal_vol);

		vol = pdata->recorder_vol;
		if(vol<30)
			wm8994_write(0x1A,  320+(vol+16)*10/15);  //mic vol
		else
		{
			wm8994_write(0x2A,  0x0030);
			wm8994_write(0x1A,  320+(vol-14)*10/15);  //mic vol
		}
		vol = pdata->headset_normal_vol;
		wm8994_write(0x1C,  320+vol+57);  //-57dB~6dB
		wm8994_write(0x1D,  320+vol+57);  //-57dB~6dB
		break;

	case wm8994_recorder_and_AP_to_speakers:
	case wm8994_FM_to_speakers:
		MAX_MIN(-57,pdata->speaker_normal_vol,6);
		MAX_MIN(-16,pdata->recorder_vol,60);
		DBG("speaker_normal_vol = %ddB \n",pdata->speaker_normal_vol);
		DBG("recorder_vol = %ddB \n",pdata->recorder_vol);

		vol = pdata->recorder_vol;
		if(vol<30)
			wm8994_write(0x1A,  320+(vol+16)*10/15);  //mic vol
		else
		{
			wm8994_write(0x2A,  0x0030);
			wm8994_write(0x1A,  320+(vol-14)*10/15);  //mic vol
		}

		vol = pdata->speaker_normal_vol;
		wm8994_write(0x26,  320+vol+57);  //-57dB~6dB
		wm8994_write(0x27,  320+vol+57);  //-57dB~6dB

		wm8994_set_AIF1DAC_EQ();
		break;
	case wm8994_handsetMIC_to_baseband_to_headset:
		MAX_MIN(-12,pdata->headset_incall_vol,6);
		MAX_MIN(-22,pdata->headset_incall_mic_vol,30);
		DBG("headset_incall_mic_vol = %ddB \n",pdata->headset_incall_mic_vol);
		DBG("headset_incall_vol = %ddB \n",pdata->headset_incall_vol);

		vol = pdata->headset_incall_mic_vol;
		if(vol<-16)
		{
			wm8994_write(0x1E,  0x0016);  //mic vol
			wm8994_write(0x18,  320+(vol+22)*10/15);  //mic vol	
		}
		else
		{
			wm8994_write(0x1E,  0x0006);  //mic vol
			wm8994_write(0x18,  320+(vol+16)*10/15);  //mic vol
		}
		break;
	case wm8994_mainMIC_to_baseband_to_headset:
		MAX_MIN(-12,pdata->headset_incall_vol,6);
		MAX_MIN(-22,pdata->speaker_incall_mic_vol,30);
		DBG("speaker_incall_mic_vol = %ddB \n",pdata->speaker_incall_mic_vol);
		DBG("headset_incall_vol = %ddB \n",pdata->headset_incall_vol);

		vol=pdata->speaker_incall_mic_vol;
		if(vol<-16)
		{
			wm8994_write(0x1E,  0x0016);  //mic vol
			wm8994_write(0x1A,  320+(vol+22)*10/15);  //mic vol	
		}
		else
		{
			wm8994_write(0x1E,  0x0006);  //mic vol
			wm8994_write(0x1A,  320+(vol+16)*10/15);  //mic vol
		}
		break;

	case wm8994_mainMIC_to_baseband_to_earpiece:
		MAX_MIN(-22,pdata->speaker_incall_mic_vol,30);
		MAX_MIN(-21,pdata->earpiece_incall_vol,6);
		DBG("earpiece_incall_vol = %ddB \n",pdata->earpiece_incall_vol);
		DBG("speaker_incall_mic_vol = %ddB \n",pdata->speaker_incall_mic_vol);

		vol = pdata->earpiece_incall_vol;
		if(vol>=0)
		{
			wm8994_write(0x33,  0x0018);  //6dB
			wm8994_write(0x31,  (((6-vol)/3)<<3)+(6-vol)/3);  //-21dB
		}
		else
		{
			wm8994_write(0x33,  0x0010);
			wm8994_write(0x31,  (((-vol)/3)<<3)+(-vol)/3);  //-21dB
		}
		vol = pdata->speaker_incall_mic_vol;
		if(vol<-16)
		{
			wm8994_write(0x1E,  0x0016);
			wm8994_write(0x1A,  320+(vol+22)*10/15);	
		}
		else
		{
			wm8994_write(0x1E,  0x0006);
			wm8994_write(0x1A,  320+(vol+16)*10/15);
		}
		break;

	case wm8994_mainMIC_to_baseband_to_speakers:
		MAX_MIN(-22,pdata->speaker_incall_mic_vol,30);
		MAX_MIN(-21,pdata->speaker_incall_vol,12);
		DBG("speaker_incall_vol = %ddB \n",pdata->speaker_incall_vol);
		DBG("speaker_incall_mic_vol = %ddB \n",pdata->speaker_incall_mic_vol);

		vol = pdata->speaker_incall_mic_vol;
		if(vol<-16)
		{
			wm8994_write(0x1E,  0x0016);
			wm8994_write(0x1A,  320+(vol+22)*10/15);	
		}
		else
		{
			wm8994_write(0x1E,  0x0006);
			wm8994_write(0x1A,  320+(vol+16)*10/15);
		}
		vol = pdata->speaker_incall_vol;
		if(vol<=0)
		{
			wm8994_write(0x31,  (((-vol)/3)<<3)+(-vol)/3);
		}
		else if(vol <= 9)
		{
			wm8994_write(0x25,  ((vol*10/15)<<3)+vol*10/15);
		}
		else
		{
			wm8994_write(0x25,  0x003F);
		}
		break;

	case wm8994_BT_baseband:
		MAX_MIN(-16,pdata->BT_incall_vol,30);
		MAX_MIN(-57,pdata->BT_incall_mic_vol,6);
		DBG("BT_incall_mic_vol = %ddB \n",pdata->BT_incall_mic_vol);
		DBG("BT_incall_vol = %ddB \n",pdata->BT_incall_vol);
		vol = pdata->BT_incall_mic_vol;
		wm8994_write(0x20,  320+vol+57);
		vol = pdata->BT_incall_vol;
		wm8994_write(0x19, 0x0500+(vol+16)*10/15);
		break;
	default:
		printk("route error !\n");
	}

}

#define wm8994_reset()	wm8994_set_all_mute();\
						wm8994_write(WM8994_RESET, 0)					

void AP_to_headset(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_AP_to_headset)return;
	wm8994_current_mode=wm8994_AP_to_headset;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x700, 0xA101);
	wm8994_write(0x01,  0x0023);
	wm8994_write(0x200, 0x0000);
	mdelay(WM8994_DELAY);

	wm8994_write(0x220, 0x0000);
	wm8994_write(0x221, 0x0700);
	wm8994_write(0x222, 0x3126);
	wm8994_write(0x223, 0x0100);

	wm8994_write(0x220, 0x0004);
	msleep(10);
	wm8994_write(0x220, 0x0005);
	msleep(5);

	wm8994_write(0x200, 0x0010);
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
#ifdef CONFIG_SND_RK29_CODEC_SOC_MASTER
	wm8994_write(0x302, 0x3000); // AIF1_MSTR=1
	wm8994_write(0x302, 0x7000); // AIF1_MSTR=1
	wm8994_write(0x303, 0x0040); // AIF1 BCLK DIV--------AIF1CLK/4
	wm8994_write(0x304, 0x0040); // AIF1 ADCLRCK DIV-----BCLK/64
	wm8994_write(0x305, 0x0040); // AIF1 DACLRCK DIV-----BCLK/64
	wm8994_write(0x300, 0x4010); // i2s 16 bits
#endif
	wm8994_write(0x200, 0x0011); // sysclk = fll (bit4 =1)   0x0011
  
	wm8994_write(0x04,  0x0303); // AIF1ADC1L_ENA=1, AIF1ADC1R_ENA=1, ADCL_ENA=1, ADCR_ENA=1/ q
	wm8994_write(0x05,  0x0303);   
	wm8994_write(0x2D,  0x0100);
	wm8994_write(0x2E,  0x0100);
	
	wm8994_write(0x4C,  0x9F25);
	msleep(5);
	wm8994_write(0x01,  0x0323);
	msleep(50);
	wm8994_write(0x60,  0x0022);
	wm8994_write(0x60,  0x00FF);

	wm8994_write(0x420, 0x0000);
	wm8994_write(0x601, 0x0001);
	wm8994_write(0x602, 0x0001);
    
	wm8994_write(0x610, 0x01A0);  //DAC1 Left Volume bit0~7  		
	wm8994_write(0x611, 0x01A0);  //DAC1 Right Volume bit0~7	
	wm8994_write(0x03,  0x3030);
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0100);
	wm8994_write(0x36,  0x0003);
	wm8994_write(0x1C,  0x017F);  //HPOUT1L Volume
	wm8994_write(0x1D,  0x017F);  //HPOUT1R Volume
}

void AP_to_speakers(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_AP_to_speakers)return;
	wm8994_current_mode=wm8994_AP_to_speakers;
//	wm8994_reset();
	wm8994_write(0,0);
	msleep(WM8994_DELAY);

//	wm8994_write(0x700, 0xA101);
//	wm8994_write(0x39,  0x006C);
	wm8994_write(0x01,  0x0023);
	wm8994_write(0x200, 0x0000);
	mdelay(WM8994_DELAY);

	wm8994_write(0x220, 0x0000);
	wm8994_write(0x221, 0x0700);
	wm8994_write(0x222, 0x3126);
	wm8994_write(0x223, 0x0100);

	wm8994_write(0x220, 0x0004);
	msleep(10);
	wm8994_write(0x220, 0x0005);
	msleep(5);

	wm8994_write(0x200, 0x0010);
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
#ifdef CONFIG_SND_RK29_CODEC_SOC_MASTER
	wm8994_write(0x302, 0x3000); // AIF1_MSTR=1
	wm8994_write(0x302, 0x7000); // AIF1_MSTR=1
	wm8994_write(0x303, 0x0040); // AIF1 BCLK DIV--------AIF1CLK/4
	wm8994_write(0x304, 0x0040); // AIF1 ADCLRCK DIV-----BCLK/64
	wm8994_write(0x305, 0x0040); // AIF1 DACLRCK DIV-----BCLK/64
	wm8994_write(0x300, 0xC010); // i2s 16 bits
#endif
	wm8994_write(0x200, 0x0011); // sysclk = fll (bit4 =1)   0x0011
  
	wm8994_write(0x01,  0x3023);
	wm8994_write(0x03,  0x0330);
	wm8994_write(0x05,  0x0303);   
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0100);	
	wm8994_write(0x2D,  0x0001);
	wm8994_write(0x2E,  0x0000);
	wm8994_write(0x36,  0x000C);	
	wm8994_write(0x4C,  0x9F25);
	wm8994_write(0x60,  0x00EE);
	wm8994_write(0x420, 0x0000); 

	wm8994_write(0x601, 0x0001);
	wm8994_write(0x602, 0x0001);

	wm8994_write(0x610, 0x01c0);  //DAC1 Left Volume bit0~7	
	wm8994_write(0x611, 0x01c0);  //DAC1 Right Volume bit0~7	

	wm8994_write(0x26,  0x017F);  //Speaker Left Output Volume
	wm8994_write(0x27,  0x017F);  //Speaker Right Output Volume
}

void FM_to_headset(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode == wm8994_FM_to_headset)return;
	wm8994_current_mode = wm8994_FM_to_headset;
	wm8994_write(0,0);
	msleep(WM8994_DELAY);

//clk
//	wm8994_write(0x701, 0x0000);//MCLK2
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC010); // i2s 16 bits
	wm8994_write(0x200, 0x0001); // sysclk = MCLK1
//	wm8994_write(0x200, 0x0009); // sysclk = MCLK2
	wm8994_set_channel_vol();
	
	wm8994_write(0x01,  0x0303); 
	wm8994_write(0x02,  0x03A0);  
	wm8994_write(0x03,  0x0030);	
	wm8994_write(0x19,  0x010B);  //LEFT LINE INPUT 3&4 VOLUME	
	wm8994_write(0x1B,  0x010B);  //RIGHT LINE INPUT 3&4 VOLUME

	wm8994_write(0x28,  0x0044);  
	wm8994_write(0x29,  0x0100);	 
	wm8994_write(0x2A,  0x0100);
	wm8994_write(0x2D,  0x0040); 
	wm8994_write(0x2E,  0x0040);
	wm8994_write(0x4C,  0x9F25);
	wm8994_write(0x60,  0x00EE);
	wm8994_write(0x220, 0x0003);
	wm8994_write(0x221, 0x0700);
	wm8994_write(0x224, 0x0CC0);
	wm8994_write(0x200, 0x0011);
	wm8994_write(0x1C,  0x01F9);  //LEFT OUTPUT VOLUME	
	wm8994_write(0x1D,  0x01F9);  //RIGHT OUTPUT VOLUME
}

void FM_to_headset_and_record(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode == wm8994_FM_to_headset_and_record)return;
	wm8994_current_mode = wm8994_FM_to_headset_and_record;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x01,   0x0003);
	msleep(WM8994_DELAY);
	wm8994_write(0x221,  0x1900);  //8~13BIT div

#ifdef CONFIG_SND_RK29_CODEC_SOC_MASTER
	wm8994_write(0x302,  0x4000);  // master = 0x4000 // slave= 0x0000
	wm8994_write(0x303,  0x0040);  // master  0x0050 lrck 7.94kHz bclk 510KHz
#endif
	
	wm8994_write(0x220,  0x0004);
	msleep(WM8994_DELAY);
	wm8994_write(0x220,  0x0005);  

	wm8994_write(0x01,   0x0323);
	wm8994_write(0x02,   0x03A0);
	wm8994_write(0x03,   0x0030);
	wm8994_write(0x19,   0x010B);  //LEFT LINE INPUT 3&4 VOLUME	
	wm8994_write(0x1B,   0x010B);  //RIGHT LINE INPUT 3&4 VOLUME
  
	wm8994_write(0x28,   0x0044);
	wm8994_write(0x29,   0x0100);
	wm8994_write(0x2A,   0x0100);
	wm8994_write(0x2D,   0x0040);
	wm8994_write(0x2E,   0x0040);
	wm8994_write(0x4C,   0x9F25);
	wm8994_write(0x60,   0x00EE);
	wm8994_write(0x200,  0x0011);
	wm8994_write(0x1C,   0x01F9);  //LEFT OUTPUT VOLUME
	wm8994_write(0x1D,   0x01F9);  //RIGHT OUTPUT VOLUME
	wm8994_write(0x04,   0x0303);
	wm8994_write(0x208,  0x000A);
	wm8994_write(0x300,  0x4050);
	wm8994_write(0x606,  0x0002);
	wm8994_write(0x607,  0x0002);
	wm8994_write(0x620,  0x0000);
}

void FM_test(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode == wm8994_FM_to_speakers)return;
	wm8994_current_mode = wm8994_FM_to_speakers;
	wm8994_reset();
	msleep(WM8994_DELAY);

//clk
//	wm8994_write(0x701, 0x0000);//MCLK2
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC010); // i2s 16 bits
	wm8994_write(0x200, 0x0001); // sysclk = MCLK1
//	wm8994_write(0x200, 0x0009); // sysclk = MCLK2
	wm8994_set_channel_vol();
	
	wm8994_write(0x20,   0x013F);
	wm8994_write(0x21,   0x013F);
//path
	wm8994_write(0x28,   0x0044);//IN2RN_TO_IN2R IN2LN_TO_IN2L
	wm8994_write(0x29,   0x0107);//IN2L PGA Output to MIXINL UnMute
	wm8994_write(0x2A,   0x0107);//IN2R PGA Output to MIXINR UnMute
	wm8994_write(0x2D,   0x0040);//MIXINL_TO_MIXOUTL
	wm8994_write(0x2E,   0x0040);//MIXINR_TO_MIXOUTR
	wm8994_write(0x36,   0x000C);//MIXOUTL_TO_SPKMIXL   MIXOUTR_TO_SPKMIXR
//volume
	wm8994_write(0x22,   0x0000);
	wm8994_write(0x23,   0x0000);
	wm8994_write(0x19,   0x013F);  //LEFT LINE INPUT 3&4 VOLUME
	wm8994_write(0x1B,   0x013F);  //RIGHT LINE INPUT 3&4 VOLUME		
//power
	wm8994_write(0x01,   0x3003);
	wm8994_write(0x02,   0x03A0);
	wm8994_write(0x03,   0x0330);	
}

void FM_to_speakers(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);
	if(wm8994_current_mode == wm8994_FM_to_speakers)return;
	wm8994_current_mode = wm8994_FM_to_speakers;
	wm8994_reset();
	msleep(WM8994_DELAY);
	wm8994_write(0x01,  0x0003);
	msleep(WM8994_DELAY);
//clk
//	wm8994_write(0x701, 0x0000);//MCLK2
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC010); // i2s 16 bits
	wm8994_write(0x200, 0x0001); // sysclk = MCLK1
//	wm8994_write(0x200, 0x0009); // sysclk = MCLK2
	wm8994_set_channel_vol();
	
//path
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0100);
	wm8994_write(0x2D,  0x0030);  //bit 1 IN2LP_TO_MIXOUTL bit 12 DAC1L_TO_HPOUT1L  0x0102 
	wm8994_write(0x2E,  0x0030);  //bit 1 IN2RP_TO_MIXOUTR bit 12 DAC1R_TO_HPOUT1R  0x0102
	wm8994_write(0x36,  0x000C);//MIXOUTL_TO_SPKMIXL	MIXOUTR_TO_SPKMIXR
//volume
	wm8994_write(0x25,  0x003F);
	wm8994_write(0x610, 0x01C0);  //DAC1 Left Volume bit0~7	
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
//power
	wm8994_write(0x01,  0x3003);	
	wm8994_write(0x03,  0x0330);
	wm8994_write(0x05,  0x0003);
}

void FM_to_speakers_and_record(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode == wm8994_FM_to_speakers_and_record)return;
	wm8994_current_mode = wm8994_FM_to_speakers_and_record;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x01,   0x0003);  
	msleep(WM8994_DELAY);

#ifdef CONFIG_SND_RK29_CODEC_SOC_MASTER
	wm8994_write(0x302,  0x4000);  // master = 0x4000 // slave= 0x0000
	wm8994_write(0x303,  0x0090);  //
#endif
	
	wm8994_write(0x220,  0x0006);
	msleep(WM8994_DELAY);

	wm8994_write(0x01,   0x3023);
	wm8994_write(0x02,   0x03A0);
	wm8994_write(0x03,   0x0330);
	wm8994_write(0x19,   0x010B);  //LEFT LINE INPUT 3&4 VOLUME
	wm8994_write(0x1B,   0x010B);  //RIGHT LINE INPUT 3&4 VOLUME
  
	wm8994_write(0x22,   0x0000);
	wm8994_write(0x23,   0x0000);
	wm8994_write(0x36,   0x000C);

	wm8994_write(0x28,   0x0044);
	wm8994_write(0x29,   0x0100);
	wm8994_write(0x2A,   0x0100);
	wm8994_write(0x2D,   0x0040);
	wm8994_write(0x2E,   0x0040);

	wm8994_write(0x220,  0x0003);
	wm8994_write(0x221,  0x0700);
	wm8994_write(0x224,  0x0CC0);

	wm8994_write(0x200,  0x0011);
	wm8994_write(0x20,   0x01F9);
	wm8994_write(0x21,   0x01F9);
	wm8994_write(0x04,   0x0303);
	wm8994_write(0x208,  0x000A);	
	wm8994_write(0x300,  0x4050);
	wm8994_write(0x606,  0x0002);	
	wm8994_write(0x607,  0x0002);
	wm8994_write(0x620,  0x0000);
}



void AP_to_speakers_and_headset(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);
	if(wm8994_current_mode==wm8994_AP_to_speakers_and_headset)return;
	wm8994_current_mode=wm8994_AP_to_speakers_and_headset;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x39,  0x006C);
	wm8994_write(0x01,  0x0023);
	wm8994_write(0x200, 0x0000);
	msleep(WM8994_DELAY);
//clk
//	wm8994_write(0x701, 0x0000);//MCLK2
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
	{
		wm8994_write(0x220, 0x0000); 
		wm8994_write(0x221, 0x0700);
		wm8994_write(0x222, 0); 
		wm8994_write(0x223, 0x0400);		
		wm8994_write(0x220, 0x0004); 
		msleep(10);
		wm8994_write(0x220, 0x0005); 			
		msleep(5);
		wm8994_write(0x200, 0x0010); // sysclk = MCLK1
	}
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC010); // i2s 16 bits	
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
		wm8994_write(0x200, 0x0011); // sysclk = MCLK1
	else 
		wm8994_write(0x200, 0x0001); // sysclk = MCLK1
//	wm8994_write(0x200, 0x0009); // sysclk = MCLK2
//path
	wm8994_write(0x2D,  0x0100);
	wm8994_write(0x2E,  0x0100);
	wm8994_write(0x60,  0x0022);
	wm8994_write(0x60,  0x00EE);
	wm8994_write(0x420, 0x0000); 	
	wm8994_write(0x601, 0x0001);
	wm8994_write(0x602, 0x0001);
	wm8994_write(0x36,  0x0003);
//	wm8994_write(0x24,  0x0011);	
//other
	wm8994_write(0x4C,  0x9F25);
//volume
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0100);
	wm8994_write(0x610, 0x01C0);  //DAC1 Left Volume bit0~7	
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
//	wm8994_write(0x25,  0x003F);	
	wm8994_set_channel_vol();
//power
	wm8994_write(0x04,  0x0303); // AIF1ADC1L_ENA=1, AIF1ADC1R_ENA=1, ADCL_ENA=1, ADCR_ENA=1
	wm8994_write(0x05,  0x0303);  
	wm8994_write(0x03,  0x3330);	
	wm8994_write(0x01,  0x3303);
	msleep(50); 
	wm8994_write(0x01,  0x3323);	
}

void recorder_and_AP_to_headset(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_recorder_and_AP_to_headset)return;
	wm8994_current_mode=wm8994_recorder_and_AP_to_headset;
	wm8994_reset();
	msleep(WM8994_DELAY);
	
	wm8994_write(0x39, 0x006C);

	wm8994_write(0x01, 0x0003);
	msleep(35);	
	wm8994_write(0xFF, 0x0000);
	msleep(5);
	wm8994_write(0x4C, 0x9F25);
	msleep(5);
	wm8994_write(0x01, 0x0303);
	wm8994_write(0x60, 0x0022);
	msleep(5);	
	wm8994_write(0x54, 0x0033);//
	
//	wm8994_write(0x01,  0x0003);	
	wm8994_write(0x200, 0x0000);
	msleep(WM8994_DELAY);
//clk
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
	{
		wm8994_write(0x220, 0x0000); 
		wm8994_write(0x221, 0x0700);
		wm8994_write(0x222, 0); 
		wm8994_write(0x223, 0x0400);		
		wm8994_write(0x220, 0x0004); 
		msleep(10);
		wm8994_write(0x220, 0x0005); 			
		msleep(5);
		wm8994_write(0x200, 0x0010); // sysclk = MCLK1
	}
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC050); // i2s 16 bits	
	msleep(10);
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
		wm8994_write(0x200, 0x0011); // sysclk = MCLK1
	else 
		wm8994_write(0x200, 0x0001); // sysclk = MCLK1	
//recorder
	wm8994_write(0x28,  0x0003); // IN1RP_TO_IN1R=1, IN1RN_TO_IN1R=1
	wm8994_write(0x606, 0x0002); // ADC1L_TO_AIF1ADC1L=1
	wm8994_write(0x607, 0x0002); // ADC1R_TO_AIF1ADC1R=1
	wm8994_write(0x620, 0x0000); 
	wm8994_write(0x402, 0x01FF); // AIF1ADC1L_VOL [7:0]
	wm8994_write(0x403, 0x01FF); // AIF1ADC1R_VOL [7:0]
//DRC	
	wm8994_write(0x440, 0x01BF);
	wm8994_write(0x450, 0x01BF);
//path	
	wm8994_write(0x2D,  0x0100); // DAC1L_TO_HPOUT1L=1
	wm8994_write(0x2E,  0x0100); // DAC1R_TO_HPOUT1R=1
	wm8994_write(0x60,  0x0022);
	wm8994_write(0x60,  0x00FF);
	wm8994_write(0x420, 0x0000); 
	wm8994_write(0x601, 0x0001); // AIF1DAC1L_TO_DAC1L=1
	wm8994_write(0x602, 0x0001); // AIF1DAC1R_TO_DAC1R=1
//volume	
	wm8994_write(0x610, 0x01A0); // DAC1_VU=1, DAC1L_VOL=1100_0000
	wm8994_write(0x611, 0x01A0); // DAC1_VU=1, DAC1R_VOL=1100_0000
	wm8994_set_channel_vol();	
//other
//	wm8994_write(0x4C,  0x9F25);		
//power
	wm8994_write(0x01,  0x0333);
	wm8994_write(0x02,  0x6110); // TSHUT_ENA=1, TSHUT_OPDIS=1, MIXINR_ENA=1,IN1R_ENA=1
	wm8994_write(0x03,  0x3030);
	wm8994_write(0x04,  0x0303); // AIF1ADC1L_ENA=1, AIF1ADC1R_ENA=1, ADCL_ENA=1, ADCR_ENA=1
	wm8994_write(0x05,  0x0303); // AIF1DAC1L_ENA=1, AIF1DAC1R_ENA=1, DAC1L_ENA=1, DAC1R_ENA=1

}

void recorder_and_AP_to_speakers(void)
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_recorder_and_AP_to_speakers)return;
	wm8994_current_mode=wm8994_recorder_and_AP_to_speakers;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x39,  0x006C);
	wm8994_write(0x01,  0x0023);
	msleep(WM8994_DELAY);
//clk
//	wm8994_write(0x701, 0x0000);//MCLK2
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
	{
		wm8994_write(0x220, 0x0000); 
		wm8994_write(0x221, 0x0700);
		wm8994_write(0x222, 0); 
		wm8994_write(0x223, 0x0400);		
		wm8994_write(0x220, 0x0004); 
		msleep(10);
		wm8994_write(0x220, 0x0005); 			
		msleep(5);
		wm8994_write(0x200, 0x0010); // sysclk = MCLK1
	}
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC010); // i2s 16 bits	
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
		wm8994_write(0x200, 0x0011); // sysclk = MCLK1
	else 
		wm8994_write(0x200, 0x0001); // sysclk = MCLK1
	
//	wm8994_write(0x200, 0x0009); // sysclk = MCLK2
//recorder
	wm8994_write(0x02,  0x6110); // TSHUT_ENA=1, TSHUT_OPDIS=1, MIXINR_ENA=1,IN1R_ENA=1
	wm8994_write(0x04,  0x0303); // AIF1ADC1L_ENA=1, AIF1ADC1R_ENA=1, ADCL_ENA=1, ADCR_ENA=1
	wm8994_write(0x28,  0x0003); // IN1RP_TO_IN1R=1, IN1RN_TO_IN1R=1	
	wm8994_write(0x606, 0x0002); // ADC1L_TO_AIF1ADC1L=1
	wm8994_write(0x607, 0x0002); // ADC1R_TO_AIF1ADC1R=1
	wm8994_write(0x620, 0x0000); 
	wm8994_write(0x402, 0x01FF); // AIF1ADC1L_VOL [7:0]
	wm8994_write(0x403, 0x01FF); // AIF1ADC1R_VOL [7:0]
//path
	wm8994_write(0x2D,  0x0001); // DAC1L_TO_MIXOUTL=1
	wm8994_write(0x2E,  0x0001); // DAC1R_TO_MIXOUTR=1
	wm8994_write(0x36,  0x000C); // MIXOUTL_TO_SPKMIXL=1, MIXOUTR_TO_SPKMIXR=1
	wm8994_write(0x601, 0x0001); // AIF1DAC1L_TO_DAC1L=1
	wm8994_write(0x602, 0x0001); // AIF1DAC1R_TO_DAC1R=1
	wm8994_write(0x420, 0x0000);
//	wm8994_write(0x24,  0x001f);	
//volume
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0100); // SPKOUT_CLASSAB=1	
	wm8994_write(0x610, 0x01C0); // DAC1_VU=1, DAC1L_VOL=1100_0000
	wm8994_write(0x611, 0x01C0); // DAC1_VU=1, DAC1R_VOL=1100_0000	
	wm8994_write(0x25,  0x003F);
	wm8994_set_channel_vol();
//other
	wm8994_write(0x4C,  0x9F25);
//DRC
	wm8994_write(0x440, 0x01BF);
	wm8994_write(0x450, 0x01BF);		
//power
	wm8994_write(0x03,  0x0330); // SPKRVOL_ENA=1, SPKLVOL_ENA=1, MIXOUTL_ENA=1, MIXOUTR_ENA=1  
	wm8994_write(0x05,  0x0303); // AIF1DAC1L_ENA=1, AIF1DAC1R_ENA=1, DAC1L_ENA=1, DAC1R_ENA=1	
	wm8994_write(0x01,  0x3003);
	msleep(50);
	wm8994_write(0x01,  0x3033);
}

#ifndef PCM_BB
void handsetMIC_to_baseband_to_headset(void)
{//
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode == wm8994_handsetMIC_to_baseband_to_headset)return;
	wm8994_current_mode = wm8994_handsetMIC_to_baseband_to_headset;
	wm8994_reset();
	msleep(WM8994_DELAY);
	
	wm8994_write(0x01,  0x0023); 
	wm8994_write(0x200, 0x0000);
	msleep(WM8994_DELAY);
//clk
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
	{
		wm8994_write(0x220, 0x0000); 
		wm8994_write(0x221, 0x0700);
		wm8994_write(0x222, 0); 
		wm8994_write(0x223, 0x0400);		
		wm8994_write(0x220, 0x0004); 
		msleep(10);
		wm8994_write(0x220, 0x0005); 			
		msleep(5);
		wm8994_write(0x200, 0x0010); // sysclk = MCLK1
	}
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC010); // i2s 16 bits	
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
		wm8994_write(0x200, 0x0011); // sysclk = MCLK1
	else 
		wm8994_write(0x200, 0x0001); // sysclk = MCLK1
//path
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0100);
	wm8994_write(0x02,  0x6240);
	wm8994_write(0x28,  0x0030);  //IN1LN_TO_IN1L IN1LP_TO_IN1L
	wm8994_write(0x2D,  0x0003);  //bit 1 IN2LP_TO_MIXOUTL bit 12 DAC1L_TO_HPOUT1L  0x0102 
	wm8994_write(0x2E,  0x0003);  //bit 1 IN2RP_TO_MIXOUTR bit 12 DAC1R_TO_HPOUT1R  0x0102
	wm8994_write(0x34,  0x0002);  //IN1L_TO_LINEOUT1P
	wm8994_write(0x36,  0x0003);
	wm8994_write(0x60,  0x0022);
	wm8994_write(0x60,  0x00EE);
	wm8994_write(0x420, 0x0000);
	wm8994_write(0x601, 0x0001);
	wm8994_write(0x602, 0x0001);
//volume
	wm8994_write(0x610, 0x01A0);  //DAC1 Left Volume bit0~7  		
	wm8994_write(0x611, 0x01A0);  //DAC1 Right Volume bit0~7
	wm8994_set_channel_vol();	
//other
	wm8994_write(0x4C,  0x9F25);	
//power
	wm8994_write(0x03,  0x3030);
	wm8994_write(0x04,  0x0300); // AIF1ADC1L_ENA=1, AIF1ADC1R_ENA=1
	wm8994_write(0x05,  0x0303);
	wm8994_write(0x01,  0x0303);
	msleep(50);
	wm8994_write(0x01,  0x0323);	
	
	wm8994_set_level_volume();
}

void mainMIC_to_baseband_to_headset(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode == wm8994_mainMIC_to_baseband_to_headset)return;

	wm8994_current_mode = wm8994_mainMIC_to_baseband_to_headset;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x700, 0xA101);
	wm8994_write(0x01,  0x0023);
	wm8994_write(0x200, 0x0000);
	mdelay(WM8994_DELAY);

	wm8994_write(0x220, 0x0000);
	wm8994_write(0x221, 0x0700);
	wm8994_write(0x222, 0x3126);
	wm8994_write(0x223, 0x0100);

	wm8994_write(0x220, 0x0004);
	msleep(10);
	wm8994_write(0x220, 0x0005);
	msleep(5);

	wm8994_write(0x200, 0x0010);
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
#ifdef CONFIG_SND_RK29_CODEC_SOC_MASTER
	wm8994_write(0x302, 0x3000); // AIF1_MSTR=1
	wm8994_write(0x302, 0x7000); // AIF1_MSTR=1
	wm8994_write(0x303, 0x0040); // AIF1 BCLK DIV--------AIF1CLK/4
	wm8994_write(0x304, 0x0040); // AIF1 ADCLRCK DIV-----BCLK/64
	wm8994_write(0x305, 0x0040); // AIF1 DACLRCK DIV-----BCLK/64
	wm8994_write(0x300, 0xC010); // i2s 16 bits
#endif
	wm8994_write(0x200, 0x0011); // sysclk = fll (bit4 =1)   0x0011

	wm8994_write(0x39,  0x006C);
	wm8994_write(0x01,  0x0023); 
	mdelay(WM8994_DELAY);
	wm8994_write(0xFF,  0x0000);
	mdelay(5);
	wm8994_write(0x4C,  0x9F25);
	mdelay(5);
	wm8994_write(0x01,  0x0323); 
	wm8994_write(0x60,  0x0022);
	wm8994_write(0x60,  0x00EE);
	mdelay(5);
	wm8994_write(0x54,  0x0033);

	wm8994_write(0x610, 0x0100);  //DAC1 Left Volume bit0~7	
	wm8994_write(0x611, 0x0100);  //DAC1 Right Volume bit0~7

	wm8994_set_volume(wm8994_current_mode,wm8994->call_vol,call_maxvol);
	wm8994_set_channel_vol();
	msleep(50);
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0100);

	wm8994_write(0x02,  0x6210);
	wm8994_write(0x28,  0x0003);  //IN1LN_TO_IN1L IN1LP_TO_IN1L
#ifdef CONFIG_SND_BB_DIFFERENTIAL_INPUT
	wm8994_write(0x2D,  0x0041);  //bit 1 MIXINL_TO_MIXOUTL bit 12 DAC1L_TO_HPOUT1L  0x0102 
	wm8994_write(0x2E,  0x0081);  //bit 1 MIXINL_TO_MIXOUTR bit 12 DAC1R_TO_HPOUT1R  0x0102
#endif
#ifdef CONFIG_SND_BB_NORMAL_INPUT
	wm8994_write(0x2D,  0x0003);  //bit 1 IN2LP_TO_MIXOUTL bit 12 DAC1L_TO_HPOUT1L  0x0102 
	wm8994_write(0x2E,  0x0003);  //bit 1 IN2RP_TO_MIXOUTR bit 12 DAC1R_TO_HPOUT1R  0x0102
#endif
	wm8994_write(0x34,  0x0004);  //IN1L_TO_LINEOUT1P
	wm8994_write(0x36,  0x0003);

	wm8994_write(0x4C,  0x9F25);
	mdelay(5);
	wm8994_write(0x01,  0x0303);
	mdelay(50);
	wm8994_write(0x60,  0x0022);
	wm8994_write(0x60,  0x00EE);

	wm8994_write(0x03,  0x3030);
	wm8994_write(0x04,  0x0300); // AIF1ADC1L_ENA=1, AIF1ADC1R_ENA=1
	wm8994_write(0x05,  0x0303);
	wm8994_write(0x420, 0x0000);

	wm8994_write(0x01,  0x0333);

	wm8994_write(0x601, 0x0001);
	wm8994_write(0x602, 0x0001);

	wm8994_write(0x610, 0x01A0);  //DAC1 Left Volume bit0~7  		
	wm8994_write(0x611, 0x01A0);  //DAC1 Right Volume bit0~7
}

void handsetMIC_to_baseband_to_headset_and_record(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode == wm8994_handsetMIC_to_baseband_to_headset_and_record)return;
	wm8994_current_mode = wm8994_handsetMIC_to_baseband_to_headset_and_record;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x01,  0x0303); 
	wm8994_write(0x02,  0x62C0); 
	wm8994_write(0x03,  0x3030); 
	wm8994_write(0x04,  0x0303); 
	wm8994_write(0x18,  0x014B);  //volume
	wm8994_write(0x19,  0x014B);  //volume
	wm8994_set_volume(wm8994_current_mode,wm8994->call_vol,call_maxvol);
	wm8994_write(0x1E,  0x0006); 
	wm8994_write(0x28,  0x00B0);  //IN2LP_TO_IN2L
	wm8994_write(0x29,  0x0120); 
	wm8994_write(0x2D,  0x0002);  //bit 1 IN2LP_TO_MIXOUTL
	wm8994_write(0x2E,  0x0002);  //bit 1 IN2RP_TO_MIXOUTR
	wm8994_write(0x34,  0x0002); 
	wm8994_write(0x4C,  0x9F25); 
	wm8994_write(0x60,  0x00EE); 
	wm8994_write(0x200, 0x0001); 
	wm8994_write(0x208, 0x000A); 
	wm8994_write(0x300, 0x0050); 

#ifdef CONFIG_SND_RK29_CODEC_SOC_MASTER
	wm8994_write(0x302, 0x4000);  // master = 0x4000 // slave= 0x0000
	wm8994_write(0x303, 0x0090);  // master lrck 16k
#endif

	wm8994_write(0x606, 0x0002); 
	wm8994_write(0x607, 0x0002); 
	wm8994_write(0x620, 0x0000);
}

void mainMIC_to_baseband_to_earpiece(void)
{//
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode == wm8994_mainMIC_to_baseband_to_earpiece)return;
	wm8994_current_mode = wm8994_mainMIC_to_baseband_to_earpiece;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x01,  0x0023);
	wm8994_write(0x200, 0x0000);
	msleep(WM8994_DELAY);
//clk
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
	{
		wm8994_write(0x220, 0x0000); 
		wm8994_write(0x221, 0x0700);
		wm8994_write(0x222, 0); 
		wm8994_write(0x223, 0x0400);		
		wm8994_write(0x220, 0x0004); 
		msleep(10);
		wm8994_write(0x220, 0x0005); 			
		msleep(5);
		wm8994_write(0x200, 0x0010); // sysclk = MCLK1
	}
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC010); // i2s 16 bits	
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
		wm8994_write(0x200, 0x0011); // sysclk = MCLK1
	else 
		wm8994_write(0x200, 0x0001); // sysclk = MCLK1
//path
	wm8994_write(0x28,  0x0003); //IN1RP_TO_IN1R IN1RN_TO_IN1R
	wm8994_write(0x34,  0x0004); //IN1R_TO_LINEOUT1P
	wm8994_write(0x2D,  0x0003);  //bit 1 IN2LP_TO_MIXOUTL bit 12 DAC1L_TO_HPOUT1L  0x0102 
	wm8994_write(0x2E,  0x0003);  //bit 1 IN2RP_TO_MIXOUTR bit 12 DAC1R_TO_HPOUT1R  0x0102
	wm8994_write(0x601, 0x0001); //AIF1DAC1L_TO_DAC1L=1
	wm8994_write(0x602, 0x0001); //AIF1DAC1R_TO_DAC1R=1
	wm8994_write(0x420, 0x0000);
//volume
	wm8994_write(0x610, 0x01C0); //DAC1_VU=1, DAC1L_VOL=1100_0000
	wm8994_write(0x611, 0x01C0); //DAC1_VU=1, DAC1R_VOL=1100_0000
	wm8994_write(0x1F,  0x0000);//HPOUT2
	wm8994_set_channel_vol();	
//other
	wm8994_write(0x4C,  0x9F25);		
//power
	wm8994_write(0x01,  0x0833); //HPOUT2_ENA=1, VMID_SEL=01, BIAS_ENA=1
	wm8994_write(0x02,  0x6250); //bit4 IN1R_ENV bit6 IN1L_ENV 
	wm8994_write(0x03,  0x30F0);
	wm8994_write(0x04,  0x0303); // AIF1ADC1L_ENA=1, AIF1ADC1R_ENA=1, ADCL_ENA=1, ADCR_ENA=1
	wm8994_write(0x05,  0x0303);
	
	wm8994_set_level_volume();	
}

void mainMIC_to_baseband_to_earpiece_and_record(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_mainMIC_to_baseband_to_earpiece_and_record)return;
	wm8994_current_mode=wm8994_mainMIC_to_baseband_to_earpiece_and_record;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x01  ,0x0803|wm8994_mic_VCC);
	wm8994_write(0x02  ,0x6310);
	wm8994_write(0x03  ,0x30A0);
	wm8994_write(0x04  ,0x0303);
	wm8994_write(0x1A  ,0x014F);
	wm8994_write(0x1E  ,0x0006);
	wm8994_write(0x1F  ,0x0000);
	wm8994_write(0x28  ,0x0003);  //MAINMIC_TO_IN1R
	wm8994_write(0x2A  ,0x0020);  //IN1R_TO_MIXINR
	wm8994_write(0x2B  ,0x0005);  //VRX_MIXINL_VOL bit 0~2
	wm8994_write(0x2C  ,0x0005);  //VRX_MIXINR_VOL
	wm8994_write(0x2D  ,0x0040);  //MIXINL_TO_MIXOUTL
	wm8994_write(0x33  ,0x0010);  //MIXOUTLVOL_TO_HPOUT2
	wm8994_write(0x34  ,0x0004);  //IN1R_TO_LINEOUT1
	wm8994_write(0x200 ,0x0001);
	wm8994_write(0x208 ,0x000A);
	wm8994_write(0x300 ,0xC050);
	wm8994_set_volume(wm8994_current_mode,wm8994->call_vol,call_maxvol);

#ifdef CONFIG_SND_RK29_CODEC_SOC_MASTER
	wm8994_write(0x302, 0x4000);  // master = 0x4000 // slave= 0x0000
	wm8994_write(0x303, 0x0090);  // master lrck 16k
#endif

	wm8994_write(0x606 ,0x0002);
	wm8994_write(0x607 ,0x0002);
	wm8994_write(0x620 ,0x0000);
}

void mainMIC_to_baseband_to_speakers(void)
{//
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_mainMIC_to_baseband_to_speakers)return;

	wm8994_current_mode=wm8994_mainMIC_to_baseband_to_speakers;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x39,  0x006C);
	wm8994_write(0x01,  0x0023);
	wm8994_write(0x200, 0x0000);
	msleep(WM8994_DELAY);
//clk
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
	{
		wm8994_write(0x220, 0x0000); 
		wm8994_write(0x221, 0x0700);
		wm8994_write(0x222, 0); 
		wm8994_write(0x223, 0x0400);		
		wm8994_write(0x220, 0x0004); 
		msleep(10);
		wm8994_write(0x220, 0x0005); 			
		msleep(5);
		wm8994_write(0x200, 0x0010); // sysclk = MCLK1
	}
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC010); // i2s 16 bits	
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
		wm8994_write(0x200, 0x0011); // sysclk = MCLK1
	else 
		wm8994_write(0x200, 0x0001); // sysclk = MCLK1
//path
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0100);
	wm8994_write(0x28,  0x0003);  //IN1LN_TO_IN1L IN1LP_TO_IN1L
	wm8994_write(0x29,  0x0030);
	wm8994_write(0x2D,  0x0003);  //bit 1 IN2LP_TO_MIXOUTL bit 12 DAC1L_TO_HPOUT1L  0x0102 
	wm8994_write(0x2E,  0x0003);  //bit 1 IN2RP_TO_MIXOUTR bit 12 DAC1R_TO_HPOUT1R  0x0102
	wm8994_write(0x34,  0x000C);  //IN1L_TO_LINEOUT1P
	wm8994_write(0x60,  0x0022);
	wm8994_write(0x36,  0x000C);	
	wm8994_write(0x60,  0x00EE);
	wm8994_write(0x420, 0x0000);
	wm8994_write(0x601, 0x0001);
	wm8994_write(0x602, 0x0001);
//	wm8994_write(0x24,  0x0011);	
//volume
//	wm8994_write(0x25,  0x003F);
	wm8994_write(0x610, 0x01C0);  //DAC1 Left Volume bit0~7	
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
	wm8994_set_channel_vol();	
//other
	wm8994_write(0x4C,  0x9F25);		
//power
	wm8994_write(0x01,  0x3003);
	msleep(50);
	wm8994_write(0x01,  0x3033);	
	wm8994_write(0x02,  0x6210);
	wm8994_write(0x03,  0x1330);
	wm8994_write(0x04,  0x0303); // AIF1ADC1L_ENA=1, AIF1ADC1R_ENA=1, ADCL_ENA=1, ADCR_ENA=1
	wm8994_write(0x05,  0x0303);	
	wm8994_set_level_volume();		
}

void mainMIC_to_baseband_to_speakers_and_record(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_mainMIC_to_baseband_to_speakers_and_record)return;
	wm8994_current_mode=wm8994_mainMIC_to_baseband_to_speakers_and_record;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x01, 0x3023|wm8994_mic_VCC);
	wm8994_write(0x02, 0x6330);
	wm8994_write(0x03, 0x3330);
	wm8994_write(0x04, 0x0303);
	wm8994_write(0x1A, 0x014B);
	wm8994_write(0x1B, 0x014B);
	wm8994_write(0x1E, 0x0006);
	wm8994_write(0x22, 0x0000);
	wm8994_write(0x23, 0x0100);
 	wm8994_write(0x28, 0x0007);
	wm8994_write(0x2A, 0x0120);
	wm8994_write(0x2D, 0x0002);  //bit 1 IN2LP_TO_MIXOUTL
	wm8994_write(0x2E, 0x0002);  //bit 1 IN2RP_TO_MIXOUTR
	wm8994_write(0x34, 0x0004);
	wm8994_write(0x36, 0x000C);
	wm8994_write(0x200, 0x0001);
	wm8994_write(0x208, 0x000A);
 	wm8994_write(0x300, 0xC050);
	wm8994_set_volume(wm8994_current_mode,wm8994->call_vol,call_maxvol);

#ifdef CONFIG_SND_RK29_CODEC_SOC_MASTER
	wm8994_write(0x302, 0x4000);  // master = 0x4000 // slave= 0x0000
	wm8994_write(0x303, 0x0090);  // master lrck 16k
#endif

 	wm8994_write(0x606, 0x0002);
 	wm8994_write(0x607, 0x0002);
 	wm8994_write(0x620, 0x0000);
}

void BT_baseband(void)
{//
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_BT_baseband)return;

	wm8994_current_mode=wm8994_BT_baseband;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x01,  0x0023);
	wm8994_write(0x200, 0x0000);
	msleep(WM8994_DELAY);
//CLK	
	//AIF1CLK
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
	{
		wm8994_write(0x220, 0x0000); 
		wm8994_write(0x221, 0x0700);
		wm8994_write(0x222, 0); 
		wm8994_write(0x223, 0x0400);		
		wm8994_write(0x220, 0x0004); 
		msleep(10);
		wm8994_write(0x220, 0x0005); 			
		msleep(5);
		wm8994_write(0x200, 0x0010); // sysclk = MCLK1
	}
	wm8994_write(0x208, 0x0008); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x208, 0x000A); //DSP_FS1CLK_ENA=1, DSP_FSINTCLK_ENA=1
	wm8994_write(0x210, 0x0083); // SR=48KHz
	wm8994_write(0x300, 0xC010); // i2s 16 bits	
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
		wm8994_write(0x200, 0x0011); // sysclk = MCLK1
	else 
		wm8994_write(0x200, 0x0001); // sysclk = MCLK1
	//AIF2CLK use FLL2
	wm8994_write(0x204, 0x0000);
	msleep(WM8994_DELAY);
	wm8994_write(0x240, 0x0000);
	wm8994_write(0x241, 0x2F00);//48
	wm8994_write(0x242, 0);
	if(wm8994_sysclk_config() == wm8994_SYSCLK_3072M)
		wm8994_write(0x243, 0x0400);
	else	
		wm8994_write(0x243, 0x0100);
	wm8994_write(0x240, 0x0004);
	msleep(10);
	wm8994_write(0x240, 0x0005);
	msleep(5);
	wm8994_write(0x204, 0x0018);    // SMbus_16inx_16dat     Write  0x34      * AIF2 Clocking (1)(204H): 0011  AIF2CLK_SRC=10, AIF2CLK_INV=0, AIF2CLK_DIV=0, AIF2CLK_ENA=1
	wm8994_write(0x208, 0x000E);
	wm8994_write(0x211, 0x0003); 
	wm8994_write(0x312, 0x3000);    // SMbus_16inx_16dat     Write  0x34      * AIF2 Master/Slave(312H): 7000  AIF2_TRI=0, AIF2_MSTR=1, AIF2_CLK_FRC=0, AIF2_LRCLK_FRC=0
	msleep(30);
	wm8994_write(0x312, 0x7000);
	wm8994_write(0x313, 0x0020);    // SMbus_16inx_16dat     Write  0x34      * AIF2 BCLK DIV--------AIF1CLK/2
	wm8994_write(0x314, 0x0080);    // SMbus_16inx_16dat     Write  0x34      * AIF2 ADCLRCK DIV-----BCLK/128
	wm8994_write(0x315, 0x0080);
	wm8994_write(0x310, 0x0118);    //DSP/PCM; 16bits; ADC L channel = R channel;MODE A
	wm8994_write(0x204, 0x0019);    // SMbus_16inx_16dat     Write  0x34      * AIF2 Clocking (1)(204H): 0011  AIF2CLK_SRC=10, AIF2CLK_INV=0, AIF2CLK_DIV=0, AIF2CLK_ENA=1
/*	
	wm8994_write(0x310, 0x0118); 
	wm8994_write(0x204, 0x0001); 	
	wm8994_write(0x208, 0x000F); 	
	wm8994_write(0x211, 0x0009); 	
	wm8994_write(0x312, 0x7000); 	
	wm8994_write(0x313, 0x00F0); 
*/	
//GPIO
	wm8994_write(0x702, 0x2100);
	wm8994_write(0x703, 0x2100);
	wm8994_write(0x704, 0xA100);
	wm8994_write(0x707, 0xA100);
	wm8994_write(0x708, 0x2100);
	wm8994_write(0x709, 0x2100);
	wm8994_write(0x70A, 0x2100);
	wm8994_write(0x06,   0x000A);
//path
	wm8994_write(0x29,   0x0100);
	wm8994_write(0x2A,   0x0100);
	wm8994_write(0x28,   0x00C0);	
	wm8994_write(0x24,   0x0009);
	wm8994_write(0x29,   0x0130);
	wm8994_write(0x2A,   0x0130);
	wm8994_write(0x2D,   0x0001);
	wm8994_write(0x2E,   0x0001);
	wm8994_write(0x34,   0x0001);
	wm8994_write(0x36,   0x0004);
	wm8994_write(0x601, 0x0004);
	wm8994_write(0x602, 0x0001);
	wm8994_write(0x603, 0x000C);
	wm8994_write(0x604, 0x0010);
	wm8994_write(0x605, 0x0010);
	wm8994_write(0x620, 0x0000);
	wm8994_write(0x420, 0x0000);
//DRC&&EQ
	wm8994_write(0x440, 0x0018);
	wm8994_write(0x450, 0x0018);
	wm8994_write(0x540, 0x01BF); //open nosie gate
	wm8994_write(0x550, 0x01BF); //open nosie gate
	wm8994_write(0x480, 0x0000);
	wm8994_write(0x481, 0x0000);
	wm8994_write(0x4A0, 0x0000);
	wm8994_write(0x4A1, 0x0000);
	wm8994_write(0x520, 0x0000);
	wm8994_write(0x540, 0x0018);
	wm8994_write(0x580, 0x0000);
	wm8994_write(0x581, 0x0000);
//volume
	wm8994_set_volume(wm8994_current_mode,wm8994->call_vol,call_maxvol);
	wm8994_write(0x1E,   0x0006);
	wm8994_set_channel_vol();
	wm8994_write(0x22,   0x0000);
	wm8994_write(0x23,   0x0100);	
	wm8994_write(0x610, 0x01C0);
	wm8994_write(0x611, 0x01C0);
	wm8994_write(0x612, 0x01C0);
	wm8994_write(0x613, 0x01C0);	
//other
	wm8994_write(0x4C,   0x9F25);
	wm8994_write(0x60,   0x00EE);
	msleep(5);
//power	
	wm8994_write(0x01,   0x3023);
	wm8994_write(0x02,   0x63A0);
	wm8994_write(0x03,   0x33F0);
	wm8994_write(0x04,   0x3303);
	wm8994_write(0x05,   0x3303);	
}

void BT_baseband_old(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;	
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_BT_baseband)return;

	wm8994_current_mode=wm8994_BT_baseband;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x01, 0x3003);
	wm8994_write(0x02, 0x63A0);
	wm8994_write(0x03, 0x33F0);
	wm8994_write(0x04, 0x3303);
	wm8994_write(0x05, 0x3303);
	wm8994_write(0x06, 0x000A);
	wm8994_set_volume(wm8994_current_mode,wm8994->call_vol,call_maxvol);
	wm8994_write(0x1E, 0x0006);
	wm8994_write(0x29, 0x0100);
	wm8994_write(0x2A, 0x0100);

	wm8994_set_channel_vol();

#ifdef CONFIG_SND_BB_NORMAL_INPUT
	wm8994_write(0x28, 0x00C0);
#endif
#ifdef CONFIG_SND_BB_DIFFERENTIAL_INPUT
	/*vol = BT_incall_vol;
	if(vol>6)vol=6;
	if(vol<-12)vol=-12;
	wm8994_write(0x2B, (vol+12)/3 + 1);*/
	wm8994_write(0x28, 0x00CC);
#endif
	wm8994_write(0x22, 0x0000);
	wm8994_write(0x23, 0x0100);
	wm8994_write(0x24, 0x0009);
	wm8994_write(0x29, 0x0130);
	wm8994_write(0x2A, 0x0130);
	wm8994_write(0x2D, 0x0001);
	wm8994_write(0x2E, 0x0001);
	wm8994_write(0x34, 0x0001);
	wm8994_write(0x36, 0x0004);
	wm8994_write(0x4C, 0x9F25);
	wm8994_write(0x60, 0x00EE);
	wm8994_write(0x01, 0x3023);
	//roger_chen@20100524
	//8KHz, BCLK=8KHz*128=1024KHz, Fout=2.048MHz
	wm8994_write(0x200, 0x0001);
	wm8994_write(0x204, 0x0001);    // SMbus_16inx_16dat     Write  0x34      * AIF2 Clocking (1)(204H): 0011  AIF2CLK_SRC=00, AIF2CLK_INV=0, AIF2CLK_DIV=0, AIF2CLK_ENA=1
	wm8994_write(0x208, 0x000E);
	wm8994_write(0x220, 0x0000);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (1)(220H):  0005  FLL1_FRACN_ENA=0, FLL1_OSC_ENA=0, FLL1_ENA=0
	wm8994_write(0x221, 0x0700);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (2)(221H):  0700  FLL1_OUTDIV=2Fh, FLL1_CTRL_RATE=000, FLL1_FRATIO=000
	wm8994_write(0x222, 0x3126);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (3)(222H):  8FD5  FLL1_K=3126h
	wm8994_write(0x223, 0x0100);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (4)(223H):  00E0  FLL1_N=8h, FLL1_GAIN=0000

	wm8994_write(0x303, 0x0040);
	wm8994_write(0x304, 0x0040);
	wm8994_write(0x305, 0x0040);
	wm8994_write(0x300, 0xC050);    //DSP/PCM; 16bits; ADC L channel = R channel;MODE A

	wm8994_write(0x210, 0x0083);    // SMbus_16inx_16dat     Write  0x34      * SR=48KHz
	wm8994_write(0x220, 0x0004);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (1)(220H):  0005  FLL1_FRACN_ENA=1, FLL1_OSC_ENA=0, FLL1_ENA=0
	msleep(50);
	wm8994_write(0x220, 0x0005);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (1)(220H):  0005  FLL1_FRACN_ENA=1, FLL1_OSC_ENA=0, FLL1_ENA=1

	wm8994_write(0x240, 0x0000);
	wm8994_write(0x241, 0x2F00);
	wm8994_write(0x242, 0x3126);
	wm8994_write(0x243, 0x0100);

	wm8994_write(0x313, 0x0020);    // SMbus_16inx_16dat     Write  0x34      * AIF2 BCLK DIV--------AIF1CLK/2
	wm8994_write(0x314, 0x0080);    // SMbus_16inx_16dat     Write  0x34      * AIF2 ADCLRCK DIV-----BCLK/128
	wm8994_write(0x315, 0x0080);
	wm8994_write(0x310, 0x0118);    //DSP/PCM; 16bits; ADC L channel = R channel;MODE A

	wm8994_write(0x211, 0x0003);    // SMbus_16inx_16dat     Write  0x34      * SR=8KHz
	wm8994_write(0x240, 0x0004);
	msleep(50);
	wm8994_write(0x240, 0x0005);

	wm8994_write(0x200, 0x0011);
	wm8994_write(0x204, 0x0019);    // SMbus_16inx_16dat     Write  0x34      * AIF2 Clocking (1)(204H): 0011  AIF2CLK_SRC=10, AIF2CLK_INV=0, AIF2CLK_DIV=0, AIF2CLK_ENA=1
	wm8994_write(0x440, 0x0018);
	wm8994_write(0x450, 0x0018);
	wm8994_write(0x540, 0x01BF); //open nosie gate
	wm8994_write(0x550, 0x01BF); //open nosie gate
	wm8994_write(0x480, 0x0000);
	wm8994_write(0x481, 0x0000);
	wm8994_write(0x4A0, 0x0000);
	wm8994_write(0x4A1, 0x0000);
	wm8994_write(0x520, 0x0000);
	wm8994_write(0x540, 0x0018);
	wm8994_write(0x580, 0x0000);
	wm8994_write(0x581, 0x0000);
	wm8994_write(0x601, 0x0004);

	wm8994_write(0x602, 0x0001);

	wm8994_write(0x603, 0x000C);
	wm8994_write(0x604, 0x0010);
	wm8994_write(0x605, 0x0010);
	wm8994_write(0x610, 0x01C0);
	wm8994_write(0x611, 0x01C0);
	wm8994_write(0x612, 0x01C0);
	wm8994_write(0x613, 0x01C0);
	wm8994_write(0x620, 0x0000);
	wm8994_write(0x420, 0x0000);

	//roger_chen@20100519
	//enable AIF2 BCLK,LRCK
	//Rev.B and Rev.D is different
	wm8994_write(0x702, 0x2100);
	wm8994_write(0x703, 0x2100);

	wm8994_write(0x704, 0xA100);
	wm8994_write(0x707, 0xA100);
	wm8994_write(0x708, 0x2100);
	wm8994_write(0x709, 0x2100);
	wm8994_write(0x70A, 0x2100);
#ifdef CONFIG_SND_RK29_CODEC_SOC_MASTER
	wm8994_write(0x700, 0xA101);
	wm8994_write(0x705, 0xA101);
	wm8994_write(0x302, 0x3000); 
	msleep(30);
	wm8994_write(0x302, 0x7000); 
	msleep(30);
	wm8994_write(0x312, 0x3000);    // SMbus_16inx_16dat     Write  0x34      * AIF2 Master/Slave(312H): 7000  AIF2_TRI=0, AIF2_MSTR=1, AIF2_CLK_FRC=0, AIF2_LRCLK_FRC=0
	msleep(30);
	wm8994_write(0x312, 0x7000); 
#endif
}

void BT_baseband_and_record(void)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_BT_baseband_and_record)return;
	wm8994_current_mode=wm8994_BT_baseband_and_record;
	wm8994_reset();
	msleep(WM8994_DELAY);

	wm8994_write(0x01, 0x0023);
	wm8994_write(0x02, 0x63A0);
	wm8994_write(0x03, 0x30A0);
	wm8994_write(0x04, 0x3303);
	wm8994_write(0x05, 0x3002);
	wm8994_write(0x06, 0x000A);
	wm8994_set_volume(wm8994_current_mode,wm8994->call_vol,call_maxvol);
	wm8994_write(0x1E, 0x0006);
	wm8994_write(0x28, 0x00CC);
	wm8994_write(0x29, 0x0100);
	wm8994_write(0x2A, 0x0100);
	wm8994_write(0x2D, 0x0001);
	wm8994_write(0x34, 0x0001);
	wm8994_write(0x200, 0x0001);

	//roger_chen@20100524
	//8KHz, BCLK=8KHz*128=1024KHz, Fout=2.048MHz
	wm8994_write(0x204, 0x0001);    // SMbus_16inx_16dat     Write  0x34      * AIF2 Clocking (1)(204H): 0011  AIF2CLK_SRC=00, AIF2CLK_INV=0, AIF2CLK_DIV=0, AIF2CLK_ENA=1
	wm8994_write(0x208, 0x000F);
	wm8994_write(0x220, 0x0000);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (1)(220H):  0005  FLL1_FRACN_ENA=0, FLL1_OSC_ENA=0, FLL1_ENA=0
	wm8994_write(0x221, 0x2F00);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (2)(221H):  0700  FLL1_OUTDIV=2Fh, FLL1_CTRL_RATE=000, FLL1_FRATIO=000
	wm8994_write(0x222, 0x3126);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (3)(222H):  8FD5  FLL1_K=3126h
	wm8994_write(0x223, 0x0100);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (4)(223H):  00E0  FLL1_N=8h, FLL1_GAIN=0000
	wm8994_write(0x302, 0x4000);
	wm8994_write(0x303, 0x0090);    
	wm8994_write(0x310, 0xC118);  //DSP/PCM; 16bits; ADC L channel = R channel;MODE A
	wm8994_write(0x312, 0x4000);    // SMbus_16inx_16dat     Write  0x34      * AIF2 Master/Slave(312H): 7000  AIF2_TRI=0, AIF2_MSTR=1, AIF2_CLK_FRC=0, AIF2_LRCLK_FRC=0
	wm8994_write(0x313, 0x0020);    // SMbus_16inx_16dat     Write  0x34      * AIF2 BCLK DIV--------AIF1CLK/2
	wm8994_write(0x314, 0x0080);    // SMbus_16inx_16dat     Write  0x34      * AIF2 ADCLRCK DIV-----BCLK/128
	wm8994_write(0x315, 0x0080);    // SMbus_16inx_16dat     Write  0x34      * AIF2 DACLRCK DIV-----BCLK/128
	wm8994_write(0x210, 0x0003);    // SMbus_16inx_16dat     Write  0x34      * SR=8KHz
	wm8994_write(0x220, 0x0004);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (1)(220H):  0005  FLL1_FRACN_ENA=1, FLL1_OSC_ENA=0, FLL1_ENA=0
	msleep(WM8994_DELAY);
	wm8994_write(0x220, 0x0005);    // SMbus_16inx_16dat     Write  0x34      * FLL1 Control (1)(220H):  0005  FLL1_FRACN_ENA=1, FLL1_OSC_ENA=0, FLL1_ENA=1
	wm8994_write(0x204, 0x0011);    // SMbus_16inx_16dat     Write  0x34      * AIF2 Clocking (1)(204H): 0011  AIF2CLK_SRC=10, AIF2CLK_INV=0, AIF2CLK_DIV=0, AIF2CLK_ENA=1

	wm8994_write(0x440, 0x0018);
	wm8994_write(0x450, 0x0018);
	wm8994_write(0x480, 0x0000);
	wm8994_write(0x481, 0x0000);
	wm8994_write(0x4A0, 0x0000);
	wm8994_write(0x4A1, 0x0000);
	wm8994_write(0x520, 0x0000);
	wm8994_write(0x540, 0x0018);
	wm8994_write(0x580, 0x0000);
	wm8994_write(0x581, 0x0000);
	wm8994_write(0x601, 0x0004);
	wm8994_write(0x603, 0x000C);
	wm8994_write(0x604, 0x0010);
	wm8994_write(0x605, 0x0010);
	wm8994_write(0x606, 0x0003);
	wm8994_write(0x607, 0x0003);
	wm8994_write(0x610, 0x01C0);
	wm8994_write(0x612, 0x01C0);
	wm8994_write(0x613, 0x01C0);
	wm8994_write(0x620, 0x0000);

	//roger_chen@20100519
	//enable AIF2 BCLK,LRCK
	//Rev.B and Rev.D is different
	wm8994_write(0x702, 0xA100);    
	wm8994_write(0x703, 0xA100);

	wm8994_write(0x704, 0xA100);
	wm8994_write(0x707, 0xA100);
	wm8994_write(0x708, 0x2100);
	wm8994_write(0x709, 0x2100);
	wm8994_write(0x70A, 0x2100);
}

#else //PCM_BB

/******************PCM BB BEGIN*****************/

void handsetMIC_to_baseband_to_headset(void) //pcmbaseband
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_handsetMIC_to_baseband_to_headset)return;
	wm8994_current_mode=wm8994_handsetMIC_to_baseband_to_headset;
	wm8994_reset();
	msleep(50);
	
	wm8994_write(0x01,  0x0003|wm8994_mic_VCC);  
	msleep(50);
	wm8994_write(0x221, 0x0700);  
	wm8994_write(0x222, 0x3127);	
	wm8994_write(0x223, 0x0100);	
	wm8994_write(0x220, 0x0004);
	msleep(50);
	wm8994_write(0x220, 0x0005);  

	wm8994_write(0x01,  0x0303|wm8994_mic_VCC);  ///0x0303);	 // sysclk = fll (bit4 =1)   0x0011 
	wm8994_write(0x02,  0x0240);
	wm8994_write(0x03,  0x0030);
	wm8994_write(0x04,  0x3003);
	wm8994_write(0x05,  0x3003);  // i2s 16 bits
	wm8994_write(0x18,  0x010B);
	wm8994_write(0x28,  0x0030);
	wm8994_write(0x29,  0x0020);
	wm8994_write(0x2D,  0x0100);  //0x0100);DAC1L_TO_HPOUT1L    ;;;bit 8 
	wm8994_write(0x2E,  0x0100);  //0x0100);DAC1R_TO_HPOUT1R    ;;;bit 8 
	wm8994_write(0x4C,  0x9F25);
	wm8994_write(0x60,  0x00EE);
	wm8994_write(0x200, 0x0001);	
	wm8994_write(0x204, 0x0001);
	wm8994_write(0x208, 0x0007);	
	wm8994_write(0x520, 0x0000);	
	wm8994_write(0x601, 0x0004);  //AIF2DACL_TO_DAC1L
	wm8994_write(0x602, 0x0004);  //AIF2DACR_TO_DAC1R

	wm8994_write(0x610, 0x01C0);  //DAC1 Left Volume bit0~7
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
	wm8994_write(0x612, 0x01C0);  //DAC2 Left Volume bit0~7	
	wm8994_write(0x613, 0x01C0);  //DAC2 Right Volume bit0~7

	wm8994_write(0x702, 0xC100);
	wm8994_write(0x703, 0xC100);
	wm8994_write(0x704, 0xC100);
	wm8994_write(0x706, 0x4100);
	wm8994_write(0x204, 0x0011);
	wm8994_write(0x211, 0x0009);
	#ifdef TD688_MODE
	wm8994_write(0x310, 0x4108); ///0x4118);  ///interface dsp mode 16bit
	#endif
	#ifdef CHONGY_MODE
	wm8994_write(0x310, 0x4118); ///0x4118);  ///interface dsp mode 16bit
	#endif	
	#ifdef MU301_MODE
	wm8994_write(0x310, 0x4118); ///0x4118);  ///interface dsp mode 16bit
	wm8994_write(0x241, 0x2f04);
	wm8994_write(0x242, 0x0000);
	wm8994_write(0x243, 0x0300);
	wm8994_write(0x240, 0x0004);
	msleep(40);
	wm8994_write(0x240, 0x0005);
	wm8994_write(0x204, 0x0019); 
	wm8994_write(0x211, 0x0003);
	wm8994_write(0x244, 0x0c83);
	wm8994_write(0x620, 0x0000);
	#endif
	#ifdef THINKWILL_M800_MODE
	wm8994_write(0x310, 0x4118); ///0x4118);  ///interface dsp mode 16bit
	#endif
	wm8994_write(0x313, 0x00F0);
	wm8994_write(0x314, 0x0020);
	wm8994_write(0x315, 0x0020);
	wm8994_write(0x603, 0x018c);  ///0x000C);  //Rev.D ADCL SideTone
	wm8994_write(0x604, 0x0010); //XX
	wm8994_write(0x605, 0x0010); //XX
	wm8994_write(0x621, 0x0000);  //0x0001);   ///0x0000);
	wm8994_write(0x317, 0x0003);
	wm8994_write(0x312, 0x0000); /// as slave  ///0x4000);  //AIF2 SET AS MASTER
	

}

void handsetMIC_to_baseband_to_headset_and_record(void) //pcmbaseband
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_handsetMIC_to_baseband_to_headset_and_record)return;
	wm8994_current_mode=wm8994_handsetMIC_to_baseband_to_headset_and_record;
	wm8994_reset();
	msleep(50);

	wm8994_write(0x01,  0x0003|wm8994_mic_VCC);  
	msleep(50);
	wm8994_write(0x221, 0x0700);  //MCLK=12MHz
	wm8994_write(0x222, 0x3127);	
	wm8994_write(0x223, 0x0100);	
	wm8994_write(0x220, 0x0004);
	msleep(50);
	wm8994_write(0x220, 0x0005);  

	wm8994_write(0x01,  0x0303|wm8994_mic_VCC);	 
	wm8994_write(0x02,  0x0240);
	wm8994_write(0x03,  0x0030);
	wm8994_write(0x04,  0x3003);
	wm8994_write(0x05,  0x3003); 
	wm8994_write(0x18,  0x010B);  // 0x011F=+30dB for MIC
	wm8994_write(0x28,  0x0030);
	wm8994_write(0x29,  0x0020);
	wm8994_write(0x2D,  0x0100);
	wm8994_write(0x2E,  0x0100);
	wm8994_write(0x4C,  0x9F25);
	wm8994_write(0x60,  0x00EE);
	wm8994_write(0x200, 0x0001);	
	wm8994_write(0x204, 0x0001);
	wm8994_write(0x208, 0x0007);	
	wm8994_write(0x520, 0x0000);	
	wm8994_write(0x601, 0x0004);
	wm8994_write(0x602, 0x0004);

	wm8994_write(0x610, 0x01C0);  //DAC1 Left Volume bit0~7
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
	wm8994_write(0x612, 0x01C0);  //DAC2 Left Volume bit0~7	
	wm8994_write(0x613, 0x01C0);  //DAC2 Right Volume bit0~7

	wm8994_write(0x700, 0x8141);  //SYNC issue, AIF1 ADCLRC1 from LRCK1
	wm8994_write(0x702, 0xC100);
	wm8994_write(0x703, 0xC100);
	wm8994_write(0x704, 0xC100);
	wm8994_write(0x706, 0x4100);
	wm8994_write(0x204, 0x0011);  //AIF2 MCLK=FLL1
	wm8994_write(0x211, 0x0009);  //LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x310, 0x4118);  //DSP/PCM 16bits
	wm8994_write(0x313, 0x00F0);
	wm8994_write(0x314, 0x0020);
	wm8994_write(0x315, 0x0020);

	wm8994_write(0x603, 0x018c);  ///0x000C);  //Rev.D ADCL SideTone
	wm8994_write(0x604, 0x0010);
	wm8994_write(0x605, 0x0010);
	wm8994_write(0x621, 0x0000);
	//wm8994_write(0x317, 0x0003);
	//wm8994_write(0x312, 0x4000);  //AIF2 SET AS MASTER
////AIF1
	wm8994_write(0x04,   0x3303);
	wm8994_write(0x200,  0x0001);
	wm8994_write(0x208,  0x000F);
	wm8994_write(0x210,  0x0009);  //LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x300,  0x0118);  //DSP/PCM 16bits, R ADC = L ADC 
	wm8994_write(0x606,  0x0003);	
	wm8994_write(0x607,  0x0003);

////AIF1 Master Clock(SR=8KHz)
	wm8994_write(0x200,  0x0011);
	wm8994_write(0x302,  0x4000);
	wm8994_write(0x303,  0x00F0);
	wm8994_write(0x304,  0x0020);
	wm8994_write(0x305,  0x0020);

////AIF1 DAC1 HP
	wm8994_write(0x05,   0x3303);
	wm8994_write(0x420,  0x0000);
	wm8994_write(0x601,  0x0001);
	wm8994_write(0x602,  0x0001);
	wm8994_write(0x700,  0x8140);//SYNC issue, AIF1 ADCLRC1 from FLL after AIF1 MASTER!!!
	
	
}

void mainMIC_to_baseband_to_earpiece(void) //pcmbaseband
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_mainMIC_to_baseband_to_earpiece)return;
	wm8994_current_mode=wm8994_mainMIC_to_baseband_to_earpiece;
	wm8994_reset();
	msleep(50);

	wm8994_write(0x01,  0x0003|wm8994_mic_VCC);  
	msleep(50);
	wm8994_write(0x221, 0x0700);  //MCLK=12MHz
	wm8994_write(0x222, 0x3127);	
	wm8994_write(0x223, 0x0100);	
	wm8994_write(0x220, 0x0004);
	msleep(50);
	wm8994_write(0x220, 0x0005);  

	wm8994_write(0x01,  0x0803|wm8994_mic_VCC);   ///0x0813);	 
	wm8994_write(0x02,  0x0240);   ///0x0110);
	wm8994_write(0x03,  0x00F0);
	wm8994_write(0x04,  0x3003);
	wm8994_write(0x05,  0x3003); 
	wm8994_write(0x18,  0x011F);
	wm8994_write(0x1F,  0x0000); 
	wm8994_write(0x28,  0x0030);  ///0x0003);
	wm8994_write(0x29,  0x0020);
	wm8994_write(0x2D,  0x0001);
	wm8994_write(0x2E,  0x0001);
	wm8994_write(0x33,  0x0018);
	wm8994_write(0x200, 0x0001);
	wm8994_write(0x204, 0x0001);
	wm8994_write(0x208, 0x0007);
	wm8994_write(0x520, 0x0000);
	wm8994_write(0x601, 0x0004);
	wm8994_write(0x602, 0x0004);

	wm8994_write(0x610, 0x01C0);  //DAC1 Left Volume bit0~7
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
	wm8994_write(0x612, 0x01C0);  //DAC2 Left Volume bit0~7	
	wm8994_write(0x613, 0x01C0);  //DAC2 Right Volume bit0~7

	wm8994_write(0x702, 0xC100);
	wm8994_write(0x703, 0xC100);
	wm8994_write(0x704, 0xC100);
	wm8994_write(0x706, 0x4100);
	wm8994_write(0x204, 0x0011);  //AIF2 MCLK=FLL1
	wm8994_write(0x211, 0x0009);  //LRCK=8KHz, Rate=MCLK/1536
	#ifdef TD688_MODE
	wm8994_write(0x310, 0x4108); ///0x4118);  ///interface dsp mode 16bit
	#endif
	#ifdef CHONGY_MODE
	wm8994_write(0x310, 0x4118); ///0x4118);  ///interface dsp mode 16bit
	#endif
	#ifdef MU301_MODE
	wm8994_write(0x310, 0x4118); ///0x4118);  ///interface dsp mode 16bit
	wm8994_write(0x241, 0x2f04);
	wm8994_write(0x242, 0x0000);
	wm8994_write(0x243, 0x0300);
	wm8994_write(0x240, 0x0004);
	msleep(40);
	wm8994_write(0x240, 0x0005);
	wm8994_write(0x204, 0x0019); 
	wm8994_write(0x211, 0x0003);
	wm8994_write(0x244, 0x0c83);
	wm8994_write(0x620, 0x0000);
	#endif
	#ifdef THINKWILL_M800_MODE
	wm8994_write(0x310, 0x4118); ///0x4118);  ///interface dsp mode 16bit
	#endif
	wm8994_write(0x313, 0x00F0);
	wm8994_write(0x314, 0x0020);
	wm8994_write(0x315, 0x0020);

	wm8994_write(0x603, 0x018C);  //Rev.D ADCL SideTone
	wm8994_write(0x604, 0x0010);
	wm8994_write(0x605, 0x0010);
	wm8994_write(0x621, 0x0000);  ///0x0001);
	wm8994_write(0x317, 0x0003);
	wm8994_write(0x312, 0x0000);  //AIF2 SET AS MASTER
	
	
}

void mainMIC_to_baseband_to_earpiece_and_record(void) //pcmbaseband
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_mainMIC_to_baseband_to_earpiece_and_record)return;
	wm8994_current_mode=wm8994_mainMIC_to_baseband_to_earpiece_and_record;
	wm8994_reset();
	msleep(50);

	wm8994_write(0x01,  0x0003|wm8994_mic_VCC);  
	msleep(50);
	wm8994_write(0x221, 0x0700);  //MCLK=12MHz
	wm8994_write(0x222, 0x3127);
	wm8994_write(0x223, 0x0100);
	wm8994_write(0x220, 0x0004);
	msleep(50);
	wm8994_write(0x220, 0x0005);  

	wm8994_write(0x01,  0x0803|wm8994_mic_VCC);
	wm8994_write(0x02,  0x0110);
	wm8994_write(0x03,  0x00F0);
	wm8994_write(0x04,  0x3003);
	wm8994_write(0x05,  0x3003); 
	wm8994_write(0x1A,  0x010B); 
	wm8994_write(0x1F,  0x0000); 
	wm8994_write(0x28,  0x0003);
	wm8994_write(0x2A,  0x0020);
	wm8994_write(0x2D,  0x0001);
	wm8994_write(0x2E,  0x0001);
	wm8994_write(0x33,  0x0018);
	wm8994_write(0x200, 0x0001);	
	wm8994_write(0x204, 0x0001);
	wm8994_write(0x208, 0x0007);	
	wm8994_write(0x520, 0x0000);	
	wm8994_write(0x601, 0x0004);
	wm8994_write(0x602, 0x0004);

	wm8994_write(0x610, 0x01C0);  //DAC1 Left Volume bit0~7
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
	wm8994_write(0x612, 0x01C0);  //DAC2 Left Volume bit0~7	
	wm8994_write(0x613, 0x01C0);  //DAC2 Right Volume bit0~7

	wm8994_write(0x702, 0xC100);
	wm8994_write(0x703, 0xC100);
	wm8994_write(0x704, 0xC100);
	wm8994_write(0x706, 0x4100);
	wm8994_write(0x204, 0x0011);  //AIF2 MCLK=FLL1
	wm8994_write(0x211, 0x0009);  //LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x310, 0x4118);  //DSP/PCM 16bits
	wm8994_write(0x313, 0x00F0);
	wm8994_write(0x314, 0x0020);
	wm8994_write(0x315, 0x0020);

	wm8994_write(0x603, 0x018C);  //Rev.D ADCL SideTone
	wm8994_write(0x604, 0x0010);
	wm8994_write(0x605, 0x0010);
	wm8994_write(0x621, 0x0001);

////AIF1
	wm8994_write(0x04,   0x3303);
	wm8994_write(0x200,  0x0001);
	wm8994_write(0x208,  0x000F);
	wm8994_write(0x210,  0x0009);  //LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x300,  0xC118);  //DSP/PCM 16bits, R ADC = L ADC 
	wm8994_write(0x606,  0x0003);	
	wm8994_write(0x607,  0x0003);

////AIF1 Master Clock(SR=8KHz)
	wm8994_write(0x200,  0x0011);
	wm8994_write(0x302,  0x4000);
	wm8994_write(0x303,  0x00F0);
	wm8994_write(0x304,  0x0020);
	wm8994_write(0x305,  0x0020);

////AIF1 DAC1 HP
	wm8994_write(0x05,   0x3303);
	wm8994_write(0x420,  0x0000);
	wm8994_write(0x601,  0x0001);
	wm8994_write(0x602,  0x0001);
	wm8994_write(0x700,  0x8140);//SYNC issue, AIF1 ADCLRC1 from FLL after AIF1 MASTER!!!
	
	
}

void mainMIC_to_baseband_to_speakers(void) //pcmbaseband
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_mainMIC_to_baseband_to_speakers)return;
	wm8994_current_mode=wm8994_mainMIC_to_baseband_to_speakers;
	wm8994_reset();
	msleep(50);

	wm8994_write(0x01,  0x0003|wm8994_mic_VCC);  //0x0013);  
	msleep(50);
	wm8994_write(0x221, 0x0700);  //MCLK=12MHz   //FLL1 CONTRLO(2)
	wm8994_write(0x222, 0x3127);  //FLL1 CONTRLO(3)	
	wm8994_write(0x223, 0x0100);  //FLL1 CONTRLO(4)	
	wm8994_write(0x220, 0x0004);  //FLL1 CONTRLO(1)
	msleep(50);
	wm8994_write(0x220, 0x0005);  //FLL1 CONTRLO(1)

	wm8994_write(0x01,  0x3003|wm8994_mic_VCC);	 
	wm8994_write(0x02,  0x0110);
	wm8994_write(0x03,  0x0030);  ///0x0330);
	wm8994_write(0x04,  0x3003);
	wm8994_write(0x05,  0x3003); 
	wm8994_write(0x1A,  0x011F);
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0100);  ///0x0000);
	//wm8994_write(0x25,  0x0152);
	wm8994_write(0x28,  0x0003);
	wm8994_write(0x2A,  0x0020);
	wm8994_write(0x2D,  0x0001);
	wm8994_write(0x2E,  0x0001);
	wm8994_write(0x36,  0x000C);  //MIXOUTL_TO_SPKMIXL  MIXOUTR_TO_SPKMIXR
	wm8994_write(0x200, 0x0001);  //AIF1 CLOCKING(1)
	wm8994_write(0x204, 0x0001);  //AIF2 CLOCKING(1)
	wm8994_write(0x208, 0x0007);  //CLOCKING(1)
	wm8994_write(0x520, 0x0000);  //AIF2 DAC FILTERS(1)
	wm8994_write(0x601, 0x0004);  //AIF2DACL_DAC1L
	wm8994_write(0x602, 0x0004);  //AIF2DACR_DAC1R

	wm8994_write(0x610, 0x01C0);  //DAC1 Left Volume bit0~7
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
	wm8994_write(0x612, 0x01C0);  //DAC2 Left Volume bit0~7	
	wm8994_write(0x613, 0x01C0);  //DAC2 Right Volume bit0~7

	wm8994_write(0x702, 0xC100);  //GPIO3
	wm8994_write(0x703, 0xC100);  //GPIO4
	wm8994_write(0x704, 0xC100);  //GPIO5
	wm8994_write(0x706, 0x4100);  //GPIO7
	wm8994_write(0x204, 0x0011);  //AIF2 MCLK=FLL1
	wm8994_write(0x211, 0x0009);  //LRCK=8KHz, Rate=MCLK/1536
	#ifdef TD688_MODE
	wm8994_write(0x310, 0xc108); ///0x4118);  ///interface dsp mode 16bit
	#endif
	#ifdef CHONGY_MODE
	wm8994_write(0x310, 0xc018); ///0x4118);  ///interface dsp mode 16bit
	#endif
	#ifdef MU301_MODE
	wm8994_write(0x310, 0xc118); ///0x4118);  ///interface dsp mode 16bit
	wm8994_write(0x241, 0x2f04);
	wm8994_write(0x242, 0x0000);
	wm8994_write(0x243, 0x0300);
	wm8994_write(0x240, 0x0004);
	msleep(40);
	wm8994_write(0x240, 0x0005);
	wm8994_write(0x204, 0x0019);
	wm8994_write(0x211, 0x0003);
	wm8994_write(0x244, 0x0c83);
	wm8994_write(0x620, 0x0000);
	#endif
	#ifdef THINKWILL_M800_MODE
	wm8994_write(0x310, 0xc118); ///0x4118);  ///interface dsp mode 16bit
	#endif
	wm8994_write(0x313, 0x00F0);  //AIF2BCLK
	wm8994_write(0x314, 0x0020);  //AIF2ADCLRCK
	wm8994_write(0x315, 0x0020);  //AIF2DACLRCLK

	wm8994_write(0x603, 0x018C);  //Rev.D ADCL SideTone
	wm8994_write(0x604, 0x0020);  ///0x0010);  //ADC2_TO_DAC2L
	wm8994_write(0x605, 0x0020);  //0x0010);  //ADC2_TO_DAC2R
	wm8994_write(0x621, 0x0000);  ///0x0001);
	wm8994_write(0x317, 0x0003);
	wm8994_write(0x312, 0x0000);  //AIF2 SET AS MASTER


}

void mainMIC_to_baseband_to_speakers_and_record(void) //pcmbaseband
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_mainMIC_to_baseband_to_speakers_and_record)return;
	wm8994_current_mode=wm8994_mainMIC_to_baseband_to_speakers_and_record;
	wm8994_reset();
	msleep(50);

	wm8994_write(0x01,  0x0003|wm8994_mic_VCC);  
	msleep(50);
	wm8994_write(0x221, 0x0700);  //MCLK=12MHz
	wm8994_write(0x222, 0x3127);	
	wm8994_write(0x223, 0x0100);	
	wm8994_write(0x220, 0x0004);
	msleep(50);
	wm8994_write(0x220, 0x0005);  

	wm8994_write(0x02,  0x0110);
	wm8994_write(0x03,  0x0330);
	wm8994_write(0x04,  0x3003);
	wm8994_write(0x05,  0x3003); 
	wm8994_write(0x1A,  0x010B); 
	wm8994_write(0x22,  0x0000);
	wm8994_write(0x23,  0x0000);
	wm8994_write(0x28,  0x0003);
	wm8994_write(0x2A,  0x0020);
	wm8994_write(0x2D,  0x0001);
	wm8994_write(0x2E,  0x0001);
	wm8994_write(0x36,  0x000C);
	wm8994_write(0x200, 0x0001);	
	wm8994_write(0x204, 0x0001);
	wm8994_write(0x208, 0x0007);	
	wm8994_write(0x520, 0x0000);	
	wm8994_write(0x601, 0x0004);
	wm8994_write(0x602, 0x0004);

	wm8994_write(0x610, 0x01C0);  //DAC1 Left Volume bit0~7
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
	wm8994_write(0x612, 0x01C0);  //DAC2 Left Volume bit0~7	
	wm8994_write(0x613, 0x01C0);  //DAC2 Right Volume bit0~7

	wm8994_write(0x700, 0x8141);
	wm8994_write(0x702, 0xC100);
	wm8994_write(0x703, 0xC100);
	wm8994_write(0x704, 0xC100);
	wm8994_write(0x706, 0x4100);
	wm8994_write(0x204, 0x0011);  //AIF2 MCLK=FLL1
	wm8994_write(0x211, 0x0009);  //LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x310, 0x4118);  //DSP/PCM 16bits
	wm8994_write(0x313, 0x00F0);
	wm8994_write(0x314, 0x0020);
	wm8994_write(0x315, 0x0020);

	wm8994_write(0x603, 0x018C);  //Rev.D ADCL SideTone
	wm8994_write(0x604, 0x0010);
	wm8994_write(0x605, 0x0010);
	wm8994_write(0x621, 0x0001);

////AIF1
	wm8994_write(0x04,   0x3303);
	wm8994_write(0x200,  0x0001);
	wm8994_write(0x208,  0x000F);
	wm8994_write(0x210,  0x0009);  //LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x300,  0xC118);  //DSP/PCM 16bits, R ADC = L ADC 
	wm8994_write(0x606,  0x0003);	
	wm8994_write(0x607,  0x0003);

////AIF1 Master Clock(SR=8KHz)
	wm8994_write(0x200,  0x0011);
	wm8994_write(0x302,  0x4000);
	wm8994_write(0x303,  0x00F0);
	wm8994_write(0x304,  0x0020);
	wm8994_write(0x305,  0x0020);

////AIF1 DAC1 HP
	wm8994_write(0x05,   0x3303);
	wm8994_write(0x420,  0x0000);
	wm8994_write(0x601,  0x0001);
	wm8994_write(0x602,  0x0001);
	wm8994_write(0x700,  0x8140);//SYNC issue, AIF1 ADCLRC1 from FLL after AIF1 MASTER!!!
	
}

void BT_baseband(void) //pcmbaseband
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_BT_baseband)return;
	wm8994_current_mode=wm8994_BT_baseband;
	wm8994_reset();
	msleep(50);

	wm8994_write(0x01 ,0x0003);
	msleep (50);

	wm8994_write(0x200 ,0x0001);
	wm8994_write(0x221 ,0x0700);//MCLK=12MHz
	wm8994_write(0x222 ,0x3127);
	wm8994_write(0x223 ,0x0100);
	wm8994_write(0x220 ,0x0004);
	msleep (50);
	wm8994_write(0x220 ,0x0005); 

	wm8994_write(0x02 ,0x0000); 
	wm8994_write(0x200 ,0x0011);// AIF1 MCLK=FLL1
	wm8994_write(0x210 ,0x0009);// LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x300 ,0x4018);// DSP/PCM 16bits

	wm8994_write(0x204 ,0x0011);// AIF2 MCLK=FLL1
	wm8994_write(0x211 ,0x0009);// LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x310 ,0x4118);// DSP/PCM 16bits
	wm8994_write(0x208 ,0x000F); 

/////AIF1
	wm8994_write(0x700 ,0x8101);
/////AIF2
	wm8994_write(0x702 ,0xC100);
	wm8994_write(0x703 ,0xC100);
	wm8994_write(0x704 ,0xC100);
	wm8994_write(0x706 ,0x4100);
/////AIF3
	wm8994_write(0x707 ,0xA100); 
	wm8994_write(0x708 ,0xA100);
	wm8994_write(0x709 ,0xA100); 
	wm8994_write(0x70A ,0xA100);

	wm8994_write(0x06 ,0x0001);

	wm8994_write(0x02 ,0x0300);
	wm8994_write(0x03 ,0x0030);
	wm8994_write(0x04 ,0x3301);//ADCL off
	wm8994_write(0x05 ,0x3301);//DACL off

	wm8994_write(0x2A ,0x0005);

	wm8994_write(0x313 ,0x00F0);
	wm8994_write(0x314 ,0x0020);
	wm8994_write(0x315 ,0x0020);

	wm8994_write(0x2E ,0x0001);
	wm8994_write(0x420 ,0x0000);
	wm8994_write(0x520 ,0x0000);
	wm8994_write(0x601 ,0x0001);
	wm8994_write(0x602 ,0x0001);
	wm8994_write(0x604 ,0x0001);
	wm8994_write(0x605 ,0x0001);
	wm8994_write(0x607 ,0x0002);
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
	wm8994_write(0x612, 0x01C0);  //DAC2 Left Volume bit0~7	
	wm8994_write(0x613, 0x01C0);  //DAC2 Right Volume bit0~7


	wm8994_write(0x312 ,0x4000);

	wm8994_write(0x606 ,0x0001);
	wm8994_write(0x607 ,0x0003);//R channel for data mix/CPU record data


////////////HP output test
	wm8994_write(0x01 ,0x0303);
	wm8994_write(0x4C ,0x9F25);
	wm8994_write(0x60 ,0x00EE);
///////////end HP test

}

void BT_baseband_and_record(void) //pcmbaseband
{
	DBG("%s::%d\n",__FUNCTION__,__LINE__);

	if(wm8994_current_mode==wm8994_BT_baseband_and_record)return;
	wm8994_current_mode=wm8994_BT_baseband_and_record;
	wm8994_reset();
	msleep(50);

	wm8994_write(0x01  ,0x0003);
	msleep (50);

	wm8994_write(0x200 ,0x0001);
	wm8994_write(0x221 ,0x0700);//MCLK=12MHz
	wm8994_write(0x222 ,0x3127);
	wm8994_write(0x223 ,0x0100);
	wm8994_write(0x220 ,0x0004);
	msleep (50);
	wm8994_write(0x220 ,0x0005); 

	wm8994_write(0x02 ,0x0000); 
	wm8994_write(0x200 ,0x0011);// AIF1 MCLK=FLL1
	wm8994_write(0x210 ,0x0009);// LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x300 ,0x4018);// DSP/PCM 16bits

	wm8994_write(0x204 ,0x0011);// AIF2 MCLK=FLL1
	wm8994_write(0x211 ,0x0009);// LRCK=8KHz, Rate=MCLK/1536
	wm8994_write(0x310 ,0x4118);// DSP/PCM 16bits
	wm8994_write(0x208 ,0x000F); 

/////AIF1
	wm8994_write(0x700 ,0x8101);
/////AIF2
	wm8994_write(0x702 ,0xC100);
	wm8994_write(0x703 ,0xC100);
	wm8994_write(0x704 ,0xC100);
	wm8994_write(0x706 ,0x4100);
/////AIF3
	wm8994_write(0x707 ,0xA100); 
	wm8994_write(0x708 ,0xA100);
	wm8994_write(0x709 ,0xA100); 
	wm8994_write(0x70A ,0xA100);

	wm8994_write(0x06 ,0x0001);
	wm8994_write(0x02 ,0x0300);
	wm8994_write(0x03 ,0x0030);
	wm8994_write(0x04 ,0x3301);//ADCL off
	wm8994_write(0x05 ,0x3301);//DACL off
	wm8994_write(0x2A ,0x0005);

	wm8994_write(0x313 ,0x00F0);
	wm8994_write(0x314 ,0x0020);
	wm8994_write(0x315 ,0x0020);

	wm8994_write(0x2E  ,0x0001);
	wm8994_write(0x420 ,0x0000);
	wm8994_write(0x520 ,0x0000);
	wm8994_write(0x602 ,0x0001);
	wm8994_write(0x604 ,0x0001);
	wm8994_write(0x605 ,0x0001);
	wm8994_write(0x607 ,0x0002);
	wm8994_write(0x611, 0x01C0);  //DAC1 Right Volume bit0~7
	wm8994_write(0x612, 0x01C0);  //DAC2 Left Volume bit0~7	
	wm8994_write(0x613, 0x01C0);  //DAC2 Right Volume bit0~7

	wm8994_write(0x312 ,0x4000);

	wm8994_write(0x606 ,0x0001);
	wm8994_write(0x607 ,0x0003);//R channel for data mix/CPU record data
////////////HP output test
	wm8994_write(0x01 ,0x0303);
	wm8994_write(0x4C ,0x9F25); 
	wm8994_write(0x60 ,0x00EE); 
///////////end HP test

}
#endif //PCM_BB

#define SOC_DOUBLE_SWITCH_WM8994CODEC(xname, route) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.info = snd_soc_info_route, \
	.get = snd_soc_get_route, .put = snd_soc_put_route, \
	.private_value = route }

int snd_soc_info_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0;
	return 0;
}

int snd_soc_get_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int snd_soc_put_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	char route = kcontrol->private_value & 0xff;
	mutex_lock(&wm8994->route_lock);
	wm8994->kcontrol = kcontrol;//save rount
	
	if(wm8994->First_Poweron == 1 && route == SPEAKER_NORMAL )
	{//First start & Poweron  mast disable wm8994
		PA_ctrl(GPIO_LOW);
		wm8994_write(0,0);
		msleep(50);
		goto out;
	}
	//before set the route -- disable PA
	switch(route)
	{
		case HEADSET_NORMAL:
		case HEADSET_INCALL:
		case EARPIECE_INCALL:
			PA_ctrl(GPIO_LOW);
			break;
		default:
			break;
	}
	//set rount
	switch(route)
	{
		case SPEAKER_NORMAL: 						//AP-> 8994Codec -> Speaker
		case SPEAKER_RINGTONE:			
		case EARPIECE_RINGTONE:	
			recorder_and_AP_to_speakers();
			break;
		case SPEAKER_INCALL: 						//BB-> 8994Codec -> Speaker
			mainMIC_to_baseband_to_speakers();
			break;					
		case HEADSET_NORMAL:						//AP-> 8994Codec -> Headset
			recorder_and_AP_to_headset();
			break;
		case HEADSET_INCALL:						//AP-> 8994Codec -> Headset
			handsetMIC_to_baseband_to_headset();
			break;	    
		case EARPIECE_INCALL:						//BB-> 8994Codec -> EARPIECE
			mainMIC_to_baseband_to_earpiece();
			break;
		case EARPIECE_NORMAL:						//BB-> 8994Codec -> EARPIECE
			switch(wm8994_current_mode)
			{
				case wm8994_handsetMIC_to_baseband_to_headset:
				case wm8994_mainMIC_to_baseband_to_headset:
					recorder_and_AP_to_headset();
					break;
				default:
					recorder_and_AP_to_speakers();	
					break;
			}
			break;   	
		case BLUETOOTH_SCO_INCALL:					//BB-> 8994Codec -> BLUETOOTH_SCO 
			BT_baseband();
			break;   
		case MIC_CAPTURE:
			switch(wm8994_current_mode)
			{
				case wm8994_AP_to_headset:
					recorder_and_AP_to_headset();
					break;
				case wm8994_AP_to_speakers:
					recorder_and_AP_to_speakers();
					break;
				case wm8994_recorder_and_AP_to_speakers:
				case wm8994_recorder_and_AP_to_headset:
					break;
				default:
					recorder_and_AP_to_speakers();	
					break;
			}
			break;
		case HEADSET_RINGTONE:
			AP_to_speakers_and_headset();
			break;
		case BLUETOOTH_A2DP_NORMAL:					//AP-> 8994Codec -> BLUETOOTH_A2DP
		case BLUETOOTH_A2DP_INCALL:
		case BLUETOOTH_SCO_NORMAL:
			printk("this route not use\n");
			break;		 			
		default:
			printk("wm8994 error route!!!\n");
			goto out;
	}

	if(wm8994->RW_status == ERROR)
	{//Failure to read or write, will re-power on wm8994
		cancel_delayed_work_sync(&wm8994->wm8994_delayed_work);
		wm8994->work_type = SNDRV_PCM_TRIGGER_PAUSE_PUSH;
		schedule_delayed_work(&wm8994->wm8994_delayed_work, msecs_to_jiffies(10));
		goto out;
	}
	//after set the route -- enable PA
	switch(route)
	{
		case EARPIECE_INCALL:
		case HEADSET_NORMAL:
		case HEADSET_INCALL:
		case BLUETOOTH_A2DP_NORMAL:	
		case BLUETOOTH_A2DP_INCALL:
		case BLUETOOTH_SCO_NORMAL:		
			break;
		default:
			msleep(50);
			PA_ctrl(GPIO_HIGH);		
			break;
	}	
out:	
	mutex_unlock(&wm8994->route_lock);	
	return 0;
}

/*
 * WM8994 Controls
 */
static const struct snd_kcontrol_new wm8994_snd_controls[] = {
SOC_DOUBLE_SWITCH_WM8994CODEC("Speaker incall Switch", SPEAKER_INCALL),	
SOC_DOUBLE_SWITCH_WM8994CODEC("Speaker normal Switch", SPEAKER_NORMAL),

SOC_DOUBLE_SWITCH_WM8994CODEC("Earpiece incall Switch", EARPIECE_INCALL),	
SOC_DOUBLE_SWITCH_WM8994CODEC("Earpiece normal Switch", EARPIECE_NORMAL),

SOC_DOUBLE_SWITCH_WM8994CODEC("Headset incall Switch", HEADSET_INCALL),	
SOC_DOUBLE_SWITCH_WM8994CODEC("Headset normal Switch", HEADSET_NORMAL),

SOC_DOUBLE_SWITCH_WM8994CODEC("Bluetooth incall Switch", BLUETOOTH_SCO_INCALL),	
SOC_DOUBLE_SWITCH_WM8994CODEC("Bluetooth normal Switch", BLUETOOTH_SCO_NORMAL),

SOC_DOUBLE_SWITCH_WM8994CODEC("Bluetooth-A2DP incall Switch", BLUETOOTH_A2DP_INCALL),	
SOC_DOUBLE_SWITCH_WM8994CODEC("Bluetooth-A2DP normal Switch", BLUETOOTH_A2DP_NORMAL),

SOC_DOUBLE_SWITCH_WM8994CODEC("Capture Switch", MIC_CAPTURE),

SOC_DOUBLE_SWITCH_WM8994CODEC("Earpiece ringtone Switch",EARPIECE_RINGTONE),
SOC_DOUBLE_SWITCH_WM8994CODEC("Speaker ringtone Switch",SPEAKER_RINGTONE),
SOC_DOUBLE_SWITCH_WM8994CODEC("Headset ringtone Switch",HEADSET_RINGTONE),
};

static void wm8994_codec_set_volume(unsigned char system_type,unsigned char volume)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
//	DBG("%s:: system_type = %d volume = %d \n",__FUNCTION__,system_type,volume);

	if(system_type == VOICE_CALL)
	{
		if(volume <= call_maxvol)
			wm8994->call_vol=volume;
		else
		{
			printk("%s----%d::max value is 5\n",__FUNCTION__,__LINE__);
			wm8994->call_vol=call_maxvol;
		}
		if(wm8994_current_mode<=wm8994_mainMIC_to_baseband_to_speakers_and_record&&
			wm8994_current_mode>=wm8994_handsetMIC_to_baseband_to_headset)
			wm8994_set_volume(wm8994_current_mode,wm8994->call_vol,call_maxvol);
	}
	else if(system_type == BLUETOOTH_SCO)
	{
		if(volume <= BT_call_maxvol)
			wm8994->BT_call_vol = volume;
		else
		{
			printk("%s----%d::max value is 15\n",__FUNCTION__,__LINE__);
			wm8994->BT_call_vol = BT_call_maxvol;
		}
		if(wm8994_current_mode<null&&
			wm8994_current_mode>=wm8994_BT_baseband)
			wm8994_set_volume(wm8994_current_mode,wm8994->BT_call_vol,BT_call_maxvol);
	}
	else
	{
		printk("%s----%d::system type error!\n",__FUNCTION__,__LINE__);
		return;
	}
}

/*
 * Note that this should be called from init rather than from hw_params.
 */
static int wm8994_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct wm8994_priv *wm8994 = codec->private_data;
	
//	DBG("%s----%d\n",__FUNCTION__,__LINE__);
	switch (freq) {

	default:
		wm8994->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int wm8994_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	return 0;
}

static int wm8994_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	return 0;
}

static int wm8994_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int wm8994_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	return 0;
}

static int wm8994_trigger(struct snd_pcm_substream *substream,
			  int cmd,
			  struct snd_soc_dai *dai)
{
//	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct snd_soc_dai_link *machine = rtd->dai;
//	struct snd_soc_dai *codec_dai = machine->codec_dai;
	struct snd_soc_codec *codec = dai->codec;
	struct wm8994_priv *wm8994 = codec->private_data;
	
	if(wm8994_current_mode >= wm8994_handsetMIC_to_baseband_to_headset && wm8994_current_mode != null)
		return 0;
//	DBG("%s::%d status = %d substream->stream '%s'\n",__FUNCTION__,__LINE__,
//	    cmd, substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "PLAYBACK":"CAPTURE");
	
	switch(cmd)
	{
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_START:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			{
				if(wm8994->playback_active == cmd)
					return 0;
				wm8994->playback_active = cmd;
			}	
			else
			{
				if(wm8994->capture_active == cmd)
					return 0;
				wm8994->capture_active = cmd;	
			}	
			break;
		default:
			return 0;
	}

	if (!wm8994->playback_active && 
		!wm8994->capture_active )
	{//suspend
		DBG("It's going to power down wm8994\n");
		cancel_delayed_work_sync(&wm8994->wm8994_delayed_work);		
		wm8994->work_type = SNDRV_PCM_TRIGGER_STOP;
		schedule_delayed_work(&wm8994->wm8994_delayed_work, msecs_to_jiffies(2000));
	} 
	else if (wm8994->playback_active 
			|| wm8994->capture_active) 
	{//resume
		DBG("Power up wm8994\n");	
		if(wm8994->work_type == SNDRV_PCM_TRIGGER_STOP)
			cancel_delayed_work_sync(&wm8994->wm8994_delayed_work);
		wm8994->work_type = SNDRV_PCM_TRIGGER_START;
		schedule_delayed_work(&wm8994->wm8994_delayed_work, msecs_to_jiffies(0));		
	}

	return 0;
}

static void wm8994_work_fun(struct work_struct *work)
{	
	struct snd_soc_codec *codec = wm8994_codec;
	struct wm8994_priv *wm8994 = codec->private_data;
	struct wm8994_pdata *pdata = wm8994->pdata;
	int error_count = 5;
//	DBG("Enter %s---%d = %d\n",__FUNCTION__,__LINE__,wm8994_current_mode);

	switch(wm8994->work_type)
	{
	case SNDRV_PCM_TRIGGER_STOP:
		if(wm8994_current_mode > wm8994_FM_to_speakers_and_record)
			return;	
	//	DBG("wm8994 shutdown\n");
		mutex_lock(&wm8994->route_lock);
		PA_ctrl(GPIO_LOW);
		msleep(50);
		wm8994_write(0,0);
		msleep(50);
		wm8994_write(0x01, 0x0023);	
		wm8994_current_mode = null;//Automatically re-set the wake-up time	
		mutex_unlock(&wm8994->route_lock);	
		break;
	case SNDRV_PCM_TRIGGER_START:
		if(wm8994->First_Poweron == 1)
		{
			DBG("wm8994 First_Poweron shutup\n");
			wm8994->First_Poweron = 0;
			if(wm8994->kcontrol->private_value != SPEAKER_NORMAL)
			{
			//	DBG("wm8994->kcontrol->private_value != SPEAKER_NORMAL\n");
				return;
			}
			wm8994_current_mode = null;
			snd_soc_put_route(wm8994->kcontrol,NULL);
		}		
		break;
	case SNDRV_PCM_TRIGGER_RESUME:	
		msleep(100);
		gpio_request(pdata->Power_EN_Pin, NULL);
		gpio_direction_output(pdata->Power_EN_Pin,GPIO_HIGH);
		gpio_free(pdata->Power_EN_Pin);		
		msleep(100);
		wm8994_current_mode = null;
		snd_soc_put_route(wm8994->kcontrol,NULL);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:	
		while(error_count)
		{
			if( wm8994_reset_ldo() ==  0)
			{
				wm8994_current_mode = null;
				snd_soc_put_route(wm8994->kcontrol,NULL);
				break;
			}
			error_count --;
		}
		if(error_count == 0)
		{
			PA_ctrl(GPIO_LOW);
			printk("wm8994 Major problems, give me log,tks, -- qjb\n");
		}	
		break;

	default:
		break;
	}

}

#define WM8994_RATES SNDRV_PCM_RATE_8000_48000
#define WM8994_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops wm8994_ops = {
	.hw_params = wm8994_pcm_hw_params,
	.set_fmt = wm8994_set_dai_fmt,
	.set_sysclk = wm8994_set_dai_sysclk,
	.digital_mute = wm8994_mute,
	.trigger	= wm8994_trigger,
	/*add by qiuen for volume*/
	.set_volume = wm8994_codec_set_volume,
};

struct snd_soc_dai wm8994_dai = {
	.name = "WM8994",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8994_RATES,
		.formats = WM8994_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = WM8994_RATES,
		.formats = WM8994_FORMATS,
	 },
	.ops = &wm8994_ops,
	.symmetric_rates = 1,
};
EXPORT_SYMBOL_GPL(wm8994_dai);

static int wm8994_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	struct wm8994_priv *wm8994 = codec->private_data;
	struct wm8994_pdata *pdata = wm8994->pdata;
	
	DBG("%s----%d\n",__FUNCTION__,__LINE__);

	cancel_delayed_work_sync(&wm8994->wm8994_delayed_work);	
	PA_ctrl(GPIO_LOW);
	wm8994_write(0x00, 0x00);
	
	gpio_request(pdata->Power_EN_Pin, NULL);
	gpio_direction_output(pdata->Power_EN_Pin,GPIO_LOW);
	gpio_free(pdata->Power_EN_Pin);	
	msleep(50);

	return 0;
}

static int wm8994_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	struct wm8994_priv *wm8994 = codec->private_data;
//	struct wm8994_pdata *pdata = wm8994->pdata;
	
	DBG("%s----%d\n",__FUNCTION__,__LINE__);

	cancel_delayed_work_sync(&wm8994->wm8994_delayed_work);	
	wm8994->work_type = SNDRV_PCM_TRIGGER_RESUME;
	schedule_delayed_work(&wm8994->wm8994_delayed_work, msecs_to_jiffies(0));	

	return 0;
}

#ifdef WM8994_PROC
static ssize_t wm8994_proc_write(struct file *file, const char __user *buffer,
			   unsigned long len, void *data)
{
	char *cookie_pot; 
	char *p;
	int reg;
	int value;
	struct snd_kcontrol kcontrol;
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	struct wm8994_pdata *pdata = wm8994->pdata;
	
	cookie_pot = (char *)vmalloc( len );
	if (!cookie_pot) 
	{
		return -ENOMEM;
	} 
	else 
	{
		if (copy_from_user( cookie_pot, buffer, len )) 
			return -EFAULT;
	}
	
	switch(cookie_pot[0])
	{
	case 'd':
	case 'D':
		debug_write_read ++;
		debug_write_read %= 2;
		if(debug_write_read != 0)
			DBG("Debug read and write reg on\n");
		else	
			DBG("Debug read and write reg off\n");	
		break;	
	case 'r':
	case 'R':
		DBG("Read reg debug\n");		
		if(cookie_pot[1] ==':')
		{
			debug_write_read = 1;
			strsep(&cookie_pot,":");
			while((p=strsep(&cookie_pot,",")))
			{
				wm8994_read(simple_strtol(p,NULL,16),(unsigned short *)&value);
			}
			debug_write_read = 0;;
			DBG("\n");		
		}
		else
		{
			DBG("Error Read reg debug.\n");
			DBG("For example: echo 'r:22,23,24,25'>wm8994_ts\n");
		}
		break;
	case 'w':
	case 'W':
		DBG("Write reg debug\n");		
		if(cookie_pot[1] ==':')
		{
			debug_write_read = 1;
			strsep(&cookie_pot,":");
			while((p=strsep(&cookie_pot,"=")))
			{
				reg = simple_strtol(p,NULL,16);
				p=strsep(&cookie_pot,",");
				value = simple_strtol(p,NULL,16);
				wm8994_write(reg,value);
			}
			debug_write_read = 0;;
			DBG("\n");
		}
		else
		{
			DBG("Error Write reg debug.\n");
			DBG("For example: w:22=0,23=0,24=0,25=0\n");
		}
		break;	
	case 'p':
	case 'P':
		if(cookie_pot[1] =='-')
		{
			kcontrol.private_value = simple_strtol(&cookie_pot[2],NULL,10);
			printk("kcontrol.private_value = %ld\n",kcontrol.private_value);
			if(kcontrol.private_value<SPEAKER_INCALL || kcontrol.private_value>HEADSET_RINGTONE)
			{
				printk("route error\n");
				goto help;
			}	
			snd_soc_put_route(&kcontrol,NULL);
			break;
		}	
		else
		{
			goto help;
		}
		help:
			printk("snd_soc_put_route list\n");
			printk("SPEAKER_INCALL--\"p-0\",\nSPEAKER_NORMAL--\"p-1\",\nHEADSET_INCALL--\"p-2\",\
			\nHEADSET_NORMAL--\"p-3\",\nEARPIECE_INCALL--\"p-4\",\nEARPIECE_NORMAL--\"p-5\",\
			\nBLUETOOTH_SCO_INCALL--\"p-6\",\nMIC_CAPTURE--\"p-10\",\nEARPIECE_RINGTONE--\"p-11\",\
			\nSPEAKER_RINGTONE--\"p-12\",\nHEADSET_RINGTONE--\"p-13\"\n");			
		break;
	case 'F':
	case 'f':
		PA_ctrl(GPIO_HIGH);		
		FM_to_speakers();
		break;
	case 'S':
	case 's':
		printk("Debug : Set volume begin\n");
		switch(cookie_pot[1])
		{
			case '+':
				if(cookie_pot[2] == '\n')
				{
				
				}
				else
				{
					value = simple_strtol(&cookie_pot[2],NULL,10);
					printk("value = %d\n",value);

				}
				break;
			case '-':
				if(cookie_pot[2] == '\n')
				{
					
				}
				else
				{
					value = simple_strtol(&cookie_pot[2],NULL,10);
					printk("value = %d\n",value);
				}
				break;
			default:
				if(cookie_pot[1] == '=')
				{
					value = simple_strtol(&cookie_pot[2],NULL,10);
					printk("value = %d\n",value);
				}	
				else
					printk("Help the set volume,Example: echo s+**>wm8994_ts,s=**>wm8994_ts,s-**>wm8994_ts\n");

				break;				
		}		
		break;	
	case '1':
		gpio_request(pdata->Power_EN_Pin, NULL);
		gpio_direction_output(pdata->Power_EN_Pin,GPIO_LOW);
		gpio_free(pdata->Power_EN_Pin);	
		break;
	case '2':	
		gpio_request(pdata->Power_EN_Pin, NULL);
		gpio_direction_output(pdata->Power_EN_Pin,GPIO_HIGH);
		gpio_free(pdata->Power_EN_Pin);			
		break;
	default:
		DBG("Help for wm8994_ts .\n-->The Cmd list: \n");
		DBG("-->'d&&D' Open or close the debug\n");
		DBG("-->'r&&R' Read reg debug,Example: echo 'r:22,23,24,25'>wm8994_ts\n");
		DBG("-->'w&&W' Write reg debug,Example: echo 'w:22=0,23=0,24=0,25=0'>wm8994_ts\n");
		DBG("-->'ph&&Ph' cat snd_soc_put_route list\n");
		break;
	}

	return len;
}

static const struct file_operations wm8994_proc_fops = {
	.owner		= THIS_MODULE,
	//.open		= snd_mem_proc_open,
	//.read		= seq_read,
//#ifdef CONFIG_PCI
	.write		= wm8994_proc_write,
//#endif
	//.llseek	= seq_lseek,
	//.release	= single_release,
};

static int wm8994_proc_init(void){

	struct proc_dir_entry *wm8994_proc_entry;

	wm8994_proc_entry = create_proc_entry("driver/wm8994_ts", 0777, NULL);

	if(wm8994_proc_entry != NULL){

		wm8994_proc_entry->write_proc = wm8994_proc_write;

		return -1;
	}else{
		printk("create proc error !\n");
	}

	return 0;
}

#endif

static int wm8994_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	struct wm8994_priv *wm8994;
	struct wm8994_pdata *pdata;
	int ret = 0;
	

#ifdef WM8994_PROC
	wm8994_proc_init();
#endif

	if (wm8994_codec == NULL) {
		dev_err(&pdev->dev, "Codec device not registered\n");
		return -ENODEV;
	}

	socdev->card->codec = wm8994_codec;
	codec = wm8994_codec;
	wm8994 = codec->private_data;
	pdata = wm8994->pdata;
	//disable power_EN
	gpio_request(pdata->Power_EN_Pin, NULL);			 
	gpio_direction_output(pdata->Power_EN_Pin,GPIO_LOW); 		
	gpio_free(pdata->Power_EN_Pin);	
	
	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(codec->dev, "failed to create pcms: %d\n", ret);
		goto pcm_err;
	}

	snd_soc_add_controls(codec,wm8994_snd_controls,
				ARRAY_SIZE(wm8994_snd_controls));

	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		dev_err(codec->dev, "failed to register card: %d\n", ret);
		goto card_err;
	}

	PA_ctrl(GPIO_LOW);
	//enable power_EN
	msleep(50);
	gpio_request(pdata->Power_EN_Pin, NULL);			 
	gpio_direction_output(pdata->Power_EN_Pin,GPIO_HIGH); 		
	gpio_free(pdata->Power_EN_Pin);	

	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
	return ret;
}

static int wm8994_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm8994 = {
	.probe = 	wm8994_probe,
	.remove = 	wm8994_remove,
	.suspend = 	wm8994_suspend,
	.resume =	wm8994_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_wm8994);

static int wm8994_register(struct wm8994_priv *wm8994,
			   enum snd_soc_control_type control)
{
	struct snd_soc_codec *codec = &wm8994->codec;
	int ret;

	if (wm8994_codec) {
		dev_err(codec->dev, "Another WM8994 is registered\n");
		ret = -EINVAL;
		goto err;
	}

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->private_data = wm8994;
	codec->name = "WM8994";
	codec->owner = THIS_MODULE;
	codec->dai = &wm8994_dai;
	codec->num_dai = 1;
//	codec->reg_cache_size = ARRAY_SIZE(wm8994->reg_cache);
//	codec->reg_cache = &wm8994->reg_cache;
	codec->bias_level = SND_SOC_BIAS_OFF;
	codec->set_bias_level = wm8994_set_bias_level;

//	memcpy(codec->reg_cache, wm8994_reg,
//	       sizeof(wm8994_reg));

	ret = snd_soc_codec_set_cache_io(codec,7, 9, control);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		goto err;
	}

	ret = 0;
	if (ret < 0) {
		dev_err(codec->dev, "Failed to issue reset\n");
		goto err;
	}

	wm8994_set_bias_level(&wm8994->codec, SND_SOC_BIAS_STANDBY);

	wm8994_dai.dev = codec->dev;

	wm8994_codec = codec;

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register codec: %d\n", ret);
		goto err;
	}

	ret = snd_soc_register_dai(&wm8994_dai);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register DAI: %d\n", ret);
		snd_soc_unregister_codec(codec);
		goto err_codec;
	}
	return 0;

err_codec:
	snd_soc_unregister_codec(codec);
err:
	kfree(wm8994);
	return ret;
}

static void wm8994_unregister(struct wm8994_priv *wm8994)
{
	wm8994_set_bias_level(&wm8994->codec, SND_SOC_BIAS_OFF);
	snd_soc_unregister_dai(&wm8994_dai);
	snd_soc_unregister_codec(&wm8994->codec);
	kfree(wm8994);
	wm8994_codec = NULL;
}

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static int wm8994_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct wm8994_priv *wm8994;
	struct snd_soc_codec *codec;

	wm8994 = kzalloc(sizeof(struct wm8994_priv), GFP_KERNEL);
	if (wm8994 == NULL)
		return -ENOMEM;

	codec = &wm8994->codec;

	i2c_set_clientdata(i2c, wm8994);
	codec->control_data = i2c;

	codec->dev = &i2c->dev;
	wm8994->pdata = i2c->dev.platform_data;//add
	wm8994->RW_status = TRUE;//add
	wm8994->capture_active = 0;
	wm8994->playback_active = 0;
	wm8994->First_Poweron = 1;
	wm8994->call_vol = call_maxvol;
	wm8994->BT_call_vol = BT_call_maxvol;
	INIT_DELAYED_WORK(&wm8994->wm8994_delayed_work, wm8994_work_fun);
	mutex_init(&wm8994->io_lock);	
	mutex_init(&wm8994->route_lock);
	return wm8994_register(wm8994, SND_SOC_I2C);
}

static int wm8994_i2c_remove(struct i2c_client *client)
{
	struct wm8994_priv *wm8994 = i2c_get_clientdata(client);
	wm8994_unregister(wm8994);
	return 0;
}

#ifdef CONFIG_PM
static int wm8994_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	return snd_soc_suspend_device(&client->dev);
}

static int wm8994_i2c_resume(struct i2c_client *client)
{
	return snd_soc_resume_device(&client->dev);
}
#else
#define wm8994_i2c_suspend NULL
#define wm8994_i2c_resume NULL
#endif

static void wm8994_i2c_shutdown(struct i2c_client *client)
{
	struct wm8994_priv *wm8994 = wm8994_codec->private_data;
	struct wm8994_pdata *pdata = wm8994->pdata;
	DBG("%s----%d\n",__FUNCTION__,__LINE__);
	//disable PA
	PA_ctrl(GPIO_LOW);	
	gpio_request(pdata->Power_EN_Pin, NULL);
	gpio_direction_output(pdata->Power_EN_Pin,GPIO_LOW);
	gpio_free(pdata->Power_EN_Pin);	
}

static const struct i2c_device_id wm8994_i2c_id[] = {
	{ "wm8994", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8994_i2c_id);

static struct i2c_driver wm8994_i2c_driver = {
	.driver = {
		.name = "WM8994",
		.owner = THIS_MODULE,
	},
	.probe = wm8994_i2c_probe,
	.remove = wm8994_i2c_remove,
	.suspend = wm8994_i2c_suspend,
	.resume = wm8994_i2c_resume,
	.id_table = wm8994_i2c_id,
	.shutdown = wm8994_i2c_shutdown,
};

#endif

#if defined(CONFIG_SPI_MASTER)
static int __devinit wm8994_spi_probe(struct spi_device *spi)
{
	struct wm8994_priv *wm8994;
	struct snd_soc_codec *codec;

	wm8994 = kzalloc(sizeof(struct wm8994_priv), GFP_KERNEL);
	if (wm8994 == NULL)
		return -ENOMEM;

	codec = &wm8994->codec;
	codec->control_data = spi;
	codec->dev = &spi->dev;

	dev_set_drvdata(&spi->dev, wm8994);

	return wm8994_register(wm8994, SND_SOC_SPI);
}

static int __devexit wm8994_spi_remove(struct spi_device *spi)
{
	struct wm8994_priv *wm8994 = dev_get_drvdata(&spi->dev);

	wm8994_unregister(wm8994);

	return 0;
}

#ifdef CONFIG_PM
static int wm8994_spi_suspend(struct spi_device *spi, pm_message_t msg)
{
	return snd_soc_suspend_device(&spi->dev);
}

static int wm8994_spi_resume(struct spi_device *spi)
{
	return snd_soc_resume_device(&spi->dev);
}
#else
#define wm8994_spi_suspend NULL
#define wm8994_spi_resume NULL
#endif

static struct spi_driver wm8994_spi_driver = {
	.driver = {
		.name	= "wm8994",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= wm8994_spi_probe,
	.remove		= __devexit_p(wm8994_spi_remove),
	.suspend	= wm8994_spi_suspend,
	.resume		= wm8994_spi_resume,
};
#endif

static int __init wm8994_modinit(void)
{
	int ret;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&wm8994_i2c_driver);
	if (ret != 0)
		pr_err("WM8994: Unable to register I2C driver: %d\n", ret);
#endif
#if defined(CONFIG_SPI_MASTER)
	ret = spi_register_driver(&wm8994_spi_driver);
	if (ret != 0)
		pr_err("WM8994: Unable to register SPI driver: %d\n", ret);
#endif
	return ret;
}
module_init(wm8994_modinit);

static void __exit wm8994_exit(void)
{
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&wm8994_i2c_driver);
#endif
#if defined(CONFIG_SPI_MASTER)
	spi_unregister_driver(&wm8994_spi_driver);
#endif
}
module_exit(wm8994_exit);


MODULE_DESCRIPTION("ASoC WM8994 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
