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
#include <string.h>

#include "parse_config.h"
#include "audio_config.h"

#include "sync.h"
#include "debug_print.h"

audio_config_t *audio_config_init(void)
{
  audio_config_t *conf;
  char *type;
  int ms_offset;
  
  conf = malloc(sizeof(audio_config_t));
  
  conf->format.nr_channels = 0;
  conf->format.ch_array = NULL;
  conf->adev_handle = NULL;
  conf->ainfo = NULL;
  
  conf->sync.delay_resolution = 0;
  conf->sync.delay_resolution_set = 0;
  conf->sync.max_sync_diff = 0;
  conf->sync.prev_delay = -1;
  conf->sync.samples_added = 0;

  type = get_sync_type();
  if(!strcmp(type, "clock")) {
    conf->sync.type = SyncType_clock;
  } else {
    conf->sync.type = SyncType_odelay;
  }
  conf->sync.resample = get_sync_resample();
  ms_offset = get_sync_offset();
  if(ms_offset > 999 || ms_offset < -999) {
    ms_offset = 0;
  }
  TIME_S(conf->sync.offset) = 0;
  TIME_SS(conf->sync.offset) = ms_offset * CT_FRACTION/1000;

  return conf;
}

int audio_config(audio_config_t *aconf,
		 int availflags, int sample_rate, int sample_resolution)
{
  int output_channels;
  int frag_size = 4096;

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
    if(!aconf->adev_handle) {
      int n;
      int driver_n = 0;
      char *driver_str = get_audio_driver();
      
      if(driver_str != NULL) {
	for(n = 0; drivers[n].name != NULL; n++) {
	  if(!strcmp(drivers[n].name, driver_str)) {
	    driver_n = n;
	    break;
	  }
	}
	if(drivers[n].name == NULL) {
	  NOTE("Audio driver '%s' not found\n", driver_str);
	}
      }

      NOTE("Using audio driver '%s'\n", drivers[driver_n].name);
	     
      if(drivers[driver_n].open != NULL) {
	char *dev_string;
	if(!strcmp("alsa", drivers[driver_n].name)) {
	  dev_string = get_audio_alsa_name();
	} else {
	  dev_string = get_audio_device();
	}
	aconf->adev_handle = ogle_ao_open(drivers[driver_n].open, dev_string);
	if(!aconf->adev_handle) {
	  FATAL("failed opening the %s audio driver at %s\n",
		drivers[driver_n].name, dev_string);
	  exit(1);
	}
      } else {
	FATAL("ogle_ao_open not taken, no audio ouput driver!\n");
	exit(1);
      }
    }
    // DNOTE("ogle_ao_open passed\n");
  }
  if(!aconf->ainfo) {
    aconf->ainfo = malloc(sizeof(ogle_ao_audio_info_t));
  }
  
  aconf->ainfo->sample_rate = sample_rate;
  aconf->ainfo->sample_resolution = sample_resolution;
  aconf->ainfo->byteorder = OGLE_AO_BYTEORDER_NE;
  aconf->ainfo->channels = aconf->format.nr_channels;
  aconf->ainfo->encoding = OGLE_AO_ENCODING_LINEAR;
  aconf->ainfo->fragment_size = frag_size;
  aconf->ainfo->fragments = -1;  // don't limit

  // check return value
  if(ogle_ao_init(aconf->adev_handle, aconf->ainfo) == -1) {

    FATAL("ogle_ao_init: ");
    if(aconf->ainfo->sample_rate == -1) {
      FATAL("sample_rate %d failed", sample_rate);
    } else if(aconf->ainfo->sample_resolution == -1 ||
	      aconf->ainfo->encoding == -1 ||
	      aconf->ainfo->byteorder == -1) {
      FATAL("encoding %d, resolution %d, byte order %d failed\n",
	    OGLE_AO_ENCODING_LINEAR, sample_resolution, OGLE_AO_BYTEORDER_NE);
    } else if(aconf->ainfo->channels == -1) {
      FATAL("channels %d failed\n", aconf->format.nr_channels);
    } 
  } else {
    if(aconf->ainfo->sample_rate != sample_rate) {
      ERROR("wanted sample rate %d, got %d\n",
	    sample_rate,
	    aconf->ainfo->sample_rate);
    }
    if(aconf->ainfo->sample_resolution != sample_resolution) {
      ERROR("wanted sample resolution %d, got %d\n",
	    sample_resolution,
	    aconf->ainfo->sample_resolution);	   
    }
    if(aconf->ainfo->byteorder != OGLE_AO_BYTEORDER_NE) {
      ERROR("wanted byte order %d, got %d\n",
	    OGLE_AO_BYTEORDER_NE, aconf->ainfo->byteorder);
    }
    if(aconf->ainfo->channels != aconf->format.nr_channels) {
      ERROR("wanted channels %d, got %d\n",
	    aconf->format.nr_channels,
	    aconf->ainfo->channels); 
    }
    if(aconf->ainfo->encoding != OGLE_AO_ENCODING_LINEAR) {
      ERROR("wanted encoding %d, got %d\n",
	    OGLE_AO_ENCODING_LINEAR,
	    aconf->ainfo->encoding);
    }
    if(aconf->ainfo->fragment_size != frag_size) {
      NOTE("wanted fragment size %d, got %d\n",
	   frag_size,
	   aconf->ainfo->fragment_size);
    }

    DNOTE("encoding %d, resolution %d, byte order %d, rate %d\n",
	  aconf->ainfo->encoding,
	  aconf->ainfo->sample_resolution,
	  aconf->ainfo->byteorder,
	  aconf->ainfo->sample_rate);
    DNOTE("fragments %d, size %d\n",
	  aconf->ainfo->fragments,
	  aconf->ainfo->fragment_size);
  }
  
  aconf->sync.delay_resolution = aconf->ainfo->sample_rate / 10;
  aconf->sync.delay_resolution_set = 0;
  aconf->sync.max_sync_diff = 2 * aconf->sync.delay_resolution *
    (CT_FRACTION / aconf->ainfo->sample_rate);
  aconf->sync.prev_delay = -1;
  aconf->sync.samples_added = 0;

  return 0;
}
