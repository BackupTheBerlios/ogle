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
#include <string.h>
#include <inttypes.h>


#include <libogleao/ogle_ao.h> // Remove me
#include <mad.h>

#include "decode.h"
#include "decode_private.h"

#include "audio_config.h"
#include "conversion.h"
#include "audio_play.h"

#include "decode_mpeg.h"

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

  /* libmad */
  struct mad_stream stream;
  struct mad_frame frame;
  struct mad_synth synth;
  mad_timer_t timer;

} adec_mpeg_handle_t;


#if 0
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
#endif


static 
int decode_mpeg(adec_mpeg_handle_t *handle, uint8_t *start, int len,
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
  
  

  /***/


  mad_stream_buffer(&handle->stream, start, len);
  handle->stream.error = 0;

  
  while(mad_frame_decode(&handle->frame, &handle->stream) == 0) {
    // one frame decoded ok
    fprintf(stderr, "curframe: %u, nextframe: %u\n",
	    handle->stream.this_frame,
	    handle->stream.next_frame);
      /*
	//convert to pcm samples
	mad_synth_frame(&handle->synth, &handle->frame);
      */
      
      }
  fprintf(stderr, "end curframe: %u, nextframe: %u\n",
	  handle->stream.this_frame,
	  handle->stream.next_frame);
  
  frame_len = handle->stream.next_frame - handle->stream.this_frame;
  /*
  if(handle->frame.next_frame - start
  */
  /***/
  
  return 0;
}


static
int flush_mpeg(adec_mpeg_handle_t *handle)
{
  handle->pts_valid = 0;
  handle->buf_ptr = handle->coded_buf;
  handle->bytes_needed = 7;
  
  // Fix this.. not the right way to do things I belive.
  ogle_ao_flush(handle->handle.config->adev_handle);
  return 0;
}


static
void free_mpeg(adec_mpeg_handle_t *handle)
{
  free(handle->coded_buf);
  free(handle);
  return;
}


adec_handle_t *init_mpeg(void)
{
  adec_mpeg_handle_t *handle;
  
  if(!(handle = (adec_mpeg_handle_t *)malloc(sizeof(adec_mpeg_handle_t)))) {
    return NULL;
  }
  
  handle->handle.decode = (audio_decode_t) decode_mpeg;  // function pointers
  handle->handle.flush  = (audio_flush_t)  flush_mpeg;
  handle->handle.free   = (audio_free_t)   free_mpeg;
  handle->handle.output_buf = NULL;
  handle->handle.output_buf_size = 0;
  handle->handle.output_buf_ptr = handle->handle.output_buf;

  handle->PTS = 0;
  handle->pts_valid = 0;
  handle->scr_nr = 0;
  handle->coded_buf = (uint8_t *)malloc(2048); // max? size of mpeg frame
  handle->buf_ptr = (uint8_t *)handle->coded_buf;
  handle->bytes_needed = 7;
  handle->sample_rate = 0;
  //  handle->decoded_format = NULL;

  {
    
    mad_stream_init(&handle->stream);
    mad_frame_init(&handle->frame);
    mad_synth_init(&handle->synth);
    mad_timer_reset(&handle->timer);

  }
  
  return (adec_handle_t *)handle;
}
