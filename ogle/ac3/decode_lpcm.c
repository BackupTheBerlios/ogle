/* Ogle - A video player
 * Copyright (C) 2001, 2002 Björn Englund, Håkan Hjort
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

#include "parse_config.h"
#include "decode.h"
#include "decode_private.h"

#include "audio_config.h"
#include "conversion.h"
#include "audio_play.h"

#include "debug_print.h"

typedef struct {
  adec_handle_t handle;
  uint64_t PTS;
  int pts_valid;
  int scr_nr;
  int sample_rate;
  int quantization_word_length;
  int channels;
  int sample_frame_size;
  uint8_t lpcm_info;
} adec_lpcm_handle_t;


static 
int decode_lpcm(adec_lpcm_handle_t *handle, uint8_t *start, int len,
	       int pts_offset, uint64_t new_PTS, int scr_nr)
{
  static uint8_t *indata_ptr;
  int bytes_left;

  uint8_t audio_frame_number;
  uint8_t new_lpcm_info;
  uint8_t dynamic_range;

  

  //header data
  audio_frame_number = start[0];
  new_lpcm_info = start[1];
  dynamic_range = start[2];

  indata_ptr = start+3; // this (is/should be) an even address
  bytes_left = len-3;


  if(new_lpcm_info != handle->lpcm_info) {
    int new_ch;
    int new_sample_rate;    
    int new_quantization_word_length;
    int new_sample_frame_size;
    int new_sample_size;
    audio_format_t new_format;
    
    handle->lpcm_info = new_lpcm_info;
    if(new_lpcm_info & 0x10) {
      new_sample_rate = 96000;
    } else {
      new_sample_rate = 48000;
    }
    if((new_ch = (new_lpcm_info & 0x07))) {
      new_ch = new_ch + 1;
    } else {
      new_ch = 2; // is mono 2ch(dual mono) or really 1 ch ?
    }
    new_quantization_word_length = (new_lpcm_info & 0xC0) >> 6;
    switch(new_quantization_word_length) {
    case 0:
      new_quantization_word_length = 16;
      new_sample_size = 2;
      break;
    case 1:
      new_quantization_word_length = 20;
      new_sample_size = 4; // ? 20bit contained in 32bit ?
      break;
    case 2:
      new_quantization_word_length = 24;
      new_sample_size = 4; // ? 24bit in 32bit ?
      break;
    default:
      new_sample_size = 0;
      FATAL("lpcm quantization_word_length %d unhandled\n",
	    new_quantization_word_length);
      break;
    }
    new_sample_frame_size = new_ch * new_sample_size; 

    handle->sample_rate = new_sample_rate;
    handle->channels = new_ch;
    handle->quantization_word_length = new_quantization_word_length;
    handle->sample_frame_size = new_sample_frame_size;
    audio_config(handle->handle.config, new_ch,
		 handle->sample_rate,
		 handle->quantization_word_length);

    //change into config format

    
    new_format.ch_array = malloc(handle->channels * sizeof(ChannelType_t));
    new_format.ch_array[0] = ChannelType_Left;
    new_format.ch_array[1] = ChannelType_Right;
    if(handle->channels > 2) {
      FATAL("%d lpcm channels, not implemented, REPORT BUG\n",
	    handle->channels);
    }
    new_format.nr_channels = handle->channels;
    new_format.sample_rate = handle->sample_rate;
    new_format.sample_resolution = handle->quantization_word_length;
    new_format.interleaved = 1;
    new_format.sample_size = new_sample_size;
    new_format.sample_frame_size = new_sample_frame_size;
    new_format.sample_byte_order = 0; // big endian
    new_format.sample_format = SampleFormat_Signed;

    init_sample_conversion((adec_handle_t *)handle, &new_format, 80*7);
    
    free(new_format.ch_array);

  }
  {
    int samples_to_first_au;
    int time_to_first_au;
    samples_to_first_au = (pts_offset-3)/(handle->sample_frame_size);
    time_to_first_au = samples_to_first_au * PTS_BASE / handle->sample_rate;
    new_PTS -= time_to_first_au;
  }
  
  
  handle->PTS = new_PTS;
  handle->pts_valid = 1;
  handle->scr_nr = scr_nr;
  
  convert_samples_start((adec_handle_t *)handle);
  
  
  convert_samples((adec_handle_t *)handle, indata_ptr,
		  bytes_left/handle->sample_frame_size);
  
  
  //output, look over how the pts/scr is handle for sync here 
  play_samples((adec_handle_t *)handle, handle->scr_nr, 
	       handle->PTS, handle->pts_valid);
  handle->pts_valid = 0;
  
  
  return 0;
}


static
int flush_lpcm(adec_lpcm_handle_t *handle)
{
  handle->pts_valid = 0;
  
  // Fix this.. not the right way to do things I belive.
  ogle_ao_flush(handle->handle.config->adev_handle);
  return 0;
}


static
void free_lpcm(adec_lpcm_handle_t *handle)
{
  audio_config_close(handle->handle.config);
  free(handle);
  return;
}


adec_handle_t *init_lpcm(void)
{
  adec_lpcm_handle_t *handle;
  
  if(!(handle = (adec_lpcm_handle_t *)malloc(sizeof(adec_lpcm_handle_t)))) {
    return NULL;
  }
  
  handle->handle.decode = (audio_decode_t) decode_lpcm;  // function pointers
  handle->handle.flush  = (audio_flush_t)  flush_lpcm;
  handle->handle.free   = (audio_free_t)   free_lpcm;
  handle->handle.output_buf = NULL;
  handle->handle.output_buf_size = 0;
  handle->handle.output_buf_ptr = handle->handle.output_buf;

  handle->PTS = 0;
  handle->pts_valid = 0;
  handle->scr_nr = 0;
  handle->sample_rate = 0;
  //  handle->decoded_format = NULL;

  
  return (adec_handle_t *)handle;
}
