/* Ogle - A video player
 * Copyright (C) 2002 Björn Englund
 * Copyright (C) 2002 Torgeir Veimo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef LIBOGLEAO_ALSA

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/asoundlib.h>

#include "ogle_ao.h"
#include "ogle_ao_private.h"

typedef struct alsa_instance_s {
	ogle_ao_instance_t ao;
	int sample_rate;
	int samples_written;
	int sample_frame_size;
	int channels;
	int initialized;
	snd_pcm_t *alsa_pcm;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	snd_pcm_format_t format;
	snd_pcm_sframes_t buffer_size;
	snd_pcm_sframes_t period_size;
} alsa_instance_t;

// logging
static snd_output_t *logs = NULL;

/* ring buffer length in us */
#define BUFFER_TIME 500000
/* period time in us */ 
#define PERIOD_TIME 10000
/* minimum samples before play */
#define MINIMUM_SAMPLES 5000

static int set_hwparams(alsa_instance_t *instance)
{
	int err, dir;

	if ((err = snd_pcm_hw_params_any(instance->alsa_pcm, instance->hwparams)) < 0) {
		fprintf(stderr, "Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
		return err;
	}
	/* set the interleaved read/write format */
	if ((err = snd_pcm_hw_params_set_access(instance->alsa_pcm, instance->hwparams, 
		SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "Access type not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the sample format */
	if ((err = snd_pcm_hw_params_set_format(instance->alsa_pcm, instance->hwparams, instance->format)) < 0) {
		fprintf(stderr, "Sample format not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the count of channels */
	if ((err = snd_pcm_hw_params_set_channels(instance->alsa_pcm, instance->hwparams, instance->channels)) > 0) {
		fprintf(stderr, "Channels count (%i) not available for playbacks: %s\n", instance->channels, snd_strerror(err));
		return err;
	}
	/* set the stream rate */
	if ((err = snd_pcm_hw_params_set_rate_near(instance->alsa_pcm, instance->hwparams, instance->sample_rate, 0)) < 0) {
		fprintf(stderr, "Rate %iHz not available for playback: %s\n", instance->sample_rate, snd_strerror(err));
		return err;
	}
	if (err != instance->sample_rate) {
		fprintf(stderr, "Rate doesn't match (requested %iHz, get %iHz)\n", instance->sample_rate, err);
		return -EINVAL;
	}
	/* set buffer time */
	if ((err = snd_pcm_hw_params_set_buffer_time_near(instance->alsa_pcm, instance->hwparams, BUFFER_TIME, &dir)) < 0) {
		fprintf(stderr, "Unable to set buffer time %i for playback: %s\n", BUFFER_TIME, snd_strerror(err));
		return err;
	}
	instance->buffer_size = snd_pcm_hw_params_get_buffer_size(instance->hwparams);
	/* set period time */
	if ((err = snd_pcm_hw_params_set_period_time_near(instance->alsa_pcm, instance->hwparams, PERIOD_TIME, &dir)) < 0) {
		fprintf(stderr, "Unable to set period time %i for playback: %s\n", PERIOD_TIME, snd_strerror(err));
		return err;
	}
	instance->period_size = snd_pcm_hw_params_get_period_size(instance->hwparams, &dir);
	/* write the parameters to device */
	if ((err = snd_pcm_hw_params(instance->alsa_pcm, instance->hwparams)) < 0) {
		fprintf(stderr, "Unable to set hw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

static int set_swparams(alsa_instance_t *instance)
{
	int err;

	/* get current swparams */
	if ((err = snd_pcm_sw_params_current(instance->alsa_pcm, instance->swparams)) < 0) {
		fprintf(stderr, "Unable to determine current swparams for playback: %s\n", snd_strerror(err));
	}
	/* start transfer when the buffer is full */
/*	if ((err = snd_pcm_sw_params_set_start_threshold(instance->alsa_pcm, instance->swparams, MINIMUM_SAMPLES)) < 0) {
		fprintf(stderr, "Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
	}*/	
	/* allow transfer when at least period_size samples can be processed */
	if ((err = snd_pcm_sw_params_set_avail_min(instance->alsa_pcm, instance->swparams, MINIMUM_SAMPLES)) < 0) {
		fprintf(stderr, "Unable to set avail min for playback: %s\n", snd_strerror(err));
	}
	/* align all transfers to 1 samples */
	if ((err = snd_pcm_sw_params_set_xfer_align(instance->alsa_pcm, instance->swparams, 1)) < 0) {
		fprintf(stderr, "Unable to set transfer align for playback: %s\n", snd_strerror(err));
	}
	/* set swparams */	
	if ((err = snd_pcm_sw_params(instance->alsa_pcm, instance->swparams)) < 0) {	
		fprintf(stderr, "Unable to set sw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}



/*
 * Set the device to IEC60958 (spdif) in IEC61937 mode.
 */
static int ctl_set_iec60958(alsa_instance_t *instance)
{
  snd_pcm_t *handle = instance->alsa_pcm;
  snd_pcm_info_t *info;
  static snd_aes_iec958_t spdif;
  int err;
  
  snd_pcm_info_alloca(&info);
  
  if ((err = snd_pcm_info(handle, info)) < 0) {
    fprintf(stderr, "info: %s\n", snd_strerror(err));
    return err;
  }
  printf("device: %d, subdevice: %d\n", 
	 snd_pcm_info_get_device(info),
	 snd_pcm_info_get_subdevice(info));                              
  {
    snd_ctl_elem_value_t *ctl;
    snd_ctl_t *ctl_handle;
    char ctl_name[12];
    int ctl_card;

    spdif.status[0] = IEC958_AES0_NONAUDIO | IEC958_AES0_CON_EMPHASIS_NONE;
    spdif.status[1] = IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER;
    spdif.status[2] = 0;
    spdif.status[3] = IEC958_AES3_CON_FS_48000;

    snd_ctl_elem_value_alloca(&ctl);
    snd_ctl_elem_value_set_interface(ctl, SND_CTL_ELEM_IFACE_PCM);
    snd_ctl_elem_value_set_device(ctl, snd_pcm_info_get_device(info));
    snd_ctl_elem_value_set_subdevice(ctl, snd_pcm_info_get_subdevice(info));
    snd_ctl_elem_value_set_name(ctl, 
				SND_CTL_NAME_IEC958("", PLAYBACK, PCM_STREAM));
    snd_ctl_elem_value_set_iec958(ctl, &spdif);
    
    ctl_card = snd_pcm_info_get_card(info);
    if (ctl_card < 0) {
      fprintf(stderr, "Unable to setup the IEC958 (S/PDIF) interface - PCM has no assigned card");
      goto __diga_end;
    }
    
    if ((err = snd_ctl_open(&ctl_handle, ctl_name, 0)) < 0) {
      fprintf(stderr, "Unable to open the control interface '%s': %s", ctl_name, snd_strer
	      ror(err));
      goto __diga_end;
    }
    
    if ((err = snd_ctl_elem_write(ctl_handle, ctl)) < 0) {
      fprintf(stderr, "Unable to update the IEC958 control: %s", snd_strerror(err));
      goto __diga_end;
      //Shouldn't ctl_handle be closed here?
    }
    
    snd_ctl_close(ctl_handle);
  
  __diga_end:
  }
  
  return 0;
}




static
int alsa_init(ogle_ao_instance_t *_instance,
	      ogle_ao_audio_info_t *audio_info)
{
	alsa_instance_t *instance = (alsa_instance_t *)_instance;
	int single_sample_size;
	int err;
	
    snd_pcm_hw_params_alloca(&(instance->hwparams));
    snd_pcm_sw_params_alloca(&(instance->swparams));
	
  	if(instance->initialized) {
		return -1;  // for now we must close and open the device for reinit
	}
	instance->sample_rate = audio_info->sample_rate;
	
	switch(audio_info->encoding) {
	    case OGLE_AO_ENCODING_LINEAR:
		switch(audio_info->sample_resolution) {
		   case 16:
		       if(audio_info->byteorder == OGLE_AO_BYTEORDER_BE) {
			   instance->format = SND_PCM_FORMAT_S16_BE;
		       } else {
			   instance->format = SND_PCM_FORMAT_S16_LE;
		       }
		       /* ok */
		       break;
		default:
		    /* not supported */
		    return -1;
		    break;
		}
		break;
	    case OGLE_AO_ENCODING_IEC61937:
	        // Setup the IEC60958 (S/PDIF) interface for NONAUDIO
	        ctl_set_iec60958(instance);
		instance->format = SND_PCM_FORMAT_S16_LE;
	    default:
		/* not supported */
		return -1;
		break;
	}
  
  	/* Check that we actually got the requested nuber of channels,
	 * the frequency and precision that we asked for?  */
  
  
	if(audio_info->chtypes != OGLE_AO_CHTYPE_UNSPECIFIED) {
	  // do this only if we have requested specific channels in chtypes
	  // otherwise trust the nr channels
	  
	  audio_info->chtypes = OGLE_AO_CHTYPE_LEFT | OGLE_AO_CHTYPE_RIGHT;
	  audio_info->channels = 2;
	}
	if(audio_info->chlist) {
	  free(audio_info->chlist);
	}
	audio_info->chlist = NULL;
	
	if(audio_info->chtypes != OGLE_AO_CHTYPE_UNSPECIFIED) {
	  audio_info->chlist = malloc(audio_info->channels * sizeof(ogle_ao_chtype_t));
	  
	  audio_info->chlist[0] = OGLE_AO_CHTYPE_LEFT;
	  audio_info->chlist[1] = OGLE_AO_CHTYPE_RIGHT;
	}

	instance->channels = audio_info->channels;

  	/* Stored as 8bit, 16bit, or 32bit */
	single_sample_size = (audio_info->sample_resolution + 7) / 8;
	if(single_sample_size > 2) {
		single_sample_size = 4;
	}
	instance->sample_frame_size = single_sample_size*audio_info->channels;
	instance->initialized = 1;

	audio_info->sample_frame_size = instance->sample_frame_size;

	if ((err = set_hwparams(instance)) < 0) {
		fprintf(stderr, "Setting of hwparams failed: %s\n", snd_strerror(err));
		return -1;
	}
	if ((err = set_swparams(instance)) < 0) {
		fprintf(stderr, "Setting of swparams failed: %s\n", snd_strerror(err));
		return -1;
	}
	fprintf(stderr, "Stream parameters are %iHz, %s, %i channels, sample size: %d\n", 
		instance->sample_rate, snd_pcm_format_name(instance->format), 
		instance->channels, instance->sample_frame_size);
    
  
  return 0;
}

static int xrun_recovery(snd_pcm_t *handle, int err)
{
	fprintf(stderr, "xrun_recovery\n");
	if (err == -EPIPE) {	/* underrun */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			fprintf(stderr, "Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);	/* wait until suspend flag is released */
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				fprintf(stderr, "Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
		}
		return 0;
	}
	return err;
}

static
int alsa_play(ogle_ao_instance_t *_instance, void *samples, size_t nbytes)
{
	alsa_instance_t *instance = (alsa_instance_t *)_instance;
	long nsamples;
	long written;
	void *ptr;
  
	nsamples = nbytes / instance->sample_frame_size;
	ptr = samples;
	while (nsamples > 0) {
		written = snd_pcm_writei(instance->alsa_pcm, ptr, nsamples);
		if (written == -EAGAIN) {
			//printf("buffer overrun, retrying..\n");
			snd_pcm_wait(instance->alsa_pcm, 1000);
			continue;
		}
		if (written < 0) {
			//printf("buffer underrun, retrying..\n");
			if (xrun_recovery(instance->alsa_pcm, written) < 0) {
				fprintf(stderr, "Write error: %s\n", snd_strerror(written));
				exit(EXIT_FAILURE);
			}
			break;	/* skip one period */
		}
		ptr += written * instance->channels;
		nsamples -= written;
	}

  	instance->samples_written += nbytes / instance->sample_frame_size;
  
  	return 0;
}

static
int  alsa_odelay(ogle_ao_instance_t *_instance, uint32_t *samples_return)
{
	alsa_instance_t *instance = (alsa_instance_t *)_instance;
	snd_pcm_sframes_t avail;
	int err;
    
  	if ((err = snd_pcm_delay(instance->alsa_pcm, &avail)) < 0) {
		fprintf(stderr, "odelay error: %s\n", snd_strerror(err));
		avail = 0;
	}
	
	// If underrun, then we have 0 'delay'.
	if (avail < 0)
	  avail = 0;
	
  	*samples_return = (uint32_t)avail;
	//printf("remaining : %d\n", avail);
	
	return 0;
}

static
void alsa_close(ogle_ao_instance_t *_instance)
{
	alsa_instance_t *instance = (alsa_instance_t *)_instance;
 
  	snd_pcm_close(instance->alsa_pcm);
}

static
int alsa_flush(ogle_ao_instance_t *_instance)
{
	alsa_instance_t *instance = (alsa_instance_t *)_instance;
	int err;
	fprintf(stderr, "[ogle_alsa]: flushing...\n");
	
    if ((err = snd_pcm_reset(instance->alsa_pcm)) < 0) {
		fprintf(stderr, "drop failed: %s\n", snd_strerror(err));
    }
    //if ((err = snd_pcm_start(instance->alsa_pcm)) < 0) {
		//printf("drop failed: %s\n", snd_strerror(err));
    //}
	
  	return 0;
}

static
int alsa_drain(ogle_ao_instance_t *_instance)
{
	alsa_instance_t *instance = (alsa_instance_t *)_instance;
	int err;
	fprintf(stderr, "[ogle_alsa]: draining...\n");
	
    if ((err = snd_pcm_drop(instance->alsa_pcm)) < 0) {
		printf("drain failed: %s\n", snd_strerror(err));
    }
  
  	return 0;
}

static
ogle_ao_instance_t *alsa_open(char *device)
{
    alsa_instance_t *instance;
    int err = 0;
	
    instance = malloc(sizeof(alsa_instance_t));
    
    if(instance == NULL)
        return NULL;

    instance->ao.init   = alsa_init;
    instance->ao.play   = alsa_play;
    instance->ao.close  = alsa_close;
    instance->ao.odelay = alsa_odelay;
    instance->ao.flush  = alsa_flush;
    instance->ao.drain  = alsa_drain;

    instance->initialized = 0;
    instance->sample_rate = 0;
    instance->samples_written = 0;
    instance->sample_frame_size = 0;

    if ((err = snd_output_stdio_attach(&logs, stderr, 0)) < 0) {
		fprintf(stderr, "Output log failed: %s\n", snd_strerror(err));
		return NULL;
    }
 	
    fprintf(stderr, "[ogle_alsa]: Opening device: %s\n", device);
	
    if ((err = snd_pcm_open(&(instance->alsa_pcm), device, 
			SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
        perror("[ogle_alsa]: error while opening alsa.\n");
		return NULL;
    }    

    return (ogle_ao_instance_t *)instance;
}

/* The one directly exported function */
ogle_ao_instance_t *ao_alsa_open(char *dev)
{
  return alsa_open(dev);
}

#endif
