
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
//#include <siginfo.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/msg.h>
#include <string.h>

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

#include "common.h"
#include "msgtypes.h"
#include "queue.h"
#include "timemath.h"
#include "sync.h"
#include "ip_sem.h"

int send_msg(mq_msg_t *msg, int mtext_size);
int wait_for_msg(mq_cmdtype_t cmdtype);
int eval_msg(mq_cmd_t *cmd);
int get_q();

int attach_ctrl_shm(int shmid);
int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);


char *program_name;

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

void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", 
	  program_name);
}

int main(int argc, char *argv[])
{
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
    //wait_for_msg(CMD_FILE_OPEN);
    wait_for_msg(CMD_DECODE_STREAM_BUFFER);
  } else {
    fprintf(stderr, "what?\n");
  }
  
  while(1) {
    get_q();
  }


  return 0;
}



int send_msg(mq_msg_t *msg, int mtext_size)
{
  if(msgsnd(msgqid, msg, mtext_size, 0) == -1) {
    perror("ctrl: msgsnd1");
    return -1;
  }
  return 0;
}


int wait_for_msg(mq_cmdtype_t cmdtype)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)(msg.mtext);
  cmd->cmdtype = CMD_NONE;
  
  while(cmd->cmdtype != cmdtype) {
    if(msgrcv(msgqid, &msg, sizeof(msg.mtext),
	      MTYPE_AUDIO_DECODE_AC3, 0) == -1) {
      perror("msgrcv");
      return -1;
    } else {
      fprintf(stderr, "ac3dec_wrap: got msg\n");
      eval_msg(cmd);
    }
    if(cmdtype == CMD_ALL) {
      break;
    }
  }
  return 0;
}




int eval_msg(mq_cmd_t *cmd)
{
  mq_msg_t sendmsg;
  mq_cmd_t *sendcmd;
  
  sendcmd = (mq_cmd_t *)&sendmsg.mtext;
  
  switch(cmd->cmdtype) {
  case CMD_CTRL_DATA:
    attach_ctrl_shm(cmd->cmd.ctrl_data.shmid);
    break;
  case CMD_DECODE_STREAM_BUFFER:
    fprintf(stderr, "ac3: got stream %x, %x buffer \n",
	    cmd->cmd.stream_buffer.stream_id,
	    cmd->cmd.stream_buffer.subtype);
    attach_stream_buffer(cmd->cmd.stream_buffer.stream_id,
			  cmd->cmd.stream_buffer.subtype,
			  cmd->cmd.stream_buffer.q_shmid);


    break;
  default:
    fprintf(stderr, "ctrl: unrecognized command cmdtype: %x\n",
	    cmd->cmdtype);
    return -1;
    break;
  }
  
  return 0;
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
      perror("attach_ctrl_data(), shmat()");
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
  
  uint8_t PTS_DTS_flags;
  uint64_t PTS;
  uint64_t DTS;
  int scr_nr;
  int off;
  int len;
  static int prev_scr_nr = 0;
  static clocktime_t time_offset = { 0, 0 };

  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;
  
  if(ip_sem_wait(&q_head->queue, BUFS_FULL) == -1) {
    perror("ac3: get_q(), sem_wait()");
    exit(1);  // XXX 
  }

  data_head = (data_buf_head_t *)data_buf_shmaddr;
  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
  
  data_elem = &data_elems[q_elems[elem].data_elem_index];
  
  PTS_DTS_flags = data_elem->PTS_DTS_flags;
  PTS = data_elem->PTS;
  DTS = data_elem->DTS;
  scr_nr = data_elem->scr_nr;
  off = data_elem->off;
  len = data_elem->len;


  if(ctrl_data->sync_master <= SYNC_AUDIO) {
    ctrl_data->sync_master = SYNC_AUDIO;
    
  //TODO release scr_nr when done
    if(prev_scr_nr != scr_nr) {
      ctrl_time[scr_nr].offset_valid = OFFSET_NOT_VALID;
    }
    
    if(ctrl_time[scr_nr].offset_valid == OFFSET_NOT_VALID) {
      if(PTS_DTS_flags & 0x2) {
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
      
      set_time_base(PTS, ctrl_time, scr_nr, time_offset);
    }
  }
  if(PTS_DTS_flags & 0x2) {
    time_offset = get_time_base_offset(PTS, ctrl_time, scr_nr);
  }

  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
  
  change_file(data_elem->filename);
  
  fwrite(mmap_base+off, len, 1, outfile);
  
  // release elem
  data_elem->in_use = 0;

  if(ip_sem_post(&q_head->queue, BUFS_EMPTY) == -1) {
    perror("ac3: get_q(), sem_post()");
    exit(1);
  }
  
  return 0;
}


