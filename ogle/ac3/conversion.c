/* Ogle - A video player
 * Copyright (C) 2002 Björn Englund, Håkan Hjort
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

#include <stdio.h>

#include "conversion.h"
#include "decode_private.h" // Fixme

/*
 * needs to know input and output format
 *
 * channel configuration
 * sample rate
 * nr_samples
 * sample precision
 * sample encoding
 */

typedef struct {
  int *channel_conf;
} conversion_t;



void setup_channel_conf(int *ch_conf, int nr_ch, int *input_ch, int *output_ch)
{
  int n,m;

  for(n = 0; n < nr_ch; n++) {
    for(m = 0; m < nr_ch; m++) {
      if(input_ch[m] == output_ch[n]) {
	ch_conf[n] = m;
	break;
      }
    }
  }
  
}

int init_sample_conversion(adec_handle_t *h, audio_format_t *format,
			   int nr_samples)
{
  // nr samples, the max number of samples that will be converted
  // between each play
  int size = h->config->format.nr_channels * h->config->format.sample_resolution;
  fprintf(stderr, "size: %u\n", size);
  if(h->output_buf_size < nr_samples * size) {
    h->output_buf_size = nr_samples * size;
    h->output_buf = realloc(h->output_buf, h->output_buf_size);
    if(h->output_buf == NULL) {
      fprintf(stderr, "realloc failed\n");
    }
  }
  fprintf(stderr, "output_buf: %ld\n", (long)h->output_buf);
  fprintf(stderr, "output_buf_size: %d\n", h->output_buf_size);
  h->output_buf_ptr = h->output_buf;
  
  // setup the conversion functions
  // from the sample buffer format to the output buffer format
  switch(format->sample_format) {
  case SampleFormat_A52float:
  case SampleFormat_MadFixed:
  case SampleFormat_Unsigned:
  case SampleFormat_Signed:
  }
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


static int convert_a52_float_to_s16(float * _f, int16_t *s16, int nr_samples,
			     int nr_channels, int *channels)
{
  int i;
  int32_t *f = (int32_t *)_f;
  
  // assert(nr_channels == 2);    
  // assert(nr_samples == 256);

  for (i = 0; i < nr_samples; i++) {
    s16[2*i] = convert(f[i]);
    s16[2*i+1] = convert(f[i+256]);
  }

  
  return 0;
}


int convert_samples(adec_handle_t *h, void *samples, int nr_samples)
{
  //convert from samples to output_buf_ptr
  /*
  fprintf(stderr, "samples: %d, output_buf_ptr: %d\n",
	  samples, h->output_buf_ptr);
  */
  convert_a52_float_to_s16((float *)samples, (int16_t *)h->output_buf_ptr, 
			   nr_samples, 2, NULL);
  h->output_buf_ptr += 2*2*nr_samples; // 2ch 2byte
  return 0;
}


void convert_samples_start(adec_handle_t *h)
{
  h->output_buf_ptr = h->output_buf;
}




