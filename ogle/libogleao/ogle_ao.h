#ifndef OGLE_AO_H
#define OGLE_AO_H

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

#include <stdlib.h>
#include <inttypes.h>

typedef enum {
  OGLE_AO_ENCODING_NONE,
  OGLE_AO_ENCODING_LINEAR
} ogle_ao_encoding_t;

/* A sample is a number representing the sound at a specific time.
 * The sample_resolution is the number of bits that encode information
 * in the sample.
 * channels is the number of samples taken at the same time.
 * The sample_frame_size is the space in bytes that is used to store
 * all samples taken at the same time.
 * For normal CD audio (44.1kHz 16bit stereo) this would be:
 * sample_rate = 44100, sample_resolution = 16, channels = 2
 * encoding = OGLE_AO_ENCODING_LINEAR, sample_frame_size = 4
 */
typedef struct {
  uint32_t sample_rate;        // samples per second
  uint32_t sample_resolution;  // bits per sample
  uint32_t channels;           // number of channels (1 = mono, 2 = stereo,...)
  ogle_ao_encoding_t encoding; // ( linear, ulaw, ac3, mpeg, ... )
  uint32_t sample_frame_size;  // the size in bytes of one sample frame
} ogle_ao_audio_info_t;

typedef struct ogle_ao_instance_s ogle_ao_instance_t;

typedef ogle_ao_instance_t *(*ao_open_t)(char *dev);

typedef struct {
    char * name;
    ao_open_t open;
} ao_driver_t;

/* return NULL terminated array of all drivers */
ao_driver_t * ao_drivers (void);


/* Open a given driver, and give it the argument of dev */  
ogle_ao_instance_t *ogle_ao_open(ao_open_t open, char *dev);

/* Call init in the given driver instance */
int ogle_ao_init(ogle_ao_instance_t *instance, 
		 ogle_ao_audio_info_t *audio_info);

/* Queue up nbytes worth of sample data to be played */ 
int ogle_ao_play(ogle_ao_instance_t *instance, void* samples, size_t nbytes);

/* Close the given driver instance */
void ogle_ao_close(ogle_ao_instance_t *instance);

/* Query the number of sample intervals left in the output queue. */
int ogle_ao_odelay(ogle_ao_instance_t *instance,
		   unsigned int *sample_intervals);

/* Discard as much of the queued ouput data as possible (as fast as possible)*/
int ogle_ao_flush(ogle_ao_instance_t *instance);

/* Block until all of the queued output data has been played */
int ogle_ao_drain(ogle_ao_instance_t *instance);

#endif /* OGLE_AO_H */
