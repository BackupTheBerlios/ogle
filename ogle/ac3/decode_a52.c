/* Ogle - A video player
 * Copyright (C) 2001, 2002 Bj�rn Englund, H�kan Hjort
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
#include <stdio.h>
#include <inttypes.h>

#include <a52dec/a52.h>
#include <a52dec/mm_accel.h>

#include <libogleao/ogle_ao.h> // Remove me

#include "decode.h"
#include "decode_private.h"

#include "audio_config.h"
#include "conversion.h"
#include "audio_play.h"


typedef struct {
  adec_handle_t handle;
  uint64_t PTS;
  int pts_valid;
  int scr_nr;
  uint8_t *coded_buf;
  uint8_t *buf_ptr;
  int sample_rate;
  int bytes_needed;
  int availflags;
  int output_flags;
  int decoding_flags;
  /* A/52 */
  sample_t *samples;
  int disable_dynrng;
} adec_a52_handle_t;


static
int a52flags_to_channels(int flags)
{
  int ch = 0;
  
  switch(flags & A52_CHANNEL_MASK) {
  case A52_CHANNEL:
    //TODO ???
    ch = ChannelType_Left | ChannelType_Right;
    break;
  case A52_MONO:
    ch = ChannelType_Center;
    break;
  case A52_STEREO:
  case A52_DOLBY:
    ch = ChannelType_Left | ChannelType_Right;
    break;
  case A52_3F:
    ch = ChannelType_Left | ChannelType_Center | ChannelType_Right;
    break;
  case A52_2F1R:
    ch = ChannelType_Left | ChannelType_Right | ChannelType_Surround;
    break;
  case A52_3F1R:
    ch = ChannelType_Left | ChannelType_Center | ChannelType_Right | 
      ChannelType_Surround;
    break;
  case A52_2F2R:
    ch = ChannelType_Left | ChannelType_Right | 
      ChannelType_LeftSurround | ChannelType_RightSurround;
    break;
  case A52_3F2R:
    ch = ChannelType_Left | ChannelType_Center | ChannelType_Right |
      ChannelType_LeftSurround | ChannelType_RightSurround;
    break;
    
  }
  
  if(flags & A52_LFE) {
    ch |= ChannelType_LFE;
  }
  
  return ch;
}

static
int config_to_a52flags(audio_config_t *conf)
{
  int i;
  int ch = 0;
  int hasLFE = 0;

  for(i = 0; i < conf->format.nr_channels; i++) {
    if(conf->format.ch_array[i] == ChannelType_LFE)
      hasLFE = A52_LFE;
    else
      ch |= conf->format.ch_array[i];
  }
  switch(ch) {
  case ChannelType_Center:
    return A52_MONO | hasLFE;
  case ChannelType_Left | ChannelType_Right:
    return A52_STEREO | hasLFE; // or A52_CHANNEL or A52_DOLBY
  case ChannelType_Left | ChannelType_Center | ChannelType_Right:
    return A52_3F | hasLFE;
  case ChannelType_Left | ChannelType_Right | ChannelType_Surround:
    return A52_2F1R | hasLFE;
  case ChannelType_Left | ChannelType_Center | ChannelType_Right | ChannelType_Surround:
    return A52_3F1R | hasLFE;
  case ChannelType_Left | ChannelType_Right |  ChannelType_LeftSurround | ChannelType_RightSurround:
    return A52_2F2R | hasLFE;
  case ChannelType_Left | ChannelType_Center | ChannelType_Right | ChannelType_LeftSurround | ChannelType_RightSurround:
    return A52_3F2R | hasLFE;
  default:
  }
  return A52_3F2R | hasLFE; // Some strange sound configuration...
}


static 
void a52flags_to_format(int flags, int *channels, ChannelType_t *channel[])
{
  int f = flags & A52_CHANNEL_MASK;
  int nr_channels = 0;
  ChannelType_t *ch_array;

  ch_array = malloc(6 * sizeof(ChannelType_t));

  if(f == A52_CHANNEL) { 
    ch_array[nr_channels++] = ChannelType_Left; //??
    ch_array[nr_channels++] = ChannelType_Right; //??
  } if(f == A52_MONO || f == A52_CHANNEL1 || f == A52_CHANNEL2) {
    ch_array[nr_channels++] = ChannelType_Center;
  } else {
    if(1) { // left
      ch_array[nr_channels++] = ChannelType_Left;
    }
    if(f == A52_3F || f == A52_3F1R || f == A52_3F2R) { // center
      ch_array[nr_channels++] = ChannelType_Center;
    }
    if(1) { // right
      ch_array[nr_channels++] = ChannelType_Right;
    }
    if(f == A52_2F1R || f == A52_3F1R) { // mono surround
      ch_array[nr_channels++] = ChannelType_Surround;
    }
    if(f == A52_2F2R || f == A52_3F2R) { // left and right surround
      ch_array[nr_channels++] = ChannelType_LeftSurround;
      ch_array[nr_channels++] = ChannelType_RightSurround;
    }
  }
  if(flags & A52_LFE) { // sub
    ch_array[nr_channels++] = ChannelType_LFE;
  }

  *channels = nr_channels;
  *channel = ch_array;
}



static 
int decode_a52(adec_a52_handle_t *handle, uint8_t *start, int len,
	       int pts_offset, uint64_t new_PTS, int scr_nr)
{
  static uint8_t *indata_ptr;
  int bytes_left;
  int frame_len;
  int new_sample_rate;
  int bit_rate;
  int n;
  int bytes_to_get;

  indata_ptr = start;
  bytes_left = len;
  

  while(bytes_left > 0) {
    if(handle->bytes_needed > bytes_left) {
      bytes_to_get = bytes_left;
      memcpy(handle->buf_ptr, indata_ptr, bytes_to_get);
      bytes_left -= bytes_to_get;
      handle->buf_ptr += bytes_to_get;
      indata_ptr += bytes_to_get;
      handle->bytes_needed -= bytes_to_get;
      
      //fprintf(stderr, "bytes_needed: %d > bytes_left: %d\n",
      //      handle->bytes_needed, bytes_left);
      return handle->bytes_needed;
    } else {
      bytes_to_get = handle->bytes_needed;
      memcpy(handle->buf_ptr, indata_ptr, bytes_to_get);
      bytes_left -= bytes_to_get;
      handle->buf_ptr += bytes_to_get;
      indata_ptr += bytes_to_get;
      handle->bytes_needed -= bytes_to_get;
    }
    
    // when we come here we have all data we want
    // either the 7 header bytes or the whole frame
    
    if(handle->buf_ptr - handle->coded_buf == 7) {
      // 7 bytes header data to test
      int new_availflags;
      
      frame_len = a52_syncinfo(handle->coded_buf,
			       &new_availflags, &new_sample_rate, &bit_rate);
      if(frame_len == 0) {
	//this is not the start of a frame
	fprintf(stderr, "*** a52 lost sync\n");
	for(n = 0; n < 6; n++) {
	  handle->coded_buf[n] = handle->coded_buf[n+1];
	}
	handle->buf_ptr--;
	handle->bytes_needed = 1;
      } else {

	// we have the start of a frame
	if((int)(indata_ptr-7-start) == pts_offset) {
	  // this frame has a pts
	  //fprintf(stderr, "frame with pts\n");
	  handle->PTS = new_PTS;
	  handle->pts_valid = 1;
	  handle->scr_nr = scr_nr;
	} else {
	  //fprintf(stderr, "frame with no pts\n");
	}

	handle->bytes_needed = frame_len - 7;
	if((new_availflags != handle->availflags) || 
	   (new_sample_rate != handle->sample_rate)) {
	  int ch;
	  
	  //fprintf(stderr, "new flags\n");

	  handle->availflags = new_availflags;
	  handle->sample_rate = new_sample_rate;
	  
	  //change a52 flags to generic channel flags
	  
	  ch = a52flags_to_channels(handle->availflags);
	  audio_config(handle->handle.config, ch, handle->sample_rate, 16);
	  //change config into a52dec flags
	  handle->output_flags = config_to_a52flags(handle->handle.config);
	}
	
      }
    } else {
      a52_state_t state;
      int i;
      int flags;
      sample_t level = 1;  // Hack for the float_to_int function
      int bias = 384;
      //we have a whole frame to decode
      //fprintf(stderr, "decode frame\n");
      //decode
      flags = handle->output_flags;
      if(a52_frame(&state, handle->coded_buf, &flags, &level, bias)) {
	fprintf(stderr, "a52_frame error\n");
	//DNOTE("a52_frame() error\n");
	goto error;
      }

      if(handle->decoding_flags != flags) {
	//new set of channels decoded
	//change into config format
	audio_format_t new_format;

	handle->decoding_flags = flags;
	
	a52flags_to_format(flags, &new_format.nr_channels,
			   &new_format.ch_array);
      
	new_format.sample_rate = handle->sample_rate;
	new_format.sample_resolution = 16;

	init_sample_conversion((adec_handle_t *)handle, &new_format, 256*6);

	free(new_format.ch_array);
      }

      convert_samples_start((adec_handle_t *)handle);

	
      if(handle->disable_dynrng) {
	a52_dynrng(&state, NULL, NULL);
      }
      for(i = 0; i < 6; i++) {

	if(a52_block(&state, handle->samples)) {
	  fprintf(stderr, "a52_block error\n");
	  //DNOTE("a52_block() error\n");
	  goto error;
	}
	convert_samples((adec_handle_t *)handle, handle->samples, 256);
      }


      //output, look over how the pts/scr is handle for sync here 
      play_samples((adec_handle_t *)handle, handle->scr_nr, 
		   handle->PTS, handle->pts_valid);
      handle->pts_valid = 0;

    error:

      //make space for next frame
      handle->buf_ptr = handle->coded_buf;
      handle->bytes_needed = 7;
    }
    
    //fprintf(stderr, "bytes_left: %d\n", bytes_left);
  }
  //fprintf(stderr, "bytes_needed: %d\n", handle->bytes_needed);
  return handle->bytes_needed;
}


static
int flush_a52(adec_a52_handle_t *handle)
{
  handle->pts_valid = 0;
  handle->buf_ptr = handle->coded_buf;
  handle->bytes_needed = 7;
  
  // Fix this.. not the right way to do things I belive.
  ogle_ao_flush(handle->handle.config->adev_handle);
  return 0;
}


static
void free_a52(adec_a52_handle_t *handle)
{
  free(handle->coded_buf);
  free(handle);
  return;
}


adec_handle_t *init_a52(void)
{
  adec_a52_handle_t *handle;
  
  if(!(handle = (adec_a52_handle_t *)malloc(sizeof(adec_a52_handle_t)))) {
    return NULL;
  }
  
  handle->handle.decode = (audio_decode_t) decode_a52;  // function pointers
  handle->handle.flush  = (audio_flush_t)  flush_a52;
  handle->handle.free   = (audio_free_t)   free_a52;
  handle->handle.output_samples = NULL;
  handle->handle.output_samples_size = 0;
  handle->handle.output_samples_ptr = handle->handle.output_samples;

  handle->PTS = 0;
  handle->pts_valid = 0;
  handle->scr_nr = 0;
  handle->coded_buf = (uint8_t *)malloc(3840); // max size of a52 frame
  handle->buf_ptr = (uint8_t *)handle->coded_buf;
  handle->bytes_needed = 7;
  handle->sample_rate = 0;
  //  handle->decoded_format = NULL;

  {
    uint32_t accel;
    accel = MM_ACCEL_MLIB;

    handle->samples = a52_init(accel);
    if(handle->samples == NULL) {
        fprintf(stderr, "A/52 init failed\n");
        exit(1);
    }
  }
  handle->disable_dynrng = 0;
  
  return (adec_handle_t *)handle;
}
