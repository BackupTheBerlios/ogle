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

#include "common.h"
#include "sync.h"
#include "debug_print.h"

#include "audio_play.h"
#include "decode_private.h" // FIXME

extern char *program_name;
extern ctrl_data_t *ctrl_data;
extern ctrl_time_t *ctrl_time;


int play_samples(adec_handle_t *h, int scr_nr, uint64_t PTS, int pts_valid)
{
  int delay;
  //sync code ...
  if(!ogle_ao_odelay(h->config->adev_handle, &delay)) {
    //odelay failed
  }
  
  if(ctrl_data->speed == 1.0) {
    clocktime_t real_time, scr_time, delay_time;
    clocktime_t t1, t2;
    
    if(ctrl_time[scr_nr].sync_master <= SYNC_AUDIO) {
      
      ctrl_time[scr_nr].sync_master = SYNC_AUDIO;
      
      if(pts_valid) {
	
	clocktime_get(&real_time);
	PTS_TO_CLOCKTIME(scr_time, PTS);
	
	TIME_S(delay_time) = ((int64_t)delay*(int64_t)CT_FRACTION/48000) /
	  CT_FRACTION;
	TIME_SS(delay_time) = ((int64_t)delay*(int64_t)CT_FRACTION/48000) %
	  CT_FRACTION;
	
	calc_realtime_left_to_scrtime(&t1,
				      &real_time,
				      &scr_time,
				      &(ctrl_time[scr_nr].sync_point));
	/*
	  DNOTE("time left %ld.%+010ld\n", TIME_S(t1), TIME_SS(t1));
	  DNOTE("delay %ld.%+010ld\n", TIME_S(delay_time), TIME_SS(delay_time));
	*/
	
	timesub(&t2, &delay_time, &t1);
	if(TIME_S(t2) > 0 || TIME_SS(t2) > 100000000 ||
	   TIME_S(t2) < 0 || TIME_SS(t2) < -10000000) {
	  DNOTE("%ld.%+010ld s off, resyncing\n",
		TIME_S(t2), TIME_SS(t2));
	  
	  timeadd(&delay_time, &delay_time, &real_time);
	  
	  set_sync_point(&ctrl_time[scr_nr],
			 &delay_time,
			 &scr_time,
			 ctrl_data->speed);
	}
      }
    }
    
    ogle_ao_play(h->config->adev_handle, h->output_samples,
		 h->output_samples_size);
  }
  
  return 0;
}
  
#if 0
  if(ctrl_data->speed == 1.0) {
    clocktime_t real_time, scr_time;
    
    clocktime_get(&real_time);
    
    if(PTS_DTS_flags & 0x2) {
      PTS_TO_CLOCKTIME(scr_time, PTS);
    }
    
    if(ctrl_time[scr_nr].sync_master <= SYNC_AUDIO) {
      clocktime_t tmptime;
      ctrl_time[scr_nr].sync_master = SYNC_AUDIO;
      
      if(ctrl_time[scr_nr].offset_valid == OFFSET_NOT_VALID) {
	if(PTS_DTS_flags & 0x2) {
	  int delay = 0;
	  
	  //	  if(!ogle_ao_odelay(ao_handle, &delay)) {
	  //  WARNING("ao_odelay failed\n");
	  // }
	  
	  TIME_S(tmptime) = 0;
	  TIME_SS(tmptime) = delay*1000000/48;
	  
	  timeadd(&tmptime, &tmptime, &real_time);
	    
      	  set_sync_point(&ctrl_time[scr_nr],
			 &tmptime,
			 &scr_time,
			 ctrl_data->speed);
	  
	}
	
      } else {
	/* offset valid */
	if(PTS_DTS_flags & 0x2) {
	  clocktime_t t1, t2;
	  int delay = 0;
	  
	  //	  if(!ogle_ao_odelay(ao_handle, &delay)) {
	  //  WARNING("ao_odelay failed\n");
	  // }

	  TIME_S(tmptime) = 0;
	  TIME_SS(tmptime) = (int64_t)delay*(int64_t)1000000/48;

	  calc_realtime_left_to_scrtime(&t1,
					&real_time,
					&scr_time,
					&(ctrl_time[scr_nr].sync_point));
	  /*
	  DNOTE("time left %ld.%+010ld\n", TIME_S(t1), TIME_SS(t1));
	  DNOTE("delay %ld.%+010ld\n", TIME_S(tmptime), TIME_SS(tmptime));
	  */
	  timesub(&t2, &tmptime, &t1);
	  if(TIME_S(t2) > 0 || TIME_SS(t2) > 70000000 ||
	     TIME_S(t2) < 0 || TIME_SS(t2) < -70000000) {
	    DNOTE("%ld.%+010ld s off, resyncing\n",
		  TIME_S(t2), TIME_SS(t2));

	    timeadd(&tmptime, &tmptime, &real_time);
	    
	    set_sync_point(&ctrl_time[scr_nr],
			   &tmptime,
			   &scr_time,
			   ctrl_data->speed);
	  }
	  
	}
      }

    }
    

  }
#endif

#if 0
  if(pts_valid) {
    if(PTS != scr+delay) {
      
      scr = pts-delay;
    }
    clocktime_t real_time, scr_time, delay_time;
    clocktime_t t1, t2;
    
    clocktime_get(&real_time);
    PTS_TO_CLOCKTIME(scr_time, PTS);
    
    TIME_S(delay_time) = 0;
    TIME_SS(delay_time) = (int64_t)delay*(int64_t)CT_FRACTION/48000;
    
    calc_realtime_left_to_scrtime(&t1,
				  &real_time,
				  &scr_time,
				  &(ctrl_time[scr_nr].sync_point));
    /*
      DNOTE("time left %ld.%+010ld\n", TIME_S(t1), TIME_SS(t1));
      DNOTE("delay %ld.%+010ld\n", TIME_S(delay_time), TIME_SS(delay_time));
    */
    timesub(&t2, &delay_time, &t1);
    if(TIME_S(t2) > 0 || TIME_SS(t2) > 100000000 ||
       TIME_S(t2) < 0 || TIME_SS(t2) < -10000000) {
      DNOTE("%ld.%+010ld s off, resyncing\n",
	    TIME_S(t2), TIME_SS(t2));
      
      timeadd(&delay_time, &delay_time, &real_time);
      
      set_sync_point(&ctrl_time[scr_nr],
		     &delay_time,
		     &scr_time,
		     ctrl_data->speed);
    }
    
    
  }
#endif
  

