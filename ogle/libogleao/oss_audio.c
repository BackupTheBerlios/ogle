/* Ogle - A video player
 * Copyright (C) 2002 Björn Englund
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

#ifdef LIBOGLEAO_OSS

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef	HAVE_STROPTS_H
#include <stropts.h>
#endif
#include <stdio.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif

#include "ogle_ao.h"
#include "ogle_ao_private.h"

typedef struct oss_instance_s {
  ogle_ao_instance_t ao;
  int fd;
  int sample_frame_size;
  int fragment_size;
  int nr_fragments;
  int fmt;
  int channels;
  int speed;
  int initialized;
} oss_instance_t;

static int log2(int val)
{
  int res = 0;

  while(val != 1) {
    val >>=1;
    ++res;
  }
  return res;
}

static
int oss_init(ogle_ao_instance_t *_instance,
		 ogle_ao_audio_info_t *audio_info)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  int single_sample_size;
  int number_of_channels, sample_format, original_sample_format, sample_speed;
  uint32_t fragment; 
  uint16_t nr_fragments;
  uint16_t fragment_size;
  audio_buf_info info;
  
  if(instance->initialized) {
    // SNDCTL_DSP_SYNC resets the audio device so we can set new parameters
    ioctl(instance->fd, SNDCTL_DSP_SYNC, 0);
    instance->initialized = 0;
  }


  // set fragment size if requested
  // can only be done once after open

  if(audio_info->fragment_size != -1) {
    if(log2(audio_info->fragment_size) > 0xffff) {
      fragment_size = 0xffff;
    } else {
      fragment_size = log2(audio_info->fragment_size);
    }
    if(audio_info->fragments != -1) {
      if(audio_info->fragments > 0xffff) {
	nr_fragments = 0xffff;
      } else {
	nr_fragments = audio_info->fragments;
      }
    } else {
      nr_fragments = 0x7fff;
    }
    
    fragment = (nr_fragments << 16) | fragment_size;
    
    if(ioctl(instance->fd, SNDCTL_DSP_SETFRAGMENT, &fragment) == -1) {
      perror("SNDCTL_DSP_SETFRAGMENT");
      //this is not fatal
    }
  }

  // Set sample format, number of channels, and sample speed
  // The order here is important according to the manual

  switch(audio_info->encoding) {
  case OGLE_AO_ENCODING_LINEAR:
    switch(audio_info->sample_resolution) {
      case 16:
	if(audio_info->byteorder == OGLE_AO_BYTEORDER_BE) {
	  sample_format = AFMT_S16_BE;
	} else {
	  sample_format = AFMT_S16_LE;
	}
        break;
      default:
	audio_info->sample_resolution = -1;
        return -1;
        break;
    }
    break;
  default:
    audio_info->encoding = -1;
    return -1;
    break;
  }
  original_sample_format = sample_format;
  if(ioctl(instance->fd, SNDCTL_DSP_SETFMT, &sample_format) == -1) {
    perror("SNDCTL_DSP_SETFMT");
    return -1;
  }
  
  instance->fmt = sample_format;
  
  // Test if we got the right format
  if (sample_format != original_sample_format) {
    switch(sample_format) {
    case AFMT_S16_BE:
      audio_info->encoding = OGLE_AO_ENCODING_LINEAR;
      audio_info->sample_resolution = 16;
      audio_info->byteorder = OGLE_AO_BYTEORDER_BE;
      break;
    case AFMT_S16_LE:
      audio_info->encoding = OGLE_AO_ENCODING_LINEAR;
      audio_info->sample_resolution = 16;
      audio_info->byteorder = OGLE_AO_BYTEORDER_LE;
      break;      
    default:
      audio_info->encoding = OGLE_AO_ENCODING_NONE;
      break;
    }
  }

  number_of_channels = audio_info->channels;
  if(ioctl(instance->fd, SNDCTL_DSP_CHANNELS, &number_of_channels) == -1) {
    perror("SNDCTL_DSP_CHANNELS");
    audio_info->channels = -1;
    return -1;
  }
  instance->channels = number_of_channels;

  // report back (maybe we can't do stereo or something...)
  audio_info->channels = number_of_channels;

  sample_speed = audio_info->sample_rate;
  if(ioctl(instance->fd, SNDCTL_DSP_SPEED, &sample_speed) == -1) {
    perror("SNDCTL_DSP_SPEED");
    audio_info->sample_rate = -1;
    return -1;
  }
  instance->speed = sample_speed;
  // report back the actual speed used
  audio_info->sample_rate = sample_speed;

  single_sample_size = (audio_info->sample_resolution + 7) / 8;
  if(single_sample_size > 2) {
    single_sample_size = 4;
  }
  instance->sample_frame_size = single_sample_size*number_of_channels;

  audio_info->sample_frame_size = instance->sample_frame_size;
  
  
  if(ioctl(instance->fd, SNDCTL_DSP_GETOSPACE, &info) == -1) {
    perror("SNDCTL_DSP_GETOSPACE");
    audio_info->fragment_size = -1;
    audio_info->fragments = -1;
    instance->fragment_size = -1;
    instance->nr_fragments = -1;
  } else {
    audio_info->fragment_size = info.fragsize;
    audio_info->fragments = info.fragstotal;
    instance->fragment_size = info.fragsize;
    instance->nr_fragments = info.fragstotal;
  }
  
  
  instance->initialized = 1;
  
  return 0;
}

/*
 * Only to be called after a SNDCTL_DSP_RESET or SNDCTL_DSP_SYNC
 * and after init
 */
static int oss_reinit(oss_instance_t *instance)
{
  int sample_format;
  int nr_channels;
  int sample_speed;
  audio_buf_info info;
  
  if(!instance->initialized) {
    return -1;
  }

  sample_format = instance->fmt;
  if(ioctl(instance->fd, SNDCTL_DSP_SETFMT, &sample_format) == -1) {
    perror("SNDCTL_DSP_SETFMT");
    return -1;
  }
  if(sample_format != instance->fmt) {
    fprintf(stderr, "oss_audio: couldn't reinit sample_format\n");
    return -1;
  }
  
  nr_channels = instance->channels;
  if(ioctl(instance->fd, SNDCTL_DSP_CHANNELS, &nr_channels) == -1) {
    perror("SNDCTL_DSP_CHANNELS");
    return -1;
  }
  if(nr_channels != instance->channels) {
    fprintf(stderr, "oss_audio: couldn't reinit nr_channels\n");
    return -1;
  }

  sample_speed = instance->speed;
  if(ioctl(instance->fd, SNDCTL_DSP_SPEED, &sample_speed) == -1) {
    perror("SNDCTL_DSP_SPEED");
    return -1;
  }
  if(sample_speed != instance->speed) {
    fprintf(stderr, "oss_audio: couldn't reinit speed\n");
    return -1;
  }
  
  if(ioctl(instance->fd, SNDCTL_DSP_GETOSPACE, &info) == -1) {
    perror("SNDCTL_DSP_GETOSPACE");
  } else {
    if(instance->fragment_size != info.fragsize) {
      fprintf(stderr, "oss_audio: fragment size differs after reinit\n");
    }
    if(instance->nr_fragments != info.fragstotal) {
      fprintf(stderr, "oss_audio: nr_fragments size differs after reinit\n");
    }
  }
  
  return 0;
}


static
int oss_play(ogle_ao_instance_t *_instance, void *samples, size_t nbyte)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  int written;
  
  written = write(instance->fd, samples, nbyte);
  if(written == -1) {
    perror("audio write");
    return -1;
  }
  
  return 0;
}

static
int oss_odelay(ogle_ao_instance_t *_instance, uint32_t *samples_return)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  int res;
  int odelay;

  res = ioctl(instance->fd, SNDCTL_DSP_GETODELAY, &odelay);
  if(res == -1) {
    perror("SNDCTL_DSP_GETODELAY");
    return -1;
  }

  *samples_return = odelay / instance->sample_frame_size;

  return 0;
}

static
void oss_close(ogle_ao_instance_t *_instance)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  
  close(instance->fd);
}

static
int oss_flush(ogle_ao_instance_t *_instance)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  
  ioctl(instance->fd, SNDCTL_DSP_RESET, 0);

  //Some cards need a reinit after DSP_RESET, maybe all do?
  oss_reinit(instance);

  return 0;
}

static
int oss_drain(ogle_ao_instance_t *_instance)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  
  ioctl(instance->fd, SNDCTL_DSP_SYNC, 0);
  
  return 0;
}

static 
ogle_ao_instance_t *oss_open(char *dev)
{
    oss_instance_t *instance;
    
    instance = malloc(sizeof(oss_instance_t));
    if(instance == NULL)
      return NULL;
    
    instance->ao.init   = oss_init;
    instance->ao.play   = oss_play;
    instance->ao.close  = oss_close;
    instance->ao.odelay = oss_odelay;
    instance->ao.flush  = oss_flush;
    instance->ao.drain  = oss_drain;
    
    instance->initialized = 0;
    
    instance->fd = open(dev, O_WRONLY);
    if(instance->fd < 0) {
      free(instance);
      return NULL;
    }
    
    return (ogle_ao_instance_t *)instance;
}

/* The one directly exported function */
ogle_ao_instance_t *ao_oss_open(char *dev)
{
  return oss_open(dev);
}

#endif
