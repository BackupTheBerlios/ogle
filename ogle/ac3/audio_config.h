#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

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

#include <stdlib.h>

#include <libogleao/ogle_ao.h>

typedef enum {
  ChannelType_Left = 1,
  ChannelType_Right = 2,
  ChannelType_Center = 4,
  ChannelType_LeftSurround = 8,
  ChannelType_RightSurround = 16,
  ChannelType_Surround = 32,
  ChannelType_LFE = 64
} ChannelType_t;

typedef struct {
  int nr_channels;
  ChannelType_t *ch_array;
  int interleaved; //the samples for the different channels are interleaved
  int sample_rate;
  int sample_resolution;
} audio_format_t;

typedef struct {
  audio_format_t format;
  ogle_ao_instance_t *adev_handle;
  ogle_ao_audio_info_t *ainfo;
} audio_config_t;


audio_config_t *audio_config_init(void);
int audio_config(audio_config_t *aconf,
		 int availflags, int sample_rate, int sample_resolution);

#endif /* AUDIO_CONFIG_H */
