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

#include "parse_config.h"
#include "audio_config.h"

audio_config_t *audio_config_init(void)
{
  audio_config_t *conf;
  
  conf = malloc(sizeof(audio_config_t));
  
  conf->format.nr_channels = 0;
  conf->format.ch_array = NULL;
  conf->adev_handle = NULL;
  conf->ainfo = NULL;
  return conf;
}

int audio_config(audio_config_t *aconf,
		 int availflags, int sample_rate, int sample_resolution)
{
  int output_channels;

  switch(availflags) {
  default:
    // TODO get the following info from the config file
    output_channels = 2;
    if(aconf->format.nr_channels != output_channels) {
      aconf->format.ch_array = realloc(aconf->format.ch_array,
				sizeof(ChannelType_t)*output_channels);
    }
    aconf->format.sample_resolution = 2;
    aconf->format.nr_channels = output_channels;
    aconf->format.ch_array[0] = ChannelType_Left;
    aconf->format.ch_array[1] = ChannelType_Right;

    break;
  }
  {
    ao_driver_t *drivers;
    
    drivers = ao_drivers();
    fprintf(stderr, "ao_drivers passed\n");
    if(!aconf->adev_handle) {
      if(drivers[0].open != NULL) {
	char *dev_string = get_audio_device();
	fprintf(stderr, "trying audio driver %s\n", drivers[0].name);
	aconf->adev_handle = ogle_ao_open(drivers[0].open, dev_string);
	if(!aconf->adev_handle) {
	  fprintf(stderr, "failed opening the %s audio driver at %s\n",
		  drivers[0].name, dev_string);
	  exit(1);
	}
      } else {
	fprintf(stderr, "ogle_ao_open not taken, no audio ouput driver!\n");
      }
    }
    fprintf(stderr, "ogle_ao_open passed\n");
  }
  if(!aconf->ainfo) {
    aconf->ainfo = malloc(sizeof(ogle_ao_audio_info_t));
  }
  
  aconf->ainfo->sample_rate = sample_rate;
  aconf->ainfo->sample_resolution = sample_resolution;
  //    aconf->aconf.sample_endianess = 0;
  aconf->ainfo->channels = aconf->format.nr_channels;
  aconf->ainfo->encoding = OGLE_AO_ENCODING_LINEAR;
  
  // check return value
  ogle_ao_init(aconf->adev_handle, aconf->ainfo);
  fprintf(stderr, "ogle_ao_init passed\n");
  return 0;
}
