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
#include "../include/common.h"
#include "video_types.h"

extern buf_ctrl_head_t *buf_ctrl_head;
extern void display(yuv_image_t *current_image);
extern void display_init(int padded_width, int padded_height,
			 int horizontal_size, int vertical_size);
extern void exit_program(int);


int get_next_picture_buf_id()
{
  int id;
  static int dpy_q_get_pos = 0;
  
  if(sem_wait(&(buf_ctrl_head->pictures_ready_to_display)) == -1) {
    perror("sem_wait get_next_picture_buf_id");
  }

  id = buf_ctrl_head->dpy_q[dpy_q_get_pos];
  dpy_q_get_pos = (dpy_q_get_pos + 1) % (buf_ctrl_head->nr_of_buffers);
  
  return id;
}

void release_picture_buf(int id)
{
  sem_post(&(buf_ctrl_head->pictures_displayed));
  return;
}




static int timecompare(struct timespec *s1, struct timespec *s2) {

  if(s1->tv_sec > s2->tv_sec) {
    return 1;
  } else if(s1->tv_sec < s2->tv_sec) {
    return -1;
  }

  if(s1->tv_nsec > s2->tv_nsec) {
    return 1;
  } else if(s1->tv_nsec < s2->tv_nsec) {
    return -1;
  }
  
  return 0;
}

static void timesub(struct timespec *d,
	     struct timespec *s1, struct timespec *s2)
{
  // d = s1-s2

  d->tv_sec = s1->tv_sec - s2->tv_sec;
  d->tv_nsec = s1->tv_nsec - s2->tv_nsec;
  
  if(d->tv_nsec >= 1000000000) {
    d->tv_sec += 1;
    d->tv_nsec -= 1000000000;
  } else if(d->tv_nsec <= -1000000000) {
    d->tv_sec -= 1;
    d->tv_nsec += 1000000000;
  }

  if((d->tv_sec > 0) && (d->tv_nsec < 0)) {
    d->tv_sec -= 1;
    d->tv_nsec += 1000000000;
  } else if((d->tv_sec < 0) && (d->tv_nsec > 0)) {
    d->tv_sec += 1;
    d->tv_nsec -= 1000000000;
  }

}  


static void timeadd(struct timespec *d,
	     struct timespec *s1, struct timespec *s2)
{
  // d = s1+s2
  
  d->tv_sec = s1->tv_sec + s2->tv_sec;
  d->tv_nsec = s1->tv_nsec + s2->tv_nsec;
  if(d->tv_nsec >= 1000000000) {
    d->tv_nsec -=1000000000;
    d->tv_sec +=1;
  } else if(d->tv_nsec <= -1000000000) {
    d->tv_nsec +=1000000000;
    d->tv_sec -=1;
  }

  if((d->tv_sec > 0) && (d->tv_nsec < 0)) {
    d->tv_sec -= 1;
    d->tv_nsec += 1000000000;
  } else if((d->tv_sec < 0) && (d->tv_nsec > 0)) {
    d->tv_sec += 1;
    d->tv_nsec -= 1000000000;
  }
}  




void display_process()
{
  struct timespec real_time, prefered_time, wait_time, frame_interval;
  struct timespec real_time2, diff, avg_time, oavg_time;
  struct timespec calc_rt, err_time;
  int buf_id;
  int drop = 0;
  int frame_nr = 0;
  int avg_nr = 23;
  int first = 1;
  frame_interval.tv_sec = 0;
  //frame_interval.tv_nsec = 28571428; //35 fps
  //frame_interval.tv_nsec = 33366700; // 29.97 fps
  frame_interval.tv_nsec = 41666666; //24 fps
  prefered_time.tv_sec = 0;

  while(1) {
    buf_id = get_next_picture_buf_id();
    //fprintf(stderr, "display: start displaying buf: %d\n", buf_id);
    frame_interval.tv_nsec = buf_ctrl_head->frame_interval;
    if(first) {
      first = 0;
      display_init(buf_ctrl_head->picture_infos[buf_id].picture.padded_width,
		   buf_ctrl_head->picture_infos[buf_id].picture.padded_height,
		   buf_ctrl_head->picture_infos[buf_id].picture.horizontal_size,
		   buf_ctrl_head->picture_infos[buf_id].picture.vertical_size);
      display(&(buf_ctrl_head->picture_infos[buf_id].picture));
    }
    clock_gettime(CLOCK_REALTIME, &real_time);
    if((prefered_time.tv_sec == 0) || (frame_interval.tv_nsec == 1)) {
      prefered_time = real_time;
    } else if(buf_ctrl_head->picture_infos[buf_id].pts_time.tv_sec != -1) {
      struct timespec buftime;
      buftime.tv_sec = 0;
      buftime.tv_nsec = 00000000;
      /*
      fprintf(stderr, "realtime_offset: %ld.%09ld\n",
	      buf_ctrl_head->picture_infos[buf_id].realtime_offset.tv_sec,
	      buf_ctrl_head->picture_infos[buf_id].realtime_offset.tv_nsec);
      fprintf(stderr, "pts: %lld\n",
	      buf_ctrl_head->picture_infos[buf_id].PTS);
      */
      
      timeadd(&calc_rt,
	      &(buf_ctrl_head->picture_infos[buf_id].pts_time),
	      &(buf_ctrl_head->picture_infos[buf_id].realtime_offset));
      timeadd(&calc_rt, &calc_rt, &buftime);
      timesub(&err_time, &calc_rt, &real_time);
      prefered_time = calc_rt;
      //      fprintf(stderr, "%ld.%+010ld\n", err_time.tv_sec, err_time.tv_nsec);
    }
    timesub(&wait_time, &prefered_time, &real_time);
    /*
    fprintf(stderr, "pref: %d.%09ld\n",
	    prefered_time.tv_sec, prefered_time.tv_nsec);

    fprintf(stderr, "real: %d.%09ld\n",
	    real_time.tv_sec, real_time.tv_nsec);

    fprintf(stderr, "wait: %d.%09ld, ",
	    wait_time.tv_sec, wait_time.tv_nsec);
    */     

    if((wait_time.tv_nsec > 5000000) && (wait_time.tv_sec >= 0)) {
      nanosleep(&wait_time, NULL);
    } else {
      //fprintf(stderr, "---less than 0.005 s\n");
    }

    frame_nr++;
    avg_nr++;
    if(avg_nr == 24) {
      avg_nr = 0;
      oavg_time = avg_time;
      clock_gettime(CLOCK_REALTIME, &avg_time);
      
      fprintf(stderr, "display: frame rate: %.3f fps\n",
	      24.0/(((double)avg_time.tv_sec+
		     (double)(avg_time.tv_nsec)/1000000000.0)-
		    ((double)oavg_time.tv_sec+
		     (double)(oavg_time.tv_nsec)/1000000000.0))
	      );
    }

    clock_gettime(CLOCK_REALTIME, &real_time2);
    timesub(&diff, &prefered_time, &real_time2);
    /*
    fprintf(stderr, "diff: %d.%09ld\n",
	    diff.tv_sec, diff.tv_nsec);
    */
    /*
    fprintf(stderr, "rt: %d.%09ld, pt: %d.%09ld, diff: %d.%+09ld\n",
	    real_time2.tv_sec, real_time2.tv_nsec,
	    prefered_time.tv_sec, prefered_time.tv_nsec,
	    diff.tv_sec, diff.tv_nsec);
    */
    /*
    if(drop) {
      if((wait_time.tv_nsec > -10000000) && (wait_time.tv_sec > -1)) {

	//	fprintf(stderr, "$ %ld.%+09ld \n",
	//	wait_time.tv_sec,
	//	wait_time.tv_nsec);
	drop = 0;
      } else {
	//fprintf(stderr, "# %ld.%+09ld \n",
	//	wait_time.tv_sec,
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
    if((wait_time.tv_nsec < 0) || (wait_time.tv_sec < 0)) {
      drop = 1;
    }
    */
    //    fprintf(stderr, "display: done with buf %d\n", buf_id);
    release_picture_buf(buf_id);
  }
}
