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

extern ctrl_data_t *ctrl_data;
extern ctrl_time_t *ctrl_time;


int play_samples(adec_handle_t *h, int scr_nr, uint64_t PTS, int pts_valid)
{
  static clocktime_t last_sync = { 0, 0 };
  static int samples_written;
  static int old_delay = 0;

  int bytes_to_write;
  int delay;
  int srate;
  static int frqlck_cnt = 0;
  static int frqlck = 0;
  static double frq_correction = 1.0;
  audio_sync_t *s = &(h->config->sync);

  //sync code ...
  if(!ogle_ao_odelay(h->config->adev_handle, &delay)) {
    //odelay failed
  }
  //fprintf(stderr, "(%d)", delay);

  if(!s->delay_resolution_set) {
    int delay_diff;
    int delay_step;
    
    delay_diff = delay - s->prev_delay;

    if(s->prev_delay != -1) {
      delay_step = s->samples_added - delay_diff;
      
      if((delay_step > 0) && (delay_step < s->delay_resolution)) {
	if(delay_step < h->config->ainfo->sample_rate/200) {
	  s->delay_resolution = h->config->ainfo->sample_rate/200;
	  s->max_sync_diff = 2 * s->delay_resolution *
	    (CT_FRACTION / h->config->ainfo->sample_rate);
	  fprintf(stderr, "delay resolution better than: 0.%09d, %d samples, rate: %d\n",
		  s->max_sync_diff/2, s->delay_resolution,
		  h->config->ainfo->sample_rate);
	  s->delay_resolution_set = 1;
	} else {
	  s->delay_resolution = delay_step;
	  s->max_sync_diff = 2 * s->delay_resolution *
	    (CT_FRACTION / h->config->ainfo->sample_rate);
	  
	  fprintf(stderr, "delay resolution: 0.%09d, %d samples, rate: %d\n",
		  s->max_sync_diff/2, s->delay_resolution,
		  h->config->ainfo->sample_rate);
	}
      }
    }
    s->prev_delay = delay;
  }
  
  if(ctrl_data->speed == 1.0) {
    clocktime_t real_time, scr_time, delay_time;
    clocktime_t t1, t2;
    clocktime_t drift, delta_sync_time;
    clocktime_t sleep_time = { 0, 0 };
    
    if(ctrl_time[scr_nr].sync_master <= SYNC_AUDIO) {
      
      ctrl_time[scr_nr].sync_master = SYNC_AUDIO;
      
      if(pts_valid) {
	int delta_samples;
	static double prev_df = 1.0;
	double drift_factor, diff;
	const clocktime_t buf_time = { 0, CT_FRACTION/5 };

	srate = h->config->ainfo->sample_rate * frq_correction;
        //fprintf(stderr, "(%d)", srate);

        TIME_S(delay_time) =
	  ((int64_t)delay * (int64_t)CT_FRACTION/srate) /
          CT_FRACTION;
	
	TIME_SS(delay_time) = 
	  ((int64_t)delay*(int64_t)CT_FRACTION/srate) %
          CT_FRACTION;
	
	if(TIME_S(delay_time) > 0 || TIME_SS(delay_time) > TIME_SS(buf_time)) {
	  timesub(&sleep_time, &delay_time, &buf_time); 
	}

	clocktime_get(&real_time);
	PTS_TO_CLOCKTIME(scr_time, PTS);

	calc_realtime_left_to_scrtime(&t1,
				      &real_time,
				      &scr_time,
				      &(ctrl_time[scr_nr].sync_point));
	/*
	  DNOTE("time left %ld.%+010ld\n", TIME_S(t1), TIME_SS(t1));
	  DNOTE("delay %ld.%+010ld\n", TIME_S(delay_time), TIME_SS(delay_time));
	*/
	
	timesub(&t2, &delay_time, &t1);
	if(TIME_S(t2) > 0 || TIME_SS(t2) > s->max_sync_diff ||
	   TIME_S(t2) < 0 || TIME_SS(t2) < -s->max_sync_diff) {
 
	  if(TIME_S(t2) > 0 || TIME_SS(t2) > 2*s->max_sync_diff ||
	     TIME_S(t2) < 0 || TIME_SS(t2) < -2*s->max_sync_diff) {
	    DNOTE("%ld.%+010ld s off, resyncing\n",
		  TIME_S(t2), TIME_SS(t2));
	  } else { 
            //fprintf(stderr, "(%d)", delay);
	    if(TIME_S(t2) == 0 || TIME_SS(t2) > 0) {
	      fprintf(stderr, "+");
	    } else {
	      fprintf(stderr, "-");
	    }
	    /*
	      timesub(&drift, &real_time, &last_sync);
	      {
	      fprintf(stderr, "[%+07ld / %+07ld]",
	      TIME_SS(t2), TIME_SS(drift));
	      
	      }
 	    */
	  }
	  timeadd(&delay_time, &delay_time, &real_time);
	  
	  set_sync_point(&ctrl_time[scr_nr],
			 &delay_time,
			 &scr_time,
			 ctrl_data->speed);
          
	}

	timesub(&delta_sync_time, &real_time, &last_sync);

	
	if(TIME_S(delta_sync_time) > 9) {
	  
          delta_samples = samples_written - (delay - old_delay);
	  
          samples_written = 0;
          last_sync = real_time;
          old_delay = delay;
          drift_factor = (double)delta_samples /
            (((double)(TIME_S(delta_sync_time)) +
              (double)(TIME_SS(delta_sync_time)) / CT_FRACTION) *
             srate);
	  fprintf(stderr, "<%ld.%+010ld>",
		  TIME_S(delta_sync_time),
		  TIME_SS(delta_sync_time));
          fprintf(stderr, "[%.6f]", drift_factor);
          
          diff = drift_factor/prev_df;
          if(diff > 0.998 && diff < 1.001) {
            frqlck_cnt++;
          } else {
            frqlck_cnt = 0;
          }
         
          {
            static int avg_cnt = 0;
            static double avg_drift = 0.0;
            
            if(frqlck_cnt > 3) {
              
              if(avg_cnt < 8) {
                avg_cnt++;
                avg_drift += drift_factor;
                
              } else if(avg_cnt == 8) {
                avg_drift /= avg_cnt;
                avg_cnt++;

                diff = frq_correction / avg_drift;
		fprintf(stderr, "diff: %.6f\n", diff);
                if(diff < 0.999 || diff > 1.001) {
                  frq_correction = avg_drift;
                  fprintf(stderr, "frq_corr: %.6f\n", frq_correction);
		  fprintf(stderr, "srate: %d\n", 
			  (int)(h->config->ainfo->sample_rate
				* frq_correction));
		  
                }
              }
            } else {
              avg_drift = 0.0;
              avg_cnt = 0;
            }
            
          }

          prev_df = drift_factor;


	}
      }
    }

    bytes_to_write = h->output_buf_ptr - h->output_buf;

    if(TIME_S(sleep_time) > 0 || TIME_SS(sleep_time) > 0) {
      //sleep so we don't buffer more than buf_time of output
      // needed mostly on solaris which can buffer huge amounts
      /*
      fprintf(stderr, "sleep: %ld.%09ld\n", 
	      TIME_S(sleep_time),
	      TIME_SS(sleep_time));
      */
      nanosleep(&sleep_time, NULL);
    }

    ogle_ao_play(h->config->adev_handle, h->output_buf,
		 bytes_to_write);

    s->samples_added = bytes_to_write / h->config->ainfo->sample_frame_size;
    samples_written += s->samples_added;
   
  }
  




  
  return 0;
}
  
