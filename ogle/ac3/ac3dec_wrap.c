/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

#include <ogle/msgevents.h>

#include "common.h"
#include "queue.h"
#include "timemath.h"
#include "sync.h"




#ifdef DEBUG

int debug_indent_level;
#define DINDENT(spaces) \
{ \
  debug_indent_level += spaces; \
  if(debug_indent_level < 0) { \
    debug_indent_level = 0; \
  } \
} 

#define DPRINTFI(level, text...) \
if(debug >= level) \
{ \
  fprintf(stderr, "%*s", debug_indent_level, ""); \
  fprintf(stderr, ## text); \
}

#define DPRINTF(level, text...) \
if(debug >= level) \
{ \
  fprintf(stderr, ## text); \
}
#else
#define DINDENT(spaces)
#define DPRINTFI(level, text...)
#define DPRINTF(level, text...)
#endif

static int get_q();
static int attach_ctrl_shm(int shmid);
static int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);

static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev);


static char *program_name;

static char *mmap_base;
static FILE *outfile;

static int ctrl_data_shmid;
static ctrl_data_t *ctrl_data;
static ctrl_time_t *ctrl_time;

static int stream_shmid;
static char *stream_shmaddr;

static int data_buf_shmid;
static char *data_buf_shmaddr;

static int msgqid = -1;
static MsgEventQ_t *msgq;

static int flush_to_scrnr = -1;

void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", 
	  program_name);
}


int main(int argc, char *argv[])
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

  // test
  outfile = fopen("/tmp/ac3", "w");
  

  if(msgqid != -1) {
    if((msgq = MsgOpen(msgqid)) == NULL) {
      fprintf(stderr, "ac3wrap: couldn't get message q\n");
      exit(-1);
    }
    
    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = DECODE_AC3_AUDIO;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      DPRINTF(1, "ac3wrap: register capabilities\n");
    }
    
    while(ev.type != MsgEventQDecodeStreamBuf) {
      MsgNextEvent(msgq, &ev);
      handle_events(msgq, &ev);
    }

  } else {
    fprintf(stderr, "what?\n");
  }

  while(1) {
    get_q();
  }


  return 0;
}


static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  
  switch(ev->type) {
  case MsgEventQNotify:
    DPRINTF(1, "ac3wrap: got notify\n");
    break;
  case MsgEventQFlushData:
    DPRINTF(1, "ac3wrap: got flush\n");
    flush_to_scrnr = ev->flushdata.to_scrnr;
    break;
  case MsgEventQDecodeStreamBuf:
    DPRINTF(1, "ac3wrap: got stream %x, %x buffer \n",
	    ev->decodestreambuf.stream_id,
	    ev->decodestreambuf.subtype);
    attach_stream_buffer(ev->decodestreambuf.stream_id,
			  ev->decodestreambuf.subtype,
			  ev->decodestreambuf.q_shmid);
    
    break;
  case MsgEventQCtrlData:
    attach_ctrl_shm(ev->ctrldata.shmid);
    break;
  default:
    fprintf(stderr, "ac3wrap: unrecognized event type: %d\n", ev->type);
    break;
  }
}


static void change_file(char *new_filename)
{
  int filefd;
  static struct stat statbuf;
  int rv;
  static char *cur_filename = NULL;

  // maybe close file when null ?
  if(new_filename == NULL) {
    return;
  }

  if(new_filename[0] == '\0') {
    return;
  }

  // if same filename do nothing
  if(cur_filename != NULL && strcmp(cur_filename, new_filename) == 0) {
    return;
  }

  if(mmap_base != NULL) {
    munmap(mmap_base, statbuf.st_size);
  }
  if(cur_filename != NULL) {
    free(cur_filename);
  }
  
  filefd = open(new_filename, O_RDONLY);
  if(filefd == -1) {
    perror(new_filename);
    exit(1);
  }


  cur_filename = strdup(new_filename);
  rv = fstat(filefd, &statbuf);
  if(rv == -1) {
    perror("fstat");
    exit(1);
  }
  mmap_base = (uint8_t *)mmap(NULL, statbuf.st_size, 
			      PROT_READ, MAP_SHARED, filefd,0);
  close(filefd);
  if(mmap_base == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  
#ifdef HAVE_MADVISE
  rv = madvise(mmap_base, statbuf.st_size, MADV_SEQUENTIAL);
  if(rv == -1) {
    perror("madvise");
    exit(1);
  }
#endif

}


int attach_ctrl_shm(int shmid)
{
  char *shmaddr;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("ac3wrap: attach_ctrl_data(), shmat()");
      return -1;
    }
    
    ctrl_data_shmid = shmid;
    ctrl_data = (ctrl_data_t*)shmaddr;
    ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  }    
  
  return 0;
}

int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid)
{
  char *shmaddr;
  q_head_t *q_head;

  fprintf(stderr, "ac3: shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("ac3: attach_decoder_buffer(), shmat()");
      return -1;
    }
    
    stream_shmid = shmid;
    stream_shmaddr = shmaddr;
  }    

  q_head = (q_head_t *)stream_shmaddr;
  shmid = q_head->data_buf_shmid;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("ac3: attach_data_buffer(), shmat()");
      return -1;
    }
    
    data_buf_shmid = shmid;
    data_buf_shmaddr = shmaddr;
  }    

  return 0;
}




int get_q()
{
  q_head_t *q_head;
  q_elem_t *q_elems;
  data_buf_head_t *data_head;
  data_elem_t *data_elems;
  data_elem_t *data_elem;
  int elem;
  uint8_t *data_buffer;
  uint8_t PTS_DTS_flags;
  uint64_t PTS;
  uint64_t DTS;
  int scr_nr;
  int off;
  int len;
  static int prev_scr_nr = 0;
  static clocktime_t time_offset = { 0, 0 };
  MsgEvent_t ev;
  
  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;

  while(MsgCheckEvent(msgq, &ev) != -1) {
    handle_events(msgq, &ev);
  }
  
  if(!q_elems[elem].in_use) {
    q_head->reader_requests_notification = 1;
    
    while(!q_elems[elem].in_use) {
      DPRINTF(1, "ac3wrap: waiting for notification1\n");
      MsgNextEvent(msgq, &ev);
      handle_events(msgq, &ev);
    }
  }

  data_head = (data_buf_head_t *)data_buf_shmaddr;
  data_buffer = data_buf_shmaddr + data_head->buffer_start_offset;
  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
  
  data_elem = &data_elems[q_elems[elem].data_elem_index];
  
  PTS_DTS_flags = data_elem->PTS_DTS_flags;
  PTS = data_elem->PTS;
  DTS = data_elem->DTS;
  scr_nr = data_elem->scr_nr;
  off = data_elem->off;
  len = data_elem->len;

  if(flush_to_scrnr != -1) {
    if(flush_to_scrnr != scr_nr) {

      q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
      
      change_file(data_elem->filename);
      // release elem
      data_elem->in_use = 0;
      q_elems[elem].in_use = 0;
      
      if(q_head->writer_requests_notification) {
	q_head->writer_requests_notification = 0;
	ev.type = MsgEventQNotify;
	if(MsgSendEvent(msgq, q_head->writer, &ev, 0) == -1) {
	  fprintf(stderr, "ac3wrap: couldn't send notification\n");
	}
      }

      return 0;
    } else {
      flush_to_scrnr = -1;
    }
  }

      

  if(ctrl_time[scr_nr].sync_master <= SYNC_AUDIO) {
    ctrl_time[scr_nr].sync_master = SYNC_AUDIO;
    
    if(ctrl_time[scr_nr].offset_valid == OFFSET_NOT_VALID) {
      if(PTS_DTS_flags & 0x2) {
	// time_offset is our guess to how much is in the output q
	fprintf(stderr, "ac3wrap: initializing offset\n");
	set_time_base(PTS, ctrl_time, scr_nr, time_offset);
      }
    }
    if(PTS_DTS_flags & 0x2) {
      time_offset = get_time_base_offset(PTS, ctrl_time, scr_nr);
    }
    prev_scr_nr = scr_nr;
    
    /*
     * primitive resync in case output buffer is emptied 
     */
    
    if(TIME_SS(time_offset) < 0 || TIME_S(time_offset) < 0) {
      TIME_S(time_offset) = 0;
      TIME_SS(time_offset) = 0;
      fprintf(stderr, "ac3wrap: resetting offset\n");
      set_time_base(PTS, ctrl_time, scr_nr, time_offset);
    }
  }
  if(PTS_DTS_flags & 0x2) {
    time_offset = get_time_base_offset(PTS, ctrl_time, scr_nr);
    if(TIME_S(time_offset) > 10) {
      TIME_S(time_offset) = 0;
      TIME_SS(time_offset) = 0;
      //fprintf(stderr, "more than 10 secs in audio output buffer, somethings wrong?\n");
    }
  }

  /** TODO this is just so we don't buffer alot in the pipe **/
  
  {
#ifndef HAVE_CLOCK_GETTIME
    struct timespec bepa;
    clocktime_t apa = {0, 100000};
    timesub(&apa, &time_offset, &apa);
    bepa.tv_sec = apa.tv_sec;
    bepa.tv_nsec = apa.tv_usec*1000;
    
    if(bepa.tv_nsec > 10000 || bepa.tv_sec > 0) {
      nanosleep(&bepa, NULL);
    }
#else
    
    clocktime_t apa = {0, 100000000};
    timesub(&apa, &time_offset, &apa);
    
    if(TIME_SS(apa) > 10000000 || TIME_S(apa) > 0) {
      nanosleep(&apa, NULL);
    }
    
#endif 
  }
  
  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
  
  //change_file(data_elem->filename);
  
  //fwrite(mmap_base+off, len, 1, outfile);
  
  fwrite(data_buffer+off, len, 1, outfile);
  
  // release elem
  data_elem->in_use = 0;
  q_elems[elem].in_use = 0;
  
  if(q_head->writer_requests_notification) {
    q_head->writer_requests_notification = 0;
    ev.type = MsgEventQNotify;
    if(MsgSendEvent(msgq, q_head->writer, &ev, 0) == -1) {
      fprintf(stderr, "ac3wrap: couldn't send notification\n");
    }
  }

  return 0;
}
