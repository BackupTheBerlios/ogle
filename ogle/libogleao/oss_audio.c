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
#include <stropts.h>
#include <stdio.h>

#if defined(__OpenBSD__)
#include <soundcard.h>
#elif defined(__FreeBSD__)
#include <machine/soundcard.h>
#else
#include <sys/soundcard.h>
#endif

#include "ogle_ao.h"
#include "ogle_ao_private.h"

typedef struct oss_instance_s {
  ogle_ao_instance_t ao;
  int fd;
  int sample_size;
  int initialized;
} oss_instance_t;


static
int oss_init(ogle_ao_instance_t *_instance,
		 ogle_ao_audio_info_t *audio_info)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  int single_sample_size;
  int number_of_channels, sample_format, original_sample_format, sample_speed;
  
  if(instance->initialized) {
    return -1;  // for now we must close and open the device for reinit
  }

  // Set sample format, number of channels, and sample speed
  // The order here is important according to the manual

  switch(audio_info->encoding) {
  case OGLE_AO_ENCODING_LINEAR:
    switch(audio_info->sample_resolution) {
      case 16:
        sample_format = AFMT_S16_LE;
        break;
      default:
        return -1;
        break;
    }
    break;
  default:
    return -1;
    break;
  }
  original_sample_format = sample_format;
  if(ioctl(instance->fd, SNDCTL_DSP_SETFMT, &sample_format) == -1) {
    perror("SNDCTL_DSP_SETFMT");
    return -1;
  }

  // Test if we got the right format
  if (sample_format != original_sample_format) {
    return -1;
  }

  number_of_channels = audio_info->channels;
  if(ioctl(instance->fd, SNDCTL_DSP_CHANNELS, &number_of_channels) == -1) {
    perror("SNDCTL_DSP_CHANNELS");
    return -1;
  }
  // report back (maybe we can't do stereo or something...)
  audio_info->channels = number_of_channels;

  sample_speed = audio_info->sample_rate;
  if(ioctl(instance->fd, SNDCTL_DSP_SPEED, &sample_speed) == -1) {
    perror("SNDCTL_DSP_SPEED");
    return -1;
  }
  // report back the actual speed used
  audio_info->sample_rate = sample_speed;

  single_sample_size = (audio_info->sample_resolution + 7) / 8;
  if(single_sample_size > 2) {
    single_sample_size = 4;
  }
  instance->sample_size = single_sample_size*number_of_channels;
  instance->initialized = 1;
  
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

  *samples_return = odelay / instance->sample_size;

  return 1;
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
  
  ioctl(instance->fd, I_FLUSH, FLUSHW);
  ioctl(instance->fd, SNDCTL_DSP_SYNC, 0);
  
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
