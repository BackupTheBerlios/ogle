#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/msg.h>
#include <errno.h>

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

#include "common.h"
#include "msgtypes.h"
#include "queue.h"
#include "ip_sem.h"
#include "timemath.h"
#include "msgevents.h"
//#include "sync.h"

int get_q();

int attach_ctrl_shm(int shmid);
int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);

static void change_file(char *new_filename);

static char *mmap_base;

static int ctrl_data_shmid;
static ctrl_data_t *ctrl_data;
static ctrl_time_t *ctrl_time;

static int stream_shmid;
static char *stream_shmaddr;

static int data_buf_shmid;
static char *data_buf_shmaddr;

int msgqid = -1;

static MsgEventQ_t *msgq;


void handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  
  switch(ev->type) {
  case MsgEventQNotify:
    //DPRINTF(1, "nav: got notification\n");
    break;
  case MsgEventQChangeFile:
   change_file(ev->changefile.filename);
    break;
  case MsgEventQDecodeStreamBuf:
    /*
      DPRINTF(1, "video_decode: got stream %x, %x buffer \n",
	    ev->decodestreambuf.stream_id,
	    ev->decodestreambuf.subtype);
    */
    attach_stream_buffer(ev->decodestreambuf.stream_id,
			 ev->decodestreambuf.subtype,
			 ev->decodestreambuf.q_shmid);
    
    break;
  case MsgEventQCtrlData:
    attach_ctrl_shm(ev->ctrldata.shmid);
    break;
  default:
    fprintf(stderr, "*vmg: unrecognized event type\n");
    break;
  }
}


static void change_file(char *new_filename)
{
  int filefd;
  static struct stat statbuf;
  int rv;
  static char *cur_filename = NULL;
  
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
      perror("vmg: attach_ctrl_data(), shmat()");
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

  fprintf(stderr, "vmg: shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("vmg: attach_decoder_buffer(), shmat()");
      return -1;
    }
    
    stream_shmid = shmid;
    stream_shmaddr = shmaddr;
  }    

  q_head = (q_head_t *)stream_shmaddr;
  shmid = q_head->data_buf_shmid;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("vmg: attach_data_buffer(), shmat()");
      return -1;
    }
    
    data_buf_shmid = shmid;
    data_buf_shmaddr = shmaddr;
  }    

  return 0;
}

int get_q(char *buffer)
{
  q_head_t *q_head;
  q_elem_t *q_elems;
  data_buf_head_t *data_head;
  data_elem_t *data_elems;
  data_elem_t *data_elem;
  int elem;
  
  uint8_t PTS_DTS_flags;
  uint64_t PTS;
  uint64_t DTS;
  uint64_t SCR_base;
  uint16_t SCR_ext = 0;
  uint8_t SCR_flags;
  int scr_nr;
  int off;
  int len;
  static int prev_scr_nr = 0;
  static int packnr = 0;
  static clocktime_t time_offset = { 0, 0 };
  MsgEvent_t ev;
  
  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;
  
  if(!q_elems[elem].in_use) {
    q_head->reader_requests_notification = 1;
    
    while(!q_elems[elem].in_use) {
      //DPRINTF(1, "vmg: waiting for notification1\n");
      MsgNextEvent(msgq, &ev);
      handle_events(msgq, &ev);
    }
  }


  data_head = (data_buf_head_t *)data_buf_shmaddr;
  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
  
  data_elem = &data_elems[q_elems[elem].data_elem_index];

  //  PTS = q_elems[elem].PTS;
  //DTS = q_elems[elem].DTS;
  SCR_flags = data_elem->SCR_flags;
  SCR_base = data_elem->SCR_base;
  if(SCR_flags & 0x2) {
    SCR_ext = data_elem->SCR_ext;
  }
  //scr_nr = q_elems[elem].scr_nr;
  
  change_file(data_elem->filename);

  off = data_elem->off;
  len = data_elem->len;

  

  /*
  if(SCR_flags & 0x1) {
    fprintf(stderr, "vmg: SCR_base: %llx\n", SCR_base);
  }
  if(SCR_flags & 0x2) {
    fprintf(stderr, "vmg: SCR_ext: %x\n", SCR_ext);
  }
  if(SCR_flags) {
    fprintf(stderr, "vmg: SCR: %.9f\n",
	    (double)(SCR_base*300+SCR_ext)/27000000.0);
  } else {
    fprintf(stderr, "vmg: SCR: no scr\n");
  }
  */
  /*
  //TODO release scr_nr when done
  if(prev_scr_nr != scr_nr) {
    ctrl_time[scr_nr].offset_valid = OFFSET_NOT_VALID;
  }
  
  if(ctrl_time[scr_nr].offset_valid == OFFSET_NOT_VALID) {
    if(PTS_DTS_flags && 0x2) {
      set_time_base(PTS, ctrl_time, scr_nr, time_offset);
    }
  }
  if(PTS_DTS_flags && 0x2) {
    time_offset = get_time_base_offset(PTS, ctrl_time, scr_nr);
  }
  prev_scr_nr = scr_nr;
  */



  /*
  fprintf(stderr, "vmg: flags: %01x, pts: %llx, dts: %llx\noff: %d, len: %d\n",
	  PTS_DTS_flags, PTS, DTS, off, len);
  */
  
  memcpy(buffer, mmap_base+off, len);
  
  
  
  // release elem
  data_elem->in_use = 0;
  q_elems[elem].in_use = 0;

  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;

  if(q_head->writer_requests_notification) {
    q_head->writer_requests_notification = 0;
    ev.type = MsgEventQNotify;
    if(MsgSendEvent(msgq, q_head->writer, &ev) == -1) {
      fprintf(stderr, "vmg: couldn't send notification\n");
    }
  }

  return len;
}
