/* Ogle - A video player
 * Copyright (C) 2002 Bj�rn Englund
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

#ifdef LIBOGLEAO_OBSD

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <inttypes.h>

#include <fcntl.h>
#include <sys/audioio.h>

#include <sys/types.h>
#include <sys/conf.h>

#include <string.h>

#include "ogle_ao.h"
#include "ogle_ao_private.h"

typedef struct obsd_instance_s {
  ogle_ao_instance_t ao;
  int fd;
  int sample_rate;
  int samples_written;
  int sample_frame_size;
  int initialized;
} obsd_instance_t;


static
int obsd_init(ogle_ao_instance_t *_instance,
		 ogle_ao_audio_info_t *audio_info)
{
  int single_sample_size;
  audio_info_t info;
  obsd_instance_t *instance = (obsd_instance_t *)_instance;
  
  if(instance->initialized) {
    return -1;  // for now we must close and open the device for reinit
  }

  AUDIO_INITINFO(&info);
  info.play.sample_rate = audio_info->sample_rate;
  info.play.precision   = audio_info->sample_resolution;
  info.play.channels = audio_info->channels;
  info.lowat = 5;
  switch(audio_info->encoding) {
  case OGLE_AO_ENCODING_LINEAR:
  default:
    info.play.encoding = AUDIO_ENCODING_LINEAR;
    break;
  }
  
  /* set the audio format */
  if(ioctl(instance->fd, AUDIO_SETINFO, &info) == -1) {
    perror("setinfo");
    return -1;
  }
  
  /* Check that we actually got the requested nuber of channles,
   * the frequency and precision that we asked for?  */
  
  /* Stored as 8bit, 16bit, or 32bit */
  single_sample_size = (info.play.precision + 7) / 8;
  if(single_sample_size > 2) {
    single_sample_size = 4;
  }
  instance->sample_frame_size = single_sample_size*info.play.channels;
  instance->initialized = 1;
  
  audio_info->sample_frame_size = instance->sample_frame_size;
  return 0;
}

static
int obsd_play(ogle_ao_instance_t *_instance, void *samples, size_t nbyte)
{
  obsd_instance_t *instance = (obsd_instance_t *)_instance;
  int written;
  
  written = write(instance->fd, samples, nbyte);
  if(written == -1) {
    perror("audio write");
    return -1;
  }
  
  instance->samples_written += written / instance->sample_frame_size;
  return 0;
}

static
int obsd_odelay(ogle_ao_instance_t *_instance, uint32_t *samples_return)
{
  obsd_instance_t *instance = (obsd_instance_t *)_instance;
  audio_info_t info;
  int res;
  int odelay;
  static int e = 0;
  u_int samples_played, seek;
  

  res = ioctl(instance->fd, AUDIO_GETINFO, &info);
  if(res == -1) {
    perror("AUDIO_GETINFO");
    return -1;
  }

  samples_played = info.play.samples;
  seek = info.play.seek;
  res = ioctl(instance->fd, AUDIO_WSEEK, &odelay );

  /*  fprintf(stderr, ":: %d :: %d :: %d :: %d -- e: %d, %d\n", 
	  instance->samples_written,
	  info.play.samples,
	  info.play.seek,
	  odelay,
	  info.play.error,
	  e);*/
  
  if(info.play.error) {
    e++;
    res = ioctl(instance->fd, AUDIO_FLUSH, 0);
    if(res == -1) {
      perror("AUDIO_FLUSH");
    }
    instance->samples_written = 0;
  }/*    if(info.play.samples != samples_played) {
      fprintf(stderr, "underrun and more samples played: %d, %d\n",
	      samples_played, info.play.samples);
    }
    info.play.samples = 0;
  }
  
  if(instance->samples_written < info.play.samples) {
    //something is wrong with the accounting
    fprintf(stderr, "written: %d < played: %d\n",
	    instance->samples_written, info.play.samples);
  }
  
  odelay = instance->samples_written - info.play.samples;
  */
  odelay = seek / instance->sample_frame_size;
  fprintf(stderr, "odelay: %d\n", odelay);
  
  *samples_return = odelay;

  return 0;
}

static
void obsd_close(ogle_ao_instance_t *_instance)
{
  obsd_instance_t *instance = (obsd_instance_t *)_instance;
  
  close(instance->fd);
}

static
int obsd_flush(ogle_ao_instance_t *_instance)
{
  obsd_instance_t *instance = (obsd_instance_t *)_instance;
  /*  audio_info_t info;
  
  AUDIO_INITINFO(&info);
  info.play.pause = 1;
  ioctl(instance->fd, AUDIO_SETINFO, &info);*/

  fprintf(stderr, "AUDIO_FLUSH called\n");
  ioctl(instance->fd, AUDIO_FLUSH, NULL);
  
  /*  AUDIO_INITINFO(&info);
  info.play.pause = 0;
  info.play.error = 0;
  info.play.samples = 0;
  ioctl(instance->fd, AUDIO_SETINFO, &info);*/
  
  instance->samples_written = 0;
  
  ioctl(instance->fd, AUDIO_DRAIN, 0);
  
  return 0;
}

static
int obsd_drain(ogle_ao_instance_t *_instance)
{
  obsd_instance_t *instance = (obsd_instance_t *)_instance;
  
  ioctl(instance->fd, AUDIO_DRAIN, 0);
  
  return 0;
}

static 
ogle_ao_instance_t *obsd_open(char *dev)
{
    obsd_instance_t *instance;
    
    instance = malloc(sizeof(obsd_instance_t));
    if(instance == NULL)
      return NULL;
    
    instance->ao.init   = obsd_init;
    instance->ao.play   = obsd_play;
    instance->ao.close  = obsd_close;
    instance->ao.odelay = obsd_odelay;
    instance->ao.flush  = obsd_flush;
    instance->ao.drain  = obsd_drain;
    
    instance->initialized = 0;
    instance->sample_rate = 0;
    instance->samples_written = 0;
    instance->sample_frame_size = 0;
    
    instance->fd = open(dev, O_WRONLY);
    if(instance->fd < 0) {
      fprintf(stderr, "Can not open %s\n", dev);
      free(instance);
      return NULL;
    }
    
    return (ogle_ao_instance_t *)instance;
}

/* The one directly exported function */
ogle_ao_instance_t *ao_obsd_open(char *dev)
{
  return obsd_open(dev);
}

#endif
