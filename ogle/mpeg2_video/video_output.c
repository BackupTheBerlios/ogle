/* SKROMPF - A video player
 * Copyright (C) 2000 Björn Englund, Håkan Hjort
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
#include <stdlib.h>

#include <glib.h>
#include <signal.h>

#include "common.h"
#include "timemath.h"
#include "video_types.h"
#include "ip_sem.h"

#include <time.h>
#ifndef HAVE_CLOCK_GETTIME
#include <sys/time.h>
#endif


extern buf_ctrl_head_t *buf_ctrl_head;
extern void display(yuv_image_t *current_image);
extern void display_init(int padded_width, int padded_height,
			 int horizontal_size, int vertical_size,
			 buf_ctrl_head_t *buf_ctrl_head);

extern void display_exit(void);

int get_next_picture_buf_id()
{
  int id;
  static int dpy_q_get_pos = 0; // Fix me!!

  if(ip_sem_wait(&buf_ctrl_head->queue, PICTURES_READY_TO_DISPLAY) == -1) {
    perror("sem_wait() get_next_picture_buf_id");
    exit(1); // XXX 
  }
  id = buf_ctrl_head->dpy_q[dpy_q_get_pos];
  dpy_q_get_pos = (dpy_q_get_pos + 1) % (buf_ctrl_head->nr_of_buffers);

  return id;
}

void release_picture_buf(int id)
{
  if(ip_sem_post(&buf_ctrl_head->queue, PICTURES_DISPLAYED) == -1) {
    perror("sem_post() pictures_displayed");
    exit(1); // XXX 
  }
}


/* Erhum test... */
clocktime_t first_time;
int frame_nr = 0;

void int_handler()
{
  display_exit();
}


void display_process()
{
  clocktime_t real_time, prefered_time, frame_interval;
  clocktime_t real_time2, diff, avg_time, oavg_time;
  clocktime_t calc_rt;
  struct timespec wait_time;
  struct sigaction sig;
  int buf_id;
  int drop = 0;
  int avg_nr = 23;
  int first = 1;
  TIME_S(frame_interval) = 0;
  //TIME_(frame_interval) = 28571428; //35 fps
  //TIME_(frame_interval) = 33366700; // 29.97 fps
  TIME_SS(frame_interval) = 41666666; //24 fps
  TIME_S(prefered_time) = 0;

  sig.sa_handler = int_handler;
  sigaction(SIGINT, &sig, NULL);

  while(1) {
    buf_id = get_next_picture_buf_id();
    //fprintf(stderr, "display: start displaying buf: %d\n", buf_id);
    TIME_SS(frame_interval) = buf_ctrl_head->frame_interval;
    if(first) {
      first = 0;
      display_init(buf_ctrl_head->picture_infos[buf_id].picture.padded_width,
		   buf_ctrl_head->picture_infos[buf_id].picture.padded_height,
		   buf_ctrl_head->picture_infos[buf_id].picture.horizontal_size,
		   buf_ctrl_head->picture_infos[buf_id].picture.vertical_size,
		   buf_ctrl_head);
      //display(&(buf_ctrl_head->picture_infos[buf_id].picture));
      /* Erhum test... */
      clocktime_get(&first_time);      
    }
    clocktime_get(&real_time);
    if(TIME_S(prefered_time) == 0 || TIME_SS(frame_interval) == 1) {
      prefered_time = real_time;
    } else if(TIME_S(buf_ctrl_head->picture_infos[buf_id].pts_time) != -1) {
      clocktime_t buftime;
      TIME_S(buftime) = 0;
      TIME_SS(buftime) = 00000000;
      /*
      fprintf(stderr, "realtime_offset: %ld.%09ld\n",
	      TIME_S(buf_ctrl_head->picture_infos[buf_id].realtime_offset),
	      TIME_SS(buf_ctrl_head->picture_infos[buf_id].realtime_offset));
      fprintf(stderr, "pts: %lld\n",
	      buf_ctrl_head->picture_infos[buf_id].PTS);
      */
      
      timeadd(&calc_rt,
	      &(buf_ctrl_head->picture_infos[buf_id].pts_time),
	      &(buf_ctrl_head->picture_infos[buf_id].realtime_offset));
      timeadd(&calc_rt, &calc_rt, &buftime);
      prefered_time = calc_rt;
    }
    // Fixme!!! wait_time can't be used here..
    timesub(&wait_time, &prefered_time, &real_time);
    /*
    fprintf(stderr, "pref: %d.%09ld\n",
	    TIME_S(prefered_time), TIME_SS(prefered_time));

    fprintf(stderr, "real: %d.%09ld\n",
	    TIME_S(real_time), TIME_SS(real_time));

    fprintf(stderr, "wait: %d.%09ld, ",
	    TIME_S(wait_time), TIME_SS(wait_time));
    */     

    if(wait_time.tv_nsec > 5000000 && wait_time.tv_sec >= 0) {
      nanosleep(&wait_time, NULL);
    } else {
      //fprintf(stderr, "---less than 0.005 s\n");
    }

    frame_nr++;
    avg_nr++;
    if(avg_nr == 24) {
      avg_nr = 0;
      oavg_time = avg_time;
      clocktime_get(&avg_time);
      
      fprintf(stderr, "display: frame rate: %.3f fps\n",
	      24.0/(((double)TIME_S(avg_time)+
		     (double)TIME_SS(avg_time)/1000000000.0)-
		    ((double)TIME_S(oavg_time)+
		     (double)TIME_SS(oavg_time)/1000000000.0))
	      );
    }

    clocktime_get(&real_time2);
    timesub(&diff, &prefered_time, &real_time2);
    /*
    fprintf(stderr, "diff: %d.%09ld\n",
	    TIME_S(diff), TIME_SS(diff));
    */
    /*
    fprintf(stderr, "rt: %d.%09ld, pt: %d.%09ld, diff: %d.%+09ld\n",
	    TIME_S(real_time2), TIME_SS(real_time2),
	    TIME_S(prefered_time), TIME_SS(prefered_time),
	    TIME_S(diff), TIME_SS(diff));
    */
    /*
    if(drop) {
      if(wait_time.tv_nsec > -10000000 && wait_time.tv_sec > -1) {

	//	fprintf(stderr, "$ %ld.%+09ld \n",
	//	wait_time.tv_sec,
	//	wait_time.tv_nsec);
	drop = 0;
      } else {
	//fprintf(stderr, "# %ld.%+09ld \n",
	//	wait_timetv_sec,
	//	wait_time.tv_nsec);
      }
    }
    */
#if 0
    if(!drop) {
      display(&(buf_ctrl_head->picture_infos[buf_id].picture));
    } else {
      fprintf(stderr, "*");
      drop = 0;
    }
#else
      display(&(buf_ctrl_head->picture_infos[buf_id].picture));
#endif

    timeadd(&prefered_time, &prefered_time, &frame_interval);
    /*
    if(wait_time.tv_nsec < 0 || wait_time.tv_sec < 0) {
      drop = 1;
    }
    */
    //    fprintf(stderr, "display: done with buf %d\n", buf_id);
    release_picture_buf(buf_id);
  }
}

void display_process_exit(void) {
  clocktime_t now_time;
  
  clocktime_get(&now_time);
      
  fprintf(stderr, "display: Total frame rate: %.3f fps "
	          "(%i frames in %.3f seconds)\n ",
	  (double)frame_nr/(((double)TIME_S(now_time)+
			     (double)TIME_SS(now_time)/1000000000.0)-
			    ((double)TIME_S(first_time)+
			     (double)TIME_SS(first_time)/1000000000.0)),
	  frame_nr, (((double)TIME_S(now_time)+
			     (double)TIME_SS(now_time)/1000000000.0)-
			    ((double)TIME_S(first_time)+
			     (double)TIME_SS(first_time)/1000000000.0))
	  );
  exit(0);
}
