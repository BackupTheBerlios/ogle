/*
 * audio_out_solaris.c
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

#ifdef LIBAO_SUNAUDIO

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/audioio.h>

#include <a52dec/a52.h>
#include "audio_out.h"

typedef struct solaris_instance_s {
    ao_instance_t ao;
    int fd;
    int sample_rate;
    int set_params;
    int flags;
} solaris_instance_t;

int solaris_setup (ao_instance_t * _instance, int sample_rate, int * flags,
		   sample_t * level, sample_t * bias)
{
    solaris_instance_t * instance = (solaris_instance_t *) _instance;

    if ((instance->set_params == 0) && (instance->sample_rate != sample_rate))
	return 1;
    instance->sample_rate = sample_rate;
    fprintf(stderr, "apa\n");
    *flags = instance->flags;
    *level = 1;
#if 1
    *bias = 384;
#else
    *bias = 0;
#endif
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

static inline void float_to_int (float * _f, int16_t * s16, int flags)
{
    int i;
#if 1 
    int32_t * f = (int32_t *) _f;
    
    for (i = 0; i < 256; i++) {
	s16[2*i] = convert (f[i]);
	s16[2*i+1] = convert (f[i+256]);
    }
#else
    for (i = 0; i < 256; i++) {
	int i1, i2;
	i1 = (int)(_f[i] * 32767.0);
	i2 = (int)(_f[i+256] * 32767.0);
	
	s16[2*i]   = i1 > 32767 ? 32767 : i1 < -32768 ? -32768 : i1;
	s16[2*i+1] = i2 > 32767 ? 32767 : i2 < -32768 ? -32768 : i2;
    }
#endif
}

int solaris_play (ao_instance_t * _instance, int flags, sample_t * _samples)
{
    solaris_instance_t * instance = (solaris_instance_t *) _instance;
    int16_t int16_samples[256*2];

#ifdef LIBA52_DOUBLE
    float samples[256 * 2];
    int i;

    for (i = 0; i < 256 * 2; i++)
	samples[i] = _samples[i];
#else
    float * samples = _samples;
#endif

    if (instance->set_params) {
	audio_info_t info;

        /* Setup our parameters */
        AUDIO_INITINFO (&info);
	info.play.sample_rate = instance->sample_rate;
	info.play.precision = 16;
	info.play.channels = 2;
	/* info.play.buffer_size = 2048; */
	info.play.encoding = AUDIO_ENCODING_LINEAR;
	/* info.play.port = AUDIO_SPEAKER; */
	/* info.play.gain = 110; */
	
	/* Write our configuration */
	/* An implicit GETINFO is also performed. */
	if (ioctl (instance->fd, AUDIO_SETINFO, &info) < 0) {
	    perror ("Writing audio config block");
	    return 1;
	}

	if ((info.play.sample_rate != instance->sample_rate) ||
	    (info.play.precision != 16) || (info.play.channels != 2)) {
	    fprintf (stderr, "Wanted %dHz %d bits %d channels\n",
		     instance->sample_rate, 16, 2);
	    fprintf (stderr, "Got    %dHz %d bits %d channels\n",
		     info.play.sample_rate, info.play.precision,
		     info.play.channels);
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

    float_to_int (samples, int16_samples, flags);
    write (instance->fd, int16_samples, 256 * sizeof (int16_t) * 2);

    return 0;
}

void solaris_close (ao_instance_t * _instance)
{
    solaris_instance_t * instance = (solaris_instance_t *) _instance;

    close (instance->fd);
}

ao_instance_t * solaris_open (int flags, char *dev)
{
    solaris_instance_t * instance;

    instance = malloc (sizeof (solaris_instance_t));
    if (instance == NULL)
	return NULL;

    instance->ao.setup = solaris_setup;
    instance->ao.play = solaris_play;
    instance->ao.close = solaris_close;

    instance->sample_rate = 0;
    instance->set_params = 1;
    instance->flags = flags;

    instance->fd = open (dev, O_WRONLY);
    if (instance->fd < 0) {
	fprintf (stderr, "Can not open %s\n", dev);
	free (instance);
	return NULL;
    }

    return (ao_instance_t *) instance;
}

ao_instance_t * ao_solaris_open (char *dev)
{
    return solaris_open (A52_STEREO, dev);
}

ao_instance_t * ao_solarisdolby_open (char *dev)
{
    return solaris_open (A52_DOLBY, dev);
}

#endif
