#include <linux/kernel.h>
#include <linux/delay.h>
#include "rk30_hdmi.h"
#include "rk30_hdmi_hw.h"


#define HDMI_MAX_TRY_TIMES	1

static char *envp[] = {"INTERFACE=HDMI", NULL};

int hdmi_sys_init(void)
{
	hdmi->pwr_mode			= PWR_SAVE_MODE_A;
	hdmi->hotplug			= HDMI_HPD_REMOVED;
	hdmi->state				= HDMI_SLEEP;
	hdmi->enable			= HDMI_ENABLE;
	
	hdmi->vic				= HDMI_VIDEO_DEFAULT_MODE;
	hdmi->audio.channel 	= HDMI_AUDIO_DEFAULT_CHANNEL;
	hdmi->audio.rate		= HDMI_AUDIO_DEFAULT_RATE;
	hdmi->audio.word_length	= HDMI_AUDIO_DEFAULT_WORD_LENGTH;
	
	return 0;
}

void hdmi_sys_remove(void)
{
	rk30_hdmi_removed();
	fb_destroy_modelist(&hdmi->edid.modelist);
	if(hdmi->edid.audio)
		kfree(hdmi->edid.audio);
	if(hdmi->edid.specs)
	{
		if(hdmi->edid.specs->modedb)
			kfree(hdmi->edid.specs->modedb);
		kfree(hdmi->edid.specs);
	}
	memset(&hdmi->edid, 0, sizeof(struct hdmi_edid));
	INIT_LIST_HEAD(&hdmi->edid.modelist);
	hdmi->state = HDMI_SLEEP;
	hdmi->hotplug = HDMI_HPD_REMOVED;
}

static int hdmi_process_command(void)
{
	int change, state = hdmi->state;
	
	change = hdmi->command;
	if(change != HDMI_CONFIG_NONE)	
	{		
		hdmi->command = HDMI_CONFIG_NONE;
		switch(change)
		{	
			case HDMI_CONFIG_ENABLE:
				/* disable HDMI */
				if(!hdmi->enable)
				{
					if(hdmi->hotplug)
						hdmi_sys_remove();
					state = HDMI_SLEEP;
				}
				if(hdmi->wait == 1) {
					complete(&hdmi->complete);
					hdmi->wait = 0;	
				}
				break;	
			case HDMI_CONFIG_COLOR:
				if(state > CONFIG_VIDEO)
					state = CONFIG_VIDEO;	
				break;
			case HDMI_CONFIG_HDCP:
				break;
			case HDMI_CONFIG_DISPLAY:
				break;
			case HDMI_CONFIG_AUDIO:
				if(state > CONFIG_AUDIO)
					state = CONFIG_AUDIO;
				break;
			case HDMI_CONFIG_VIDEO:
			default:
				if(state > SYSTEM_CONFIG)
					state = SYSTEM_CONFIG;
				else
				{
					if(hdmi->wait == 1) {
						complete(&hdmi->complete);
						hdmi->wait = 0;	
					}					
				}
				break;
		}
	}
	
	return state;
}

void hdmi_work(struct work_struct *work)
{
	int hotplug, state_last, state;
	int rc = HDMI_ERROR_SUCESS, trytimes = 0;
	/* Process hdmi command */
	state = hdmi_process_command();
	
	if(!hdmi->enable)
		return;
	
	hotplug = rk30_hdmi_detect_hotplug();
	hdmi_dbg(hdmi->dev, "[%s] hotplug %02x curvalue %d\n", __FUNCTION__, hotplug, hdmi->hotplug);
	if(hotplug != hdmi->hotplug)
	{
		hdmi->hotplug  = hotplug;
		if(hdmi->hotplug  == HDMI_HPD_INSERT)
			state = READ_PARSE_EDID;
		else {
			hdmi_sys_remove();
			kobject_uevent_env(&hdmi->dev->kobj, KOBJ_REMOVE, envp);
			return;
		}			
	}
	do {
		state_last = state;
		switch(state)
		{
			case READ_PARSE_EDID:
				rc = hdmi_sys_parse_edid(hdmi);
				if(rc == HDMI_ERROR_SUCESS)
				{
					state = SYSTEM_CONFIG;	
					kobject_uevent_env(&hdmi->dev->kobj, KOBJ_ADD, envp);
				}
				break;
			case SYSTEM_CONFIG:
				if(hdmi->autoconfig)	
					hdmi->vic = hdmi_find_best_mode(hdmi, 0);
				else
					hdmi->vic = hdmi_find_best_mode(hdmi, hdmi->vic);
				rc = hdmi_switch_fb(hdmi, hdmi->vic);
				if(rc == HDMI_ERROR_SUCESS)
					state = CONFIG_VIDEO;
				break;
			case CONFIG_VIDEO:					
				rc = rk30_hdmi_config_video(hdmi->vic, VIDEO_OUTPUT_RGB444, hdmi->edid.sink_hdmi);			
				if(rc == HDMI_ERROR_SUCESS)
				{
					if(hdmi->edid.sink_hdmi)
						state = CONFIG_AUDIO;
					else
						state = PLAY_BACK;
				}
				break;
			case CONFIG_AUDIO:
				rc = rk30_hdmi_config_audio(&(hdmi->audio));
							
				if(rc == HDMI_ERROR_SUCESS)
					state = PLAY_BACK;
				break;
			case PLAY_BACK:
				rk30_hdmi_control_output(1);
				if(hdmi->wait == 1) {	
					complete(&hdmi->complete);
					hdmi->wait = 0;						
				}
				break;
			default:
				break;
		}
		if(rc != HDMI_ERROR_SUCESS)
		{
			trytimes++;
			msleep(10);
		}
		if(state != state_last)
			trytimes = 0;
	}while((state != state_last || (rc != HDMI_ERROR_SUCESS) ) && trytimes < HDMI_MAX_TRY_TIMES);
	
	if(trytimes == HDMI_MAX_TRY_TIMES)
	{
		if(hdmi->hotplug)
			hdmi_sys_remove();
	}
}