/*
 * audio_out_oss.c
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 *
 * a52dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * a52dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* #include "config.h" */

#ifdef LIBAO_OSS

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#if defined(__OpenBSD__)
#include <soundcard.h>
#elif defined(__FreeBSD__)
#include <machine/soundcard.h>
#ifndef AFMT_S16_NE
#include <machine/endian.h>
#if BYTE_ORDER == LITTLE_ENDIAN
#define AFMT_S16_NE AFMT_S16_LE
#else
#define AFMT_S16_NE AFMT_S16_BE
#endif
#endif
#else
#include <sys/soundcard.h>
#endif

#include <a52dec/a52.h>
#include "audio_out.h"

typedef struct oss_instance_s {
    ao_instance_t ao;
    int fd;
    int sample_rate;
    int set_params;
    int flags;
  int resample_idx;
} oss_instance_t;

int oss_setup (ao_instance_t * _instance, int sample_rate, int * flags,
	       sample_t * level, sample_t * bias)
{
    oss_instance_t * instance = (oss_instance_t *) _instance;

    if ((instance->set_params == 0) && (instance->sample_rate != sample_rate))
	return 1;
    instance->sample_rate = sample_rate;

    *flags = instance->flags;
    *level = 1;
    *bias = 384;

    return 0;
}

static inline int16_t convert (int32_t i)
{
    if (i > 0x43c07fff)
	return 32767;
    else if (i < 0x43bf8000)
	return -32768;
    else
	return i - 0x43c00000;
}

unsigned char tr[235] =
{
  0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 20, 21, 22,
  23, 24, 25, 26, 27, 28, 29, 30, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
  44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 56, 57, 58, 59, 60, 61, 62, 63,
  64, 65, 66, 67, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 81, 82, 83, 84,
  85, 86, 87, 88, 89, 90, 91, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103,
  104, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 118, 119, 120,
  121, 122, 123, 124, 125, 126, 127, 128, 130, 131, 132, 133, 134, 135, 136,
  137, 138, 139, 140, 141, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152,
  153, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 167, 168, 169,
  170, 171, 172, 173, 174, 175, 176, 177, 179, 180, 181, 182, 183, 184, 185,
  186, 187, 188, 189, 190, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201,
  202, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 216, 217, 218,
  219, 220, 221, 222, 223, 224, 225, 226, 227, 229, 230, 231, 232, 233, 234,
  235, 236, 237, 238, 239, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250,
  251, 253, 254, 255
};

static inline void float_to_int235 (float * _f, int16_t * s16, int flags)
{
  int i;
  int32_t * f = (int32_t *) _f;
  
  switch (flags) {
  case A52_MONO:
    for (i = 0; i < 235; i++) {
      s16[5*i] = s16[5*i+1] = s16[5*i+2] = s16[5*i+3] = 0;
           s16[5*i+4] = convert (f[tr[i]]);
    }
    break;
  case A52_CHANNEL:
  case A52_STEREO:
  case A52_DOLBY:
    for (i = 0; i < 235; i++) {
      s16[2*i] = convert (f[tr[i]]);
      s16[2*i+1] = convert (f[tr[i]+256]);
    }
    break;
  case A52_3F:
    for (i = 0; i < 235; i++) {
      s16[5*i] = convert (f[tr[i]]);
      s16[5*i+1] = convert (f[tr[i]+512]);
      s16[5*i+2] = s16[5*i+3] = 0;
      s16[5*i+4] = convert (f[tr[i]+256]);
    }
    break;
  case A52_2F2R:
    for (i = 0; i < 235; i++) {
      s16[4*i] = convert (f[tr[i]]);
      s16[4*i+1] = convert (f[tr[i]+256]);
      s16[4*i+2] = convert (f[tr[i]+512]);
      s16[4*i+3] = convert (f[tr[i]+768]);
    }
    break;
  case A52_3F2R:
    for (i = 0; i < 235; i++) {
      s16[5*i] = convert (f[tr[i]]);
      s16[5*i+1] = convert (f[tr[i]+512]);
      s16[5*i+2] = convert (f[tr[i]+768]);
      s16[5*i+3] = convert (f[tr[i]+1024]);
      s16[5*i+4] = convert (f[tr[i]+256]);
    }
    break;
  case A52_MONO | A52_LFE:
    for (i = 0; i < 235; i++) {
      s16[6*i] = s16[6*i+1] = s16[6*i+2] = s16[6*i+3] = 0;
      s16[6*i+4] = convert (f[tr[i]+256]);
      s16[6*i+5] = convert (f[tr[i]]);
    }
    break;
  case A52_CHANNEL | A52_LFE:
  case A52_STEREO | A52_LFE:
  case A52_DOLBY | A52_LFE:
    for (i = 0; i < 235; i++) {
      s16[6*i] = convert (f[tr[i]+256]);
      s16[6*i+1] = convert (f[tr[i]+512]);
      s16[6*i+2] = s16[6*i+3] = s16[6*i+4] = 0;
      s16[6*i+5] = convert (f[tr[i]]);
    }
    break;
  case A52_3F | A52_LFE:
    for (i = 0; i < 235; i++) {
      s16[6*i] = convert (f[tr[i]+256]);
      s16[6*i+1] = convert (f[tr[i]+768]);
      s16[6*i+2] = s16[6*i+3] = 0;
      s16[6*i+4] = convert (f[tr[i]+512]);
      s16[6*i+5] = convert (f[tr[i]]);
    }
    break;
  case A52_2F2R | A52_LFE:
    for (i = 0; i < 235; i++) {
      s16[6*i] = convert (f[tr[i]+256]);
      s16[6*i+1] = convert (f[tr[i]+512]);
      s16[6*i+2] = convert (f[tr[i]+768]);
      s16[6*i+3] = convert (f[tr[i]+1024]);
      s16[6*i+4] = 0;
      s16[6*i+5] = convert (f[tr[i]]);
    }
    break;
  case A52_3F2R | A52_LFE:
    for (i = 0; i < 235; i++) {
      s16[6*i] = convert (f[tr[i]+256]);
      s16[6*i+1] = convert (f[tr[i]+768]);
      s16[6*i+2] = convert (f[tr[i]+1024]);
      s16[6*i+3] = convert (f[tr[i]+1280]);
      s16[6*i+4] = convert (f[tr[i]+512]);
      s16[6*i+5] = convert (f[tr[i]]);
    }
    break;
  }
}


static inline int float_to_int48_44 (float * _f, int16_t * s16, int flags)
{
  int i;
  int32_t * f = (int32_t *) _f;
  static int m = 0;
  int d, s;

  switch (flags) {
  case A52_MONO:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[5*d] = s16[5*d+1] = s16[5*d+2] = s16[5*d+3] = 0;
      s16[5*d+4] = convert (f[s]);
      
      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  case A52_CHANNEL:
  case A52_STEREO:
  case A52_DOLBY:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[2*d] = convert (f[s]);
      s16[2*d+1] = convert (f[s+256]);
      
      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  case A52_3F:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[5*d] = convert (f[s]);
      s16[5*d+1] = convert (f[s+512]);
      s16[5*d+2] = s16[5*d+3] = 0;
      s16[5*d+4] = convert (f[s+256]);
      
      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  case A52_2F2R:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[4*d] = convert (f[s]);
      s16[4*d+1] = convert (f[s+256]);
      s16[4*d+2] = convert (f[s+512]);
      s16[4*d+3] = convert (f[s+768]);

      
      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  case A52_3F2R:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[5*d] = convert (f[s]);
      s16[5*d+1] = convert (f[s+512]);
      s16[5*d+2] = convert (f[s+768]);
      s16[5*d+3] = convert (f[s+1024]);
      s16[5*d+4] = convert (f[s+256]);

      
      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  case A52_MONO | A52_LFE:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[6*d] = s16[6*d+1] = s16[6*d+2] = s16[6*d+3] = 0;
      s16[6*d+4] = convert (f[s+256]);
      s16[6*d+5] = convert (f[s]);


      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  case A52_CHANNEL | A52_LFE:
  case A52_STEREO | A52_LFE:
  case A52_DOLBY | A52_LFE:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[6*d] = convert (f[s+256]);
      s16[6*d+1] = convert (f[s+512]);
      s16[6*d+2] = s16[6*d+3] = s16[6*d+4] = 0;
      s16[6*d+5] = convert (f[s]);

      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  case A52_3F | A52_LFE:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[6*d] = convert (f[s+256]);
      s16[6*d+1] = convert (f[s+768]);
      s16[6*d+2] = s16[6*d+3] = 0;
      s16[6*d+4] = convert (f[s+512]);
      s16[6*d+5] = convert (f[s]);

      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  case A52_2F2R | A52_LFE:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[6*d] = convert (f[s+256]);
      s16[6*d+1] = convert (f[s+512]);
      s16[6*d+2] = convert (f[s+768]);
      s16[6*d+3] = convert (f[s+1024]);
      s16[6*d+4] = 0;
      s16[6*d+5] = convert (f[s]);

      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  case A52_3F2R | A52_LFE:
    for(s = 0, d = 0; s < 256; m++, d++) {
      if(m%147 == 0) {
	m = 0; 
      }    
      s16[6*d] = convert (f[s+256]);
      s16[6*d+1] = convert (f[s+768]);
      s16[6*d+2] = convert (f[s+1024]);
      s16[6*d+3] = convert (f[s+1280]);
      s16[6*d+4] = convert (f[s+512]);
      s16[6*d+5] = convert (f[s]);

      if(m%12 == 0) {
	s+=2;
      } else {
	s++;
      }
      
    }
    break;
  }
  return d;
}


static inline void float_to_int (float * _f, int16_t * s16, int flags)
{
    int i;
    int32_t * f = (int32_t *) _f;

    switch (flags) {
    case A52_MONO:
	for (i = 0; i < 256; i++) {
	    s16[5*i] = s16[5*i+1] = s16[5*i+2] = s16[5*i+3] = 0;
	    s16[5*i+4] = convert (f[i]);
	}
	break;
    case A52_CHANNEL:
    case A52_STEREO:
    case A52_DOLBY:
	for (i = 0; i < 256; i++) {
	    s16[2*i] = convert (f[i]);
	    s16[2*i+1] = convert (f[i+256]);
	}
	break;
    case A52_3F:
	for (i = 0; i < 256; i++) {
	    s16[5*i] = convert (f[i]);
	    s16[5*i+1] = convert (f[i+512]);
	    s16[5*i+2] = s16[5*i+3] = 0;
	    s16[5*i+4] = convert (f[i+256]);
	}
	break;
    case A52_2F2R:
	for (i = 0; i < 256; i++) {
	    s16[4*i] = convert (f[i]);
	    s16[4*i+1] = convert (f[i+256]);
	    s16[4*i+2] = convert (f[i+512]);
	    s16[4*i+3] = convert (f[i+768]);
	}
	break;
    case A52_3F2R:
	for (i = 0; i < 256; i++) {
	    s16[5*i] = convert (f[i]);
	    s16[5*i+1] = convert (f[i+512]);
	    s16[5*i+2] = convert (f[i+768]);
	    s16[5*i+3] = convert (f[i+1024]);
	    s16[5*i+4] = convert (f[i+256]);
	}
	break;
    case A52_MONO | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = s16[6*i+1] = s16[6*i+2] = s16[6*i+3] = 0;
	    s16[6*i+4] = convert (f[i+256]);
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    case A52_CHANNEL | A52_LFE:
    case A52_STEREO | A52_LFE:
    case A52_DOLBY | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+512]);
	    s16[6*i+2] = s16[6*i+3] = s16[6*i+4] = 0;
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    case A52_3F | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+768]);
	    s16[6*i+2] = s16[6*i+3] = 0;
	    s16[6*i+4] = convert (f[i+512]);
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    case A52_2F2R | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+512]);
	    s16[6*i+2] = convert (f[i+768]);
	    s16[6*i+3] = convert (f[i+1024]);
	    s16[6*i+4] = 0;
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    case A52_3F2R | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+768]);
	    s16[6*i+2] = convert (f[i+1024]);
	    s16[6*i+3] = convert (f[i+1280]);
	    s16[6*i+4] = convert (f[i+512]);
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    }
}

int oss_play (ao_instance_t * _instance, int flags, sample_t * _samples)
{
    oss_instance_t * instance = (oss_instance_t *) _instance;
    int16_t int16_samples[256*6];
    int chans = -1;
    static int downsample = 0;

#ifdef LIBA52_DOUBLE
    float samples[256 * 6];
    int i;

    for (i = 0; i < 256 * 6; i++)
	samples[i] = _samples[i];
#else
    float * samples = _samples;
#endif

    flags &= A52_CHANNEL_MASK | A52_LFE;

    if (flags & A52_LFE)
	chans = 6;
    else if (flags & 1)	/* center channel */
	chans = 5;
    else switch (flags) {
    case A52_2F2R:
	chans = 4;
	break;
    case A52_CHANNEL:
    case A52_STEREO:
    case A52_DOLBY:
	chans = 2;
	break;
    default:
	return 1;
    }

    if (instance->set_params) {
	int tmp;
	instance->resample_idx = 0;
	    
	tmp = chans;
	if ((ioctl (instance->fd, SNDCTL_DSP_CHANNELS, &tmp) < 0) ||
	    (tmp != chans)) {
	    fprintf (stderr, "Can not set number of channels\n");
	    return 1;
	}
	
	downsample = 0;
	tmp = instance->sample_rate;
	fprintf(stderr, "Sample rate %d\n", instance->sample_rate);
	if ((ioctl (instance->fd, SNDCTL_DSP_SPEED, &tmp) < 0) ||
	    (tmp != instance->sample_rate)) {
	  fprintf (stderr, "Can not set sample rate\n");
	  if (instance->sample_rate == 48000) {
	    tmp = 44100;
	    if ((ioctl (instance->fd, SNDCTL_DSP_SPEED, &tmp) < 0) ||
		(tmp != 44100)) {
	      fprintf (stderr, "Can not downsample to 44100\n");
	      return 1;
	    }
	    downsample = 1;
	  } else
	    return 1;
	  
	}

	instance->flags = flags;
	instance->set_params = 0;
    } else if ((flags == A52_DOLBY) && (instance->flags == A52_STEREO)) {
	fprintf (stderr, "Switching from stereo to dolby surround\n");
	instance->flags = A52_DOLBY;
    } else if ((flags == A52_STEREO) && (instance->flags == A52_DOLBY)) {
	fprintf (stderr, "Switching from dolby surround to stereo\n");
	instance->flags = A52_STEREO;
    } else if (flags != instance->flags)
	return 1;

    if (downsample) {
      int num;
      num = float_to_int48_44(samples, int16_samples, flags);
      write(instance->fd, int16_samples, num * sizeof (int16_t) * chans);
    } else {
      float_to_int(samples, int16_samples, flags);
      write(instance->fd, int16_samples, 256 * sizeof (int16_t) * chans);
    }
    
    return 0;
}

void oss_close (ao_instance_t * _instance)
{
    oss_instance_t * instance = (oss_instance_t *) _instance;

    close (instance->fd);
}

ao_instance_t * oss_open (int flags, char *dev)
{
    oss_instance_t * instance;
    int format;

    instance = malloc (sizeof (oss_instance_t));
    if (instance == NULL)
	return NULL;

    instance->ao.setup = oss_setup;
    instance->ao.play = oss_play;
    instance->ao.close = oss_close;

    instance->sample_rate = 0;
    instance->set_params = 1;
    instance->flags = flags;

    instance->fd = open (dev, O_WRONLY);
    if (instance->fd < 0) {
	fprintf (stderr, "Can not open %s\n", dev);
	free (instance);
	return NULL;
    }

    format = AFMT_S16_NE;
    if ((ioctl (instance->fd, SNDCTL_DSP_SETFMT, &format) < 0) ||
	(format != AFMT_S16_NE)) {
	fprintf (stderr, "Can not set sample format\n");
	free (instance);
	return NULL;
    }

    return (ao_instance_t *) instance;
}

ao_instance_t * ao_oss_open (char *dev)
{
    return oss_open (A52_STEREO, dev);
}

ao_instance_t * ao_ossdolby_open (char *dev)
{
    return oss_open (A52_DOLBY, dev);
}

ao_instance_t * ao_oss4_open (char *dev)
{
    return oss_open (A52_2F2R, dev);
}

ao_instance_t * ao_oss6_open (char *dev)
{
    return oss_open (A52_3F2R | A52_LFE, dev);
}

#endif
