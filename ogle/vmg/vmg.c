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

#ifdef USE_SYSV_SEM
#include <sys/sem.h>
#endif

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

#include "../include/common.h"
#include "../include/msgtypes.h"
#include "../include/queue.h"


int wait_for_msg(cmdtype_t cmdtype);
int eval_msg(cmd_t *cmd);
int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);
int get_q();
int attach_ctrl_shm(int shmid);

void print_time_base_offset(uint64_t PTS, int scr_nr);
#ifdef HAVE_CLOCK_GETTIME
int set_time_base(uint64_t PTS, int scr_nr, struct timespec offset);
struct timespec get_time_base_offset(uint64_t PTS, int scr_nr);
#else
int set_time_base(uint64_t PTS, int scr_nr, struct timeval offset);
struct timeval get_time_base_offset(uint64_t PTS, int scr_nr);
#endif

char *program_name;

int ctrl_data_shmid;
ctrl_data_t *ctrl_data;
ctrl_time_t *ctrl_time;

int stream_shmid;
char *stream_shmaddr;

int data_buf_shmid;
char *data_buf_shmaddr;

int msgqid = -1;

void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", 
	  program_name);
}

int filefd;
char *mmap_base;
FILE *outfile;
struct stat statbuf;
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
  outfile = fopen("/tmp/vmg", "w");
  

  if(msgqid != -1) {
    wait_for_msg(CMD_FILE_OPEN);
    wait_for_msg(CMD_DECODE_STREAM_BUFFER);
  } else {
    fprintf(stderr, "what?\n");
  }
  
  while(1) {
    get_q();
  }


  return 0;
}

int file_open(char *infile)
{
  filefd = open(infile, O_RDONLY);
  fstat(filefd, &statbuf);
  mmap_base = (char *)mmap(NULL, statbuf.st_size, 
			   PROT_READ, MAP_SHARED, filefd,0);
  return 0;
}


int send_msg(msg_t *msg, int mtext_size)
{
  if(msgsnd(msgqid, msg, mtext_size, 0) == -1) {
    perror("ctrl: msgsnd1");
    return -1;
  }
  return 0;
}


int wait_for_msg(cmdtype_t cmdtype)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)(msg.mtext);
  cmd->cmdtype = CMD_NONE;
  
  while(cmd->cmdtype != cmdtype) {
    if(msgrcv(msgqid, &msg, sizeof(msg.mtext),
	      MTYPE_DECODE_MPEG_PRIVATE_STREAM_2, 0) == -1) {
      perror("msgrcv");
      return -1;
    } else {
      fprintf(stderr, "vmg: got msg\n");
      eval_msg(cmd);
    }
    if(cmdtype == CMD_ALL) {
      break;
    }
  }
  return 0;
}




int eval_msg(cmd_t *cmd)
{
  msg_t sendmsg;
  cmd_t *sendcmd;
  
  sendcmd = (cmd_t *)&sendmsg.mtext;
  
  switch(cmd->cmdtype) {
  case CMD_FILE_OPEN:
    file_open(cmd->cmd.file_open.file);
    break;
  case CMD_CTRL_DATA:
    attach_ctrl_shm(cmd->cmd.ctrl_data.shmid);
    break;
  case CMD_DECODE_STREAM_BUFFER:
    fprintf(stderr, "vmg: got stream %x, %x buffer \n",
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
  uint64_t SCR_base;
  uint16_t SCR_ext = 0;
  uint8_t SCR_flags;
  int scr_nr;
  int off;
  int len;
  static int prev_scr_nr = 0;
  static int packnr = 0;
  static struct timespec time_offset = { 0, 0 };
  
  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;
  
#if defined USE_POSIX_SEM  
  if(sem_wait(&q_head->bufs_full) == -1) {
    perror("vmg: get_q(), sem_wait()");
    return -1;
  }
#elif defined USE_SYSV_SEM
  {
    struct sembuf sops;
    sops.sem_num = BUFS_FULL;
    sops.sem_op = -1;
    sops.sem_flg = 0;

    if(semop(q_head->semid_bufs, &sops, 1) == -1) {
      perror("vmg: get_q(), semop() wait");
    }
  }
#else
#error No semaphore type set
#endif


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
      set_time_base(PTS, scr_nr, time_offset);
    }
  }
  if(PTS_DTS_flags && 0x2) {
    time_offset = get_time_base_offset(PTS, scr_nr);
  }
  prev_scr_nr = scr_nr;
  */


  /*
  if((packnr % 10) == 0) {
    print_time_base_offset(PTS, scr_nr);
  }
  packnr++;
  */
  /*
  fprintf(stderr, "vmg: flags: %01x, pts: %llx, dts: %llx\noff: %d, len: %d\n",
	  PTS_DTS_flags, PTS, DTS, off, len);
  */
  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;

  fwrite(mmap_base+off, len, 1, outfile);
  
  data_elem->in_use = 0;
  // release elem

#if defined USE_POSIX_SEM
  if(sem_post(&q_head->bufs_empty) == -1) {
    perror("vmg: get_q(), sem_post()");
    return -1;
  }
#elif defined USE_SYSV_SEM
  {
    struct sembuf sops;
    sops.sem_num = BUFS_EMPTY;
    sops.sem_op = 1;
    sops.sem_flg = 0;
    
    if(semop(q_head->semid_bufs, &sops, 1) == -1) {
      perror("vmg: get_q(), semop() post");
    }
  }
#else
#error No semaphore type set
#endif

  return 0;
}


#ifdef HAVE_CLOCK_GETTIME

int timecompare(struct timespec *s1, struct timespec *s2) {

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

void timesub(struct timespec *d,
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

void timeadd(struct timespec *d,
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


int set_time_base(uint64_t PTS, int scr_nr, struct timespec offset)
{
  struct timespec curtime;
  struct timespec ptstime;
  struct timespec modtime;
  
  ptstime.tv_sec = PTS/90000;
  ptstime.tv_nsec = (PTS%90000)*(1000000000/90000);

  clock_gettime(CLOCK_REALTIME, &curtime);
  timeadd(&modtime, &curtime, &offset);
  timesub(&(ctrl_time[scr_nr].realtime_offset), &modtime, &ptstime);
  ctrl_time[scr_nr].offset_valid = OFFSET_VALID;
  
  fprintf(stderr, "vmg: setting offset[%d]: %ld.%09ld\n",
	  scr_nr,
	  ctrl_time[scr_nr].realtime_offset.tv_sec,
	  ctrl_time[scr_nr].realtime_offset.tv_nsec);
  
  return 0;
}
  

void print_time_base_offset(uint64_t PTS, int scr_nr)
{
  struct timespec curtime;
  struct timespec ptstime;
  struct timespec predtime;
  struct timespec offset;

  ptstime.tv_sec = PTS/90000;
  ptstime.tv_nsec = (PTS%90000)*(1000000000/90000);

  clock_gettime(CLOCK_REALTIME, &curtime);
  timeadd(&predtime, &(ctrl_time[scr_nr].realtime_offset), &ptstime);

  timesub(&offset, &predtime, &curtime);

  //fprintf(stderr, "\nvmg: offset: %ld.%09ld\n", offset.tv_sec, offset.tv_nsec);
}

struct timespec get_time_base_offset(uint64_t PTS, int scr_nr)
{
  struct timespec curtime;
  struct timespec ptstime;
  struct timespec predtime;
  struct timespec offset;

  ptstime.tv_sec = PTS/90000;
  ptstime.tv_nsec = (PTS%90000)*(1000000000/90000);

  clock_gettime(CLOCK_REALTIME, &curtime);
  timeadd(&predtime, &(ctrl_time[scr_nr].realtime_offset), &ptstime);

  timesub(&offset, &predtime, &curtime);

  //fprintf(stderr, "\nvmg: get offset: %ld.%09ld\n", offset.tv_sec, offset.tv_nsec);

  return offset;
}

#else

int timecompare(struct timeval *s1, struct timeval *s2) {

  if(s1->tv_sec > s2->tv_sec) {
    return 1;
  } else if(s1->tv_sec < s2->tv_sec) {
    return -1;
  }

  if(s1->tv_usec > s2->tv_usec) {
    return 1;
  } else if(s1->tv_usec < s2->tv_usec) {
    return -1;
  }
  
  return 0;
}

void timesub(struct timeval *d,
	     struct timeval *s1, struct timeval *s2)
{
  // d = s1-s2

  d->tv_sec = s1->tv_sec - s2->tv_sec;
  d->tv_usec = s1->tv_usec - s2->tv_usec;
  
  if(d->tv_usec >= 1000000) {
    d->tv_sec += 1;
    d->tv_usec -= 1000000;
  } else if(d->tv_usec <= -1000000) {
    d->tv_sec -= 1;
    d->tv_usec += 1000000;
  }

  if((d->tv_sec > 0) && (d->tv_usec < 0)) {
    d->tv_sec -= 1;
    d->tv_usec += 1000000;
  } else if((d->tv_sec < 0) && (d->tv_usec > 0)) {
    d->tv_sec += 1;
    d->tv_usec -= 1000000;
  }

}  

void timeadd(struct timeval *d,
	     struct timeval *s1, struct timeval *s2)
{
  // d = s1+s2
  
  d->tv_sec = s1->tv_sec + s2->tv_sec;
  d->tv_usec = s1->tv_usec + s2->tv_usec;
  if(d->tv_usec >= 1000000) {
    d->tv_usec -=1000000;
    d->tv_sec +=1;
  } else if(d->tv_usec <= -1000000) {
    d->tv_usec +=1000000;
    d->tv_sec -=1;
  }

  if((d->tv_sec > 0) && (d->tv_usec < 0)) {
    d->tv_sec -= 1;
    d->tv_usec += 1000000;
  } else if((d->tv_sec < 0) && (d->tv_usec > 0)) {
    d->tv_sec += 1;
    d->tv_usec -= 1000000;
  }
}  


int set_time_base(uint64_t PTS, int scr_nr, struct timeval offset)
{
  struct timeval curtime;
  struct timeval ptstime;
  struct timeval modtime;
  
  ptstime.tv_sec = PTS/90000;
  ptstime.tv_usec = (PTS%90000)*(1000000/90000);

  gettimeofday(&curtime, NULL);
  timeadd(&modtime, &curtime, &offset);
  timesub(&(ctrl_time[scr_nr].realtime_offset), &modtime, &ptstime);
  ctrl_time[scr_nr].offset_valid = OFFSET_VALID;
  
  fprintf(stderr, "vmg: setting offset[%d]: %ld.%06ld\n",
	  scr_nr,
	  ctrl_time[scr_nr].realtime_offset.tv_sec,
	  ctrl_time[scr_nr].realtime_offset.tv_usec);
  
  return 0;
}
  

void print_time_base_offset(uint64_t PTS, int scr_nr)
{
  struct timeval curtime;
  struct timeval ptstime;
  struct timeval predtime;
  struct timeval offset;

  ptstime.tv_sec = PTS/90000;
  ptstime.tv_usec = (PTS%90000)*(1000000/90000);

  gettimeofday(&curtime, NULL);
  timeadd(&predtime, &(ctrl_time[scr_nr].realtime_offset), &ptstime);

  timesub(&offset, &predtime, &curtime);

  //fprintf(stderr, "\nvmg: offset: %ld.%06ld\n", offset.tv_sec, offset.tv_usec);
}

struct timeval get_time_base_offset(uint64_t PTS, int scr_nr)
{
  struct timeval curtime;
  struct timeval ptstime;
  struct timeval predtime;
  struct timeval offset;

  ptstime.tv_sec = PTS/90000;
  ptstime.tv_usec = (PTS%90000)*(1000000/90000);

  gettimeofday(&curtime, NULL);
  timeadd(&predtime, &(ctrl_time[scr_nr].realtime_offset), &ptstime);

  timesub(&offset, &predtime, &curtime);

  //fprintf(stderr, "\nvmg: get offset: %ld.%06ld\n", offset.tv_sec, offset.tv_usec);

  return offset;
}

#endif

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
