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

#include <mad.h>

#include "debug_print.h"
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

static ChannelType_t ch_transform[10];
static int src_ch;
static int dst_ch;
static int conversion_routine = 0;

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

int init_sample_conversion(adec_handle_t *h,
			   audio_format_t *src_format,
			   int nr_samples)
{
  // nr samples, the max number of samples that will be converted
  // between each play
  int m;
  int n;
  int output_frame_size = h->config->dst_format.sample_frame_size;

  if(src_format->sample_format == SampleFormat_DTSFrame) {
    
    if(h->output_buf_size < nr_samples * output_frame_size) {
      h->output_buf_size = nr_samples * output_frame_size;
      h->output_buf = realloc(h->output_buf, h->output_buf_size);
      if(h->output_buf == NULL) {
	FATAL("init_sample_conversion2, realloc failed\n", 0);
	exit(1); // ?
      }
    }
    
  } else if(src_format->sample_format == SampleFormat_AC3Frame) {
    
    if(h->output_buf_size < 256*6*2*2) {
      h->output_buf_size = 256*6*2*2;
      h->output_buf = realloc(h->output_buf, h->output_buf_size);
      if(h->output_buf == NULL) {
	FATAL("init_sample_conversion2, realloc failed\n", 0);
	exit(1); // ?
      }
    }
    
  } else {
    if(h->output_buf_size < nr_samples * output_frame_size) {
      h->output_buf_size = nr_samples * output_frame_size;
      h->output_buf = realloc(h->output_buf, h->output_buf_size);
      if(h->output_buf == NULL) {
	FATAL("init_sample_conversion, realloc failed\n", 0);
	exit(1); // ?
      }
    }
    //fprintf(stderr, "output_buf: %ld\n", (long)h->output_buf);
    //fprintf(stderr, "output_buf_size: %d\n", h->output_buf_size);
    h->output_buf_ptr = h->output_buf;
    
    dst_ch = h->config->dst_format.nr_channels;
    src_ch = src_format->nr_channels;
    if(dst_ch > 10) {
      FATAL("*** more than 10 channels\n", 0);
      exit(1);
    }
    for(n = 0; n < h->config->dst_format.nr_channels; n++) {
      for(m = 0; m < src_format->nr_channels; m++) {
	if(h->config->dst_format.ch_array[n] == src_format->ch_array[m]) {
	  ch_transform[n] = m;
	  break;
	}
      }
      if(m == src_format->nr_channels) {
	//type not in src
	ch_transform[n] = -1;
      }
    }
  }
  // setup the conversion functions
  // from the sample buffer format to the output buffer format
  switch(src_format->sample_format) {
  case SampleFormat_A52float:
    conversion_routine = 0;
    break;
  case SampleFormat_Signed:
    conversion_routine = 1;
    break;
  case SampleFormat_MadFixed:
    conversion_routine = 2;
    break;
  case SampleFormat_AC3Frame:
    conversion_routine = 3;
    break;
  case SampleFormat_DTSFrame:
    conversion_routine = 4;
    break;
  case SampleFormat_Unsigned:
  default:
    FATAL("init_conversion: SampleFormat %d not supported\n",
	  src_format->sample_format);
    break;
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
  int n;
  int32_t *f = (int32_t *)_f;
  
  // assert(nr_channels == 2);    
  // assert(nr_samples == 256);

  for (i = 0; i < nr_samples; i++) {
    for(n = 0; n < dst_ch; n++) {
      if(ch_transform[n] != -1) {
	s16[dst_ch*i+n] = convert(f[i+ch_transform[n]*256]);
      } else {
	s16[dst_ch*i+n] = 0;
      }
    }
  }
  
  
  return 0;
}


/** code borrowed from vlc **/
static inline int16_t convert_mad(mad_fixed_t fixed)
{
  /* round */
  fixed += (1L << (MAD_F_FRACBITS - 16));
  
  /* clip */
  if (fixed >= MAD_F_ONE)
    fixed = MAD_F_ONE - 1;
  else if (fixed < -MAD_F_ONE)
    fixed = -MAD_F_ONE;
  
  /* quantize */
  return (int16_t)(fixed >> (MAD_F_FRACBITS + 1 - 16));
}

static int convert_mad_fixed_to_s16(mad_fixed_t *m, int16_t *s16,
				    int nr_samples,
				    int nr_channels, int *channels)
{
  int i;
  
  // assert(nr_channels == 2);    
  
  for (i = 0; i < nr_samples; i++) {
    s16[2*i] = convert_mad(m[i]);
    s16[2*i+1] = convert_mad(m[i+1152]);
  }

  
  return 0;
}

static int convert_s16be_to_s16ne(int16_t *s16be, int16_t *s16ne,
				  int nr_samples, int nr_channels,
				  int *channels)
{
  int i;

#if WORDS_BIGENDIAN == 1
  memcpy(s16ne, s16be, nr_samples * nr_channels * sizeof(int16_t));
#else
  for (i = 0; i < nr_samples * nr_channels; i++) {
    ((uint16_t *)s16ne)[i] =
      ((((uint16_t *)s16be)[i] >> 8) & 0xff) | 
      ((((uint16_t *)s16be)[i] << 8) & 0xff00);
  }

#endif
  
  return 0;
}

struct frmsize_s
{
  uint16_t bit_rate;
  uint16_t frm_size[3];
};

static const struct frmsize_s frmsizecod_tbl[64] =
{
  { 32  ,{64   ,69   ,96   } },
  { 32  ,{64   ,70   ,96   } },
  { 40  ,{80   ,87   ,120  } },
  { 40  ,{80   ,88   ,120  } },
  { 48  ,{96   ,104  ,144  } },
  { 48  ,{96   ,105  ,144  } },
  { 56  ,{112  ,121  ,168  } },
  { 56  ,{112  ,122  ,168  } },
  { 64  ,{128  ,139  ,192  } },
  { 64  ,{128  ,140  ,192  } },
  { 80  ,{160  ,174  ,240  } },
  { 80  ,{160  ,175  ,240  } },
  { 96  ,{192  ,208  ,288  } },
  { 96  ,{192  ,209  ,288  } },
  { 112 ,{224  ,243  ,336  } },
  { 112 ,{224  ,244  ,336  } },
  { 128 ,{256  ,278  ,384  } },
  { 128 ,{256  ,279  ,384  } },
  { 160 ,{320  ,348  ,480  } },
  { 160 ,{320  ,349  ,480  } },
  { 192 ,{384  ,417  ,576  } },
  { 192 ,{384  ,418  ,576  } },
  { 224 ,{448  ,487  ,672  } },
  { 224 ,{448  ,488  ,672  } },
  { 256 ,{512  ,557  ,768  } },
  { 256 ,{512  ,558  ,768  } },
  { 320 ,{640  ,696  ,960  } },
  { 320 ,{640  ,697  ,960  } },
  { 384 ,{768  ,835  ,1152 } },
  { 384 ,{768  ,836  ,1152 } },
  { 448 ,{896  ,975  ,1344 } },
  { 448 ,{896  ,976  ,1344 } },
  { 512 ,{1024 ,1114 ,1536 } },
  { 512 ,{1024 ,1115 ,1536 } },
  { 576 ,{1152 ,1253 ,1728 } },
  { 576 ,{1152 ,1254 ,1728 } },
  { 640 ,{1280 ,1393 ,1920 } },
  { 640 ,{1280 ,1394 ,1920 } }
};

static int convert_ac3frame_to_iec61937frame(uint16_t *ac3,
					     uint16_t *iec61937,
					     int nr_samples)
{
  int i;
  
  uint32_t syncword, crc1, fscod,frmsizecod,bsid,bsmod,frame_size;
  uint8_t *data_out = (uint8_t *)iec61937;
  uint8_t *data_in = (uint8_t *)ac3;
  int frame_bytes;
  
  if (ac3[0] != 0x770b) {
    fprintf(stderr, "SYNC/////////\n");
  }
  fscod = (data_in[4] >> 6) & 0x3;
  frmsizecod = data_in[4] & 0x3f;
  bsmod = data_in[5] & 0x7;		// bsmod, stream = 0
  frame_size = frmsizecod_tbl[frmsizecod].frm_size[fscod] ;
  
  data_out[0] = 0x72; data_out[1] = 0xf8;	/* iec61937 Preamble Pa */
  data_out[2] = 0x1f; data_out[3] = 0x4e;	/*                   Pb */
  data_out[4] = 0x01;			        /* AC3               Pc */
  data_out[5] = bsmod;			        /* bsmod, stream = 0    */
  data_out[6] = (frame_size << 4) & 0xff;       /* frame_size * 16   Pd */
  data_out[7] = ((frame_size ) >> 4) & 0xff;
  swab(data_in, &data_out[8], frame_size * 2 );
  //  fprintf(stderr, "frame_size: %d\n", frame_size);
  frame_bytes = frame_size * 2 + 8;
  memset(&data_out[frame_bytes], 0, 256*6*2*2 - frame_bytes);

  return 0;
}


static int convert_dtsframe_to_iec61937frame(uint16_t *dts,
					     uint16_t *iec61937,
					     int nr_samples)
{
  int i;
  
  uint8_t *data_out = (uint8_t *)iec61937;
  uint8_t *data_in = (uint8_t *)dts;
  int frame_bytes;
  int fsize;
  int burst_len;  
  fsize = (data_in[5] & 0x03) << 12 |
    (data_in[6] << 4) | (data_in[7] >> 4);
  fsize = fsize + 1;
  burst_len = fsize * 8;

  data_out[0] = 0x72; data_out[1] = 0xf8;	/* iec 61937     */
  data_out[2] = 0x1f; data_out[3] = 0x4e;	/*  syncword     */
  switch(nr_samples) {
  case 512:
    data_out[4] = 0x0b;			/* DTS-1 (512-sample bursts) */
    break;
  case 1024:
    data_out[4] = 0x0c;			/* DTS-2 (1024-sample bursts) */
    break;
  case 2048:
    data_out[4] = 0x0d;			/* DTS-3 (2048-sample bursts) */
    break;
  default:
    FATAL("IEC61937-5: %s-sample bursts not supported\n", nr_samples);
    data_out[4] = 0x00;
    break;
  }

  data_out[5] = 0;                      /* ?? */	   
  data_out[6] = (burst_len) & 0xff;   
  data_out[7] = (burst_len >> 8) & 0xff;
  
  if(fsize+8 > nr_samples*2*2) {
    ERROR("IEC61937-5: more data than fits\n", 0);
  }
  //TODO if fzise is odd, swab doesn't copy the last byte
  swab(data_in, &data_out[8], fsize);
  //  fprintf(stderr, "frame_size: %d\n", frame_size);
  memset(&data_out[fsize+8], 0, nr_samples*2*2 - (fsize+8));

  return 0;
}

int convert_samples(adec_handle_t *h, void *samples, int nr_samples)
{
  //convert from samples to output_buf_ptr
  /*
  fprintf(stderr, "samples: %d, output_buf_ptr: %d\n",
	  samples, h->output_buf_ptr);
  */
  switch(conversion_routine) {
  case 0:
    convert_a52_float_to_s16((float *)samples, (int16_t *)h->output_buf_ptr, 
			     nr_samples, 2, NULL);
    h->output_buf_ptr += 2*dst_ch*nr_samples; // 2byte per sample
    break;
  case 1:
    convert_s16be_to_s16ne(samples, (int16_t *)h->output_buf_ptr,
			   nr_samples, 2, NULL);
    h->output_buf_ptr += 2*2*nr_samples;
    break;
  case 2:
    convert_mad_fixed_to_s16((mad_fixed_t *)samples,
			     (int16_t *)h->output_buf_ptr, 
			     nr_samples, 2, NULL);
    h->output_buf_ptr += 2*2*nr_samples; // 2ch 2byte
    break;
  case 3:
    convert_ac3frame_to_iec61937frame(samples,
				      (uint16_t *)h->output_buf_ptr, 
				      nr_samples);
    h->output_buf_ptr += 256*6*2*2; // 2ch 16bit 48kHz (256*6 samples)
    
    break;
  case 4:
    convert_dtsframe_to_iec61937frame(samples,
				      (uint16_t *)h->output_buf_ptr, 
				      nr_samples);
    h->output_buf_ptr += 2*2*nr_samples; // 2ch 16bit 48kHz
    
    break;
  }
   return 0;
}


void convert_samples_start(adec_handle_t *h)
{
  h->output_buf_ptr = h->output_buf;
}




