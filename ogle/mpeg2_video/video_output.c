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

//#include <glib.h>
#include <signal.h>
#include <sys/shm.h>
#include <time.h>
#include <errno.h>

#ifndef HAVE_CLOCK_GETTIME
#include <sys/time.h>
#endif

#include "common.h"
#include "timemath.h"
#include "video_types.h"
#include "msgevents.h"
#include "sync.h"

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

extern void display(yuv_image_t *current_image);
extern void display_init(int padded_width, int padded_height,
			 int horizontal_size, int vertical_size,
			 picture_data_elem_t *pinfo,
			 yuv_image_t *picture_data,
			 data_buf_head_t *picture_data_head,
			 char *picture_buf_base);

extern void display_exit(void);


int register_event_handler(int(*eh)(MsgEventQ_t *, MsgEvent_t *));
int event_handler(MsgEventQ_t *q, MsgEvent_t *ev);

int debug;
int video_scr_nr;

static char *program_name;

static int ctrl_data_shmid;
static ctrl_data_t *ctrl_data;
ctrl_time_t *ctrl_time;

static int data_buf_shmid;
static char *data_buf_shmaddr;

int msgqid = -1;
MsgEventQ_t *msgq;


static picture_data_elem_t *picture_ctrl_data;
static q_head_t *picture_q_head;
static q_elem_t *picture_q_elems;

static char *picture_buf_base;
static data_buf_head_t *picture_ctrl_head;

yuv_image_t *image_bufs;




typedef struct _event_handler_t {
  int(*eh)(MsgEventQ_t *, MsgEvent_t *);
  struct _event_handler_t *next;
} event_handler_t;

static event_handler_t *eh_head = NULL;


int register_event_handler(int(*eh)(MsgEventQ_t *, MsgEvent_t *))
{
  event_handler_t *eh_ptr;
  
  eh_ptr = malloc(sizeof(event_handler_t));
  eh_ptr->next = eh_head;
  eh_ptr->eh = eh;
  eh_head = eh_ptr;


  return 0;
}

int event_handler(MsgEventQ_t *q, MsgEvent_t *ev)
{
  event_handler_t *eh_ptr;
  
  eh_ptr = eh_head;

  while(eh_ptr != NULL) {
    if(eh_ptr->eh(q, ev)) {
      return 1;
    }
    eh_ptr = eh_ptr->next;
  }
  fprintf(stderr, "vo: event_handler: unhandled event: %d\n", ev->type);
  return 0;
}

static int attach_ctrl_shm(int shmid)
{
  char *shmaddr;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("vo: attach_ctrl_data(), shmat()");
      return -1;
    }
    
    ctrl_data_shmid = shmid;
    ctrl_data = (ctrl_data_t*)shmaddr;
    ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  }    
  
  return 0;
}


static int attach_picture_buffer(int shmid)
{
  int n;
  char *shmaddr;
  //  q_head_t *q_head;
  char *q_shmaddr;
  picture_data_elem_t *data_elems;

  fprintf(stderr, "vo: q_shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("video_ouput: attach_decoder_buffer(), shmat()");
      return -1;
    }
    
    //q_shmid = shmid;
    q_shmaddr = shmaddr;
  }    

  picture_q_head = (q_head_t *)q_shmaddr;
  picture_q_elems = (q_elem_t *)(q_shmaddr+sizeof(q_head_t));

  shmid = picture_q_head->data_buf_shmid;
  
  fprintf(stderr, "vo: picture_buf_shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("vo: attach_data_buffer(), shmat()");
      return -1;
    }
    
    //picture_buf_shmid = shmid;
    picture_buf_base = shmaddr;
  
    
    picture_ctrl_head = (data_buf_head_t *)picture_buf_base;

    data_elems =(picture_data_elem_t *)(picture_buf_base + 
					sizeof(data_buf_head_t));

    picture_ctrl_data = data_elems;

    image_bufs = 
      malloc(picture_ctrl_head->nr_of_dataelems * sizeof(yuv_image_t));
    

    
    for(n = 0; n < picture_ctrl_head->nr_of_dataelems; n++) {
      image_bufs[n].y = picture_buf_base + data_elems[n].picture.y_offset;
      image_bufs[n].u = picture_buf_base + data_elems[n].picture.u_offset;
      image_bufs[n].v = picture_buf_base + data_elems[n].picture.v_offset;
      image_bufs[n].info = &data_elems[n].picture;
    }
    
  }    

  return 0;
}


static int handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  
  switch(ev->type) {
  case MsgEventQNotify:
    if((picture_q_head != NULL) &&
       (ev->notify.qid == picture_q_head->qid)) {
      //DPRINTF(1, "vo: got notify\n");
    } else {
      return 0;
    }
    break;
  case MsgEventQAttachQ:
    attach_picture_buffer(ev->attachq.q_shmid);
    break;
  case MsgEventQCtrlData:
    attach_ctrl_shm(ev->ctrldata.shmid);
    break;
  default:
    //fprintf(stderr, "vo: unrecognized event type: %d\n", ev->type);
    return 0;
    break;
  }
  return 1;
}


static yuv_image_t *last_image_buf = NULL;
static int redraw_needed = 0;

void redraw_request(void)
{
  redraw_needed = 1;
}

void redraw_done(void)
{
  redraw_needed = 0;
}

static void redraw_screen(void)
{
  if(last_image_buf != NULL) {
    display(last_image_buf);    
  }
  redraw_done();
}

static int get_next_picture_buf_id()
{
  int elem;
  MsgEvent_t ev;
  
  elem = picture_q_head->read_nr;
  //fprintf(stderr, "vo: waiting for picture qelem: #%d\n", elem);
  
  if(!picture_q_elems[elem].in_use) {
    picture_q_head->reader_requests_notification = 1;
    //fprintf(stderr, "vo: elem in use, setting notification req\n");

    while(!picture_q_elems[elem].in_use) {
      //fprintf(stderr, "vo: waiting for notification\n");
      MsgNextEvent(msgq, &ev);
      event_handler(msgq, &ev);
      if(redraw_needed) {
	redraw_screen();
      }
    }
  }
  /*
    fprintf(stderr, "vo: got qelem #%d, data_buf: #%d\n",
	    elem,
	    picture_q_elems[elem].data_elem_index);
  */
  return picture_q_elems[elem].data_elem_index;
}

static void release_picture_buf(int id)
{
  MsgEvent_t ev;
  
  
  picture_ctrl_data[id].displayed = 1;
  picture_q_elems[picture_q_head->read_nr].in_use = 0;
  /*
  fprintf(stderr, "vo: release databuf: #%d,  qelem: #%d\n",
	  id, picture_q_head->read_nr);
  */
  if(picture_q_head->writer_requests_notification) {
    picture_q_head->writer_requests_notification = 0;
    ev.type = MsgEventQNotify;
    //fprintf(stderr, "vo: writer wants notify, sending...\n");
    if(MsgSendEvent(msgq, picture_q_head->writer, &ev) == -1) {
      fprintf(stderr, "vo: couldn't send notification\n");
    }
  }

  picture_q_head->read_nr =
    (picture_q_head->read_nr+1)%picture_q_head->nr_of_qelems;

}


/* Erhum test... */
clocktime_t first_time;
int frame_nr = 0;

void int_handler()
{
  display_exit();
}

//#define LINUX
static void display_process()
{
  clocktime_t real_time, prefered_time, frame_interval;
  clocktime_t real_time2, diff, avg_time, oavg_time;
  clocktime_t calc_rt;
#ifdef LINUX
  clocktime_t waittmp;
#endif
  struct timespec wait_time;
  struct sigaction sig;
  int buf_id, prev_buf_id;
  int drop = 0;
  int avg_nr = 23;
  int first = 1;
  picture_data_elem_t *pinfos;
  static int prev_scr_nr = 0;
  static clocktime_t time_offset = { 0, 0 };

  TIME_S(frame_interval) = 0;
  //TIME_(frame_interval) = 28571428; //35 fps
  //TIME_(frame_interval) = 33366700; // 29.97 fps
  TIME_SS(frame_interval) = 41666666; //24 fps
  TIME_S(prefered_time) = 0;
  
  pinfos = picture_ctrl_data;
  
  sig.sa_handler = int_handler;
  sigaction(SIGINT, &sig, NULL);

  while(1) {

    buf_id = get_next_picture_buf_id();
    last_image_buf = &image_bufs[buf_id];
    video_scr_nr = pinfos[buf_id].scr_nr;
    prev_buf_id = buf_id;
    
    // Consume all messages for us and spu_mixer
    if(msgqid != -1) {
      MsgEvent_t ev;
      while(MsgCheckEvent(msgq, &ev) != -1) {
	event_handler(msgq, &ev);
      }
      //chk_for_msg();
    }
    
    //#ifdef SYNCMASTER
    
    if(ctrl_time[pinfos[buf_id].scr_nr].sync_master <= SYNC_VIDEO) {
      ctrl_time[pinfos[buf_id].scr_nr].sync_master = SYNC_VIDEO;
      
      //TODO release scr_nr when done
      
      if(ctrl_time[pinfos[buf_id].scr_nr].offset_valid == OFFSET_NOT_VALID) {
	if(pinfos[buf_id].PTS_DTS_flags & 0x2) {
	  fprintf(stderr, "vo: setting timebase\n");
	  set_time_base(pinfos[buf_id].PTS,
			ctrl_time, pinfos[buf_id].scr_nr, time_offset);
	}
      }
      if(pinfos[buf_id].PTS_DTS_flags & 0x2) {
	time_offset = get_time_base_offset(pinfos[buf_id].PTS,
					   ctrl_time, pinfos[buf_id].scr_nr);
      }
      prev_scr_nr = pinfos[buf_id].scr_nr;
    }
    //#endif
    

    //fprintf(stderr, "display: start displaying buf: %d\n", buf_id);
    PTS_TO_CLOCKTIME(frame_interval, pinfos[buf_id].frame_interval);

    if(first) {
      first = 0;
      display_init(pinfos[buf_id].picture.padded_width,
		   pinfos[buf_id].picture.padded_height,
		   pinfos[buf_id].picture.horizontal_size,
		   pinfos[buf_id].picture.vertical_size,
		   &pinfos[buf_id],
		   &image_bufs[buf_id],
		   picture_ctrl_head,
		   picture_buf_base);
      //display(&(buf_ctrl_head->picture_infos[buf_id].picture));
      /* Erhum test... */
      clocktime_get(&first_time);      
    }
    clocktime_get(&real_time);
    
    if(ctrl_data->mode == MODE_SPEED) {
      clocktime_t difftime;
      
      timesub(&difftime, &real_time, &(ctrl_data->rt_start));
      timemul(&difftime, &difftime, ctrl_data->speed);
      timeadd(&real_time, &(ctrl_data->rt_start), &difftime);
      timesub(&real_time, &real_time, &ctrl_data->vt_offset);
    }
    
    if(TIME_S(prefered_time) == 0 || TIME_SS(frame_interval) == 1) {
      prefered_time = real_time;
    } else if(ctrl_time[pinfos[buf_id].scr_nr].offset_valid == OFFSET_NOT_VALID) {
      prefered_time = real_time;
    } else /* if(TIME_S(pinfos[buf_id].pts_time) != -1) */ {
      
      clocktime_t buftime, pts_time;
      TIME_S(buftime) = 0;
      TIME_SS(buftime) = 00000000;
      PTS_TO_CLOCKTIME(pts_time, pinfos[buf_id].PTS);
      timeadd(&calc_rt,
	      &pts_time,
	      &ctrl_time[pinfos[buf_id].scr_nr].realtime_offset);
      timeadd(&calc_rt, &calc_rt, &buftime);
      prefered_time = calc_rt;
      
    }
    // Fixme!!! wait_time can't be used here..
#ifdef LINUX
    timesub(&waittmp, &prefered_time, &real_time);
#else
    timesub(&wait_time, &prefered_time, &real_time);
#endif
    /*
    fprintf(stderr, "pref: %d.%09ld\n",
	    TIME_S(prefered_time), TIME_SS(prefered_time));

    fprintf(stderr, "wait: %d.%09ld, ",
	    TIME_S(wait_time), TIME_SS(wait_time));
    */
#ifdef LINUX
    wait_time.tv_sec = waittmp.tv_sec;
    wait_time.tv_nsec = waittmp.tv_usec*1000;
#endif
    wait_time.tv_nsec-=10000000;
    if(wait_time.tv_nsec > 0 || wait_time.tv_sec > 0) {
      if(wait_time.tv_nsec < 0) {
	//TODO fix this time mess
	//fprintf(stderr, "APA\n");
	wait_time.tv_nsec = 0;
      }
      if(wait_time.tv_sec > 0) {
	fprintf(stderr, "*vo: waittime > 1 sec\n");
      }
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
		     (double)TIME_SS(avg_time)/CT_FRACTION)-
		    ((double)TIME_S(oavg_time)+
		     (double)TIME_SS(oavg_time)/CT_FRACTION))
	      );
    }
    /*
    clocktime_get(&real_time2);
    timesub(&diff, &prefered_time, &real_time2);
    
    fprintf(stderr, "diff: %d.%+010ld\n",
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
    display(&image_bufs[buf_id]);
    redraw_done();
#endif

      //timeadd(&prefered_time, &prefered_time, &frame_interval);
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








static void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", 
	  program_name);
}

int main(int argc, char **argv)
{
  MsgEvent_t ev;
  int c; 
  program_name = argv[0];
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "m:h?")) != EOF) {
    switch (c) {
    case 'm':
      msgqid = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }

  if(msgqid == -1) {
    if(argc - optind != 1){
      usage();
      return 1;
    }
  }
  
  

  if(msgqid != -1) {
    
    if((msgq = MsgOpen(msgqid)) == NULL) {
      fprintf(stderr, "vo: couldn't get message q\n");
      exit(-1);
    }
    
    register_event_handler(handle_events);
    init_spu();

    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = VIDEO_OUTPUT | DECODE_DVD_SPU;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev) == -1) {
      DPRINTF(1, "vo: register capabilities\n");
    }

    fprintf(stderr, "vo: sent caps\n");

    fprintf(stderr, "vo: waiting for attachq\n");
    
    while(ev.type != MsgEventQAttachQ) {
      MsgNextEvent(msgq, &ev);
      event_handler(msgq, &ev);
    }

    fprintf(stderr, "vo: got attachq\n");


    
    display_process();
  } else {
    fprintf(stderr, "what?\n");
  }

  
}



  

