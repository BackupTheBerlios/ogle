/* SKROMPF - A video player
 * Copyright (C) 2000 Björn Englund,Håkan Hjort
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
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#if defined USE_SYSV_SEM
#include <sys/sem.h>
#elif defined USE_POSIX_SEM
#include <semaphore.h>
#endif


#include <sys/shm.h>
#include <sys/msg.h>
#include <errno.h>


#include "c_getbits.h"
#include "../include/common.h"
#include "../include/msgtypes.h"
#include "../include/queue.h"


#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif


extern int ctrl_data_shmid;
extern ctrl_data_t *ctrl_data;
extern ctrl_time_t *ctrl_time;

extern uint8_t PTS_DTS_flags;
extern uint64_t PTS;
extern uint64_t DTS;
extern int scr_nr;

extern int msgqid;
int stream_shmid = -1;
char *stream_shmaddr;

int data_buf_shmid = -1;
char *data_buf_shmaddr;

int wait_for_msg(cmdtype_t cmdtype);
int chk_for_msg();
int get_q();
int eval_msg(cmd_t *cmd);
int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);
#ifdef HAVE_CLOCK_GETTIME
int set_time_base(uint64_t PTS, int scr_nr, struct timespec offset);
struct timespec get_time_base_offset(uint64_t PTS, int scr_nr);
#else
int set_time_base(uint64_t PTS, int scr_nr, struct timeval offset);
struct timeval get_time_base_offset(uint64_t PTS, int scr_nr);
#endif
int attach_ctrl_shm(int shmid);



FILE *infile;
char *infilename;

#ifdef GETBITSMMAP // Mmap i/o
uint32_t *buf;
uint32_t buf_size;
struct off_len_packet packet;
uint8_t *mmap_base = NULL;

#else // Normal i/o
uint32_t buf[BUF_SIZE_MAX] __attribute__ ((aligned (8)));

#endif

#ifdef GETBITS32
unsigned int backed = 0;
unsigned int buf_pos = 0;
unsigned int bits_left = 32;
uint32_t cur_word = 0;
#endif

#ifdef GETBITS64
int offs = 0;
unsigned int bits_left = 64;
uint64_t cur_word = 0;
#endif



#ifdef GETBITSMMAP // Support functions
void setup_mmap(char *filename) {
  int filefd;
  struct stat statbuf;
  int rv;
  
  filefd = open(filename, O_RDONLY);
  if(filefd == -1) {
    perror(filename);
    exit(1);
  }
  rv = fstat(filefd, &statbuf);
  if(rv == -1) {
    perror("fstat");
    exit(1);
  }
  mmap_base = (uint8_t *)mmap(NULL, statbuf.st_size, 
			      PROT_READ, MAP_SHARED, filefd,0);
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
  DPRINTF(1, "All mmap systems ok!\n");
}

#ifdef HAVE_CLOCK_GETTIME

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

#else

static void timesub(struct timeval *d,
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

static void timeadd(struct timeval *d,
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

#endif


void get_next_packet()
{

  if(msgqid == -1) {
    if(mmap_base == NULL) {
#ifdef HAVE_CLOCK_GETTIME
      static struct timespec time_offset = { 0, 0 };
#else
      static struct timeval time_offset = { 0, 0 };
#endif
      setup_mmap(infilename);
      packet.offset = 0;
      packet.length = 1000000000;

      ctrl_time = malloc(sizeof(ctrl_time_t));
      scr_nr = 0;
      ctrl_time[scr_nr].offset_valid = OFFSET_VALID;
      set_time_base(PTS, scr_nr, time_offset);

    } else {
      packet.length = 1000000000;


    }
  } else {
    
    if(stream_shmid != -1) {
      // msg
      while(chk_for_msg() != -1)
	;
      
      // Q
      get_q();
    } 
    
    while(mmap_base == NULL) {
      wait_for_msg(CMD_FILE_OPEN);
    }
    while(stream_shmid == -1) {
      wait_for_msg(CMD_DECODE_STREAM_BUFFER);
    }
    
  }
}  


void read_buf()
{
  uint8_t *packet_base = &mmap_base[packet.offset];
  int end_bytes;
  int i, j;
  
  /* How many bytes are there left? (0, 1, 2 or 3). */
  end_bytes = &packet_base[packet.length] - (uint8_t *)&buf[buf_size];
  
  /* Read them, as we have at least 32 bits free they will fit. */
  i = 0;
  while( i < end_bytes ) {
    //cur_word=cur_word|(((uint64_t)packet_base[packet.length-end_bytes+i++])<<(56-bits_left)); //+
    cur_word=(cur_word << 8) | packet_base[packet.length - end_bytes + i++];
    bits_left += 8;
  }
   
  /* If we have enough 'free' bits so that we always can align
     the buff[] pointer to a 4 byte boundary. */
  if( (64-bits_left) >= 24 ) {
    int start_bytes;
    get_next_packet(); // Get new packet struct
    packet_base = &mmap_base[packet.offset];
    
    /* How many bytes to the next 4 byte boundary? (0, 1, 2 or 3). */
    start_bytes = (4 - ((long)packet_base % 4)) % 4; 
    
    /* Do we have that many bytes in the packet? */
    j = start_bytes < packet.length ? start_bytes : packet.length;
    
    /* Read them, as we have at least 24 bits free they will fit. */
    i = 0;
    while( j-- ) {
      //cur_word=cur_word|(((uint64_t)packet_base[i++])<<(56-bits_left)); //+
      cur_word  = (cur_word << 8) | packet_base[i++];
      bits_left += 8;
    }
     
    buf = (uint32_t *)&packet_base[start_bytes];
    buf_size = (packet.length - start_bytes) / 4; // Number of 32 bit words
    offs = 0;
    
    /* If there were fewer bytes than needed to get to the first 4 byte boundary,
       then we need to make this inte a valid 'empty' packet. */
    if( start_bytes > packet.length ) {
      /* This make the computation of end_bytes come 0 and 
	 forces us to call read_buff() the next time get/dropbits are used. */
      packet.length = start_bytes;
      buf_size = 0;
    }
    
    /* Make sure we have enough bits before we return */
    if(bits_left <= 32) {
      /* If the buffer is empty get the next one. */
      if(offs >= buf_size)
	read_buf();
      else {
	uint32_t new_word = GUINT32_FROM_BE(buf[offs++]);
	//cur_word = cur_word | (((uint64_t)new_word) << (32-bits_left)); //+
	cur_word = (cur_word << 32) | new_word;
	bits_left += 32;
      }
    }
  
  } else {
    /* The trick!! 
       We have enough data to return. Infact it's so much data that we 
       can't be certain that we can read enough of the next packet to 
       align the buff[ ] pointer to a 4 byte boundary.
       Fake it so that we still are at the end of the packet but make
       sure that we don't read the last bytes again. */
    
    packet.length -= end_bytes;
  }

}
#else // Normal (no-mmap) file io support functions

void read_buf()
{
  if(!fread(&buf[0], READ_SIZE, 1 , infile)) {
    if(feof(infile)) {
      fprintf(stderr, "*End Of File\n");
      exit_program(0);
    } else {
      fprintf(stderr, "**File Error\n");
      exit_program(0);
    }
  }
  offs = 0;
}

#endif



#ifdef GETBITS32 // 'Normal' getbits, word based support functions
void back_word(void)
{
  backed = 1;
  
  if(buf_pos == 0) {
    buf_pos = 1;
  } else {
    buf_pos = 0;
  }
  cur_word = buf[buf_pos];
}

void next_word(void)
{

  if(buf_pos == 0) {
    buf_pos = 1;
  } else {
    buf_pos = 0;
  }
  
  
  if(backed == 0) {
    if(!fread(&buf[buf_pos], 1, 4, infile)) {
      if(feof(infile)) {
	fprintf(stderr, "*End Of File\n");
	exit_program(0);
      } else {
	fprintf(stderr, "**File Error\n");
	exit_program(0);
      }
    }
  } else {
    backed = 0;
  }
  cur_word = buf[buf_pos];

}
#endif


/* ---------------------------------------------------------------------- */





int send_msg(msg_t *msg, int mtext_size)
{
  if(msgsnd(msgqid, msg, mtext_size, 0) == -1) {
    perror("video_dec: msgsnd1");
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
	      MTYPE_VIDEO_DECODE_MPEG, 0) == -1) {
      perror("msgrcv");
      return -1;
    } else {
      //fprintf(stderr, "video_decode: got msg\n");
      eval_msg(cmd);
    }
    if(cmdtype == CMD_ALL) {
      break;
    }
  }
  return 0;
}

int chk_for_msg()
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)(msg.mtext);
  cmd->cmdtype = CMD_NONE;
  
  if(msgrcv(msgqid, &msg, sizeof(msg.mtext),
	    MTYPE_VIDEO_DECODE_MPEG, IPC_NOWAIT) == -1) {
    if(errno != ENOMSG) {
      perror("msgrcv");
    }
    return -1;
  } else {
    //fprintf(stderr, "video_decode: got msg\n");
    eval_msg(cmd);
  }
  return 0;
}




int eval_msg(cmd_t *cmd)
{
  msg_t sendmsg;
  cmd_t *sendcmd;
  
  sendcmd = (cmd_t *)&sendmsg.mtext;
  
  switch(cmd->cmdtype) {
  case CMD_CTRL_DATA:
    attach_ctrl_shm(cmd->cmd.ctrl_data.shmid);
    break;
  case CMD_FILE_OPEN:
    //fprintf(stderr, "video_dec: got file open '%s'\n",
    //    cmd->cmd.file_open.file);
    setup_mmap(cmd->cmd.file_open.file);
    break;
  case CMD_DECODE_STREAM_BUFFER:
    //fprintf(stderr, "video_dec: got stream %x, %x buffer \n",
    //	    cmd->cmd.stream_buffer.stream_id,
    //    cmd->cmd.stream_buffer.subtype);
    attach_stream_buffer(cmd->cmd.stream_buffer.stream_id,
			 cmd->cmd.stream_buffer.subtype,
			 cmd->cmd.stream_buffer.q_shmid);
    
    
    break;
  case CMD_CTRL_CMD:
    {
      static ctrlcmd_t prevcmd = CTRLCMD_PLAY;
      switch(cmd->cmd.ctrl_cmd.ctrlcmd) {
      case CTRLCMD_STOP:
	if(prevcmd != CTRLCMD_STOP) {
	  wait_for_msg(CMD_ALL);
	}
	break;
      case CTRLCMD_PLAY:
	break;
      default:
	break;
      }
    }
    break;
  default:
    fprintf(stderr, "video_dec: unrecognized command cmdtype: %x\n",
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
  
  fprintf(stderr, "video_dec: shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("video_stream: attach_decoder_buffer(), shmat()");
      return -1;
    }
    
    stream_shmid = shmid;
    stream_shmaddr = shmaddr;
    
  }    

  q_head = (q_head_t *)stream_shmaddr;
  shmid = q_head->data_buf_shmid;
  fprintf(stderr, "video_dec: data_buf shmid: %d\n", shmid);

  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("video_stream: attach_buffer(), shmat()");
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
  static data_elem_t *data_elem;
  int elem;
  static int delayed_posts = 0;
  int n;
  //  uint8_t PTS_DTS_flags;
  //  uint64_t PTS;
  //  uint64_t DTS;

  int off;
  int len;
  static int have_buf = 0;

#ifdef SYNCMASTER
  static int prev_scr_nr = 0;
#ifdef HAVE_CLOCK_GETTIME
  static struct timespec time_offset = { 0, 0 };
#else
  static struct timeval time_offset = { 0, 0 };
#endif  
#endif

  //fprintf(stderr, "video_dec: get_q()\n");
  
  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;
  
  
  // release prev elem

  if(have_buf) {
    int sval;
   
    data_elem->in_use = 0;
    
#if defined USE_POSIX_SEM
    sem_getvalue(&q_head->bufs_full, &sval);
#elif defined USE_SYSV_SEM
    if((sval = semctl(q_head->semid_bufs, BUFS_FULL, GETVAL, NULL)) == -1) {
      perror("semctl getval");
      exit(-1);
    }
#else
#error No semaphore type set
#endif

    if(sval < 50) {
      for(n = 0; n < delayed_posts+1; n++) {
#if defined USE_POSIX_SEM
	if(sem_post(&q_head->bufs_empty) == -1) {
	  perror("video_decode: get_q(), sem_post()");
	  return -1;
	}
#elif defined USE_SYSV_SEM
	struct sembuf sops;
	sops.sem_num = BUFS_EMPTY;
	sops.sem_op = 1;
	sops.sem_flg = 0;
	if(semop(q_head->semid_bufs, &sops, 1) == -1) {
	  perror("video_decode: get_q(), semop() post");
	  return -1;
	}
#else
#error No semaphore type set
#endif
      }
      delayed_posts = 0;
    } else {
      delayed_posts++;
    }
  }
  
  {
    int sval;
#if defined USE_POSIX_SEM
    sem_getvalue(&q_head->bufs_full, &sval);
#elif defined USE_SYSV_SEM
    if((sval = semctl(q_head->semid_bufs, BUFS_FULL, GETVAL, NULL)) == -1) {
      perror("semctl getval");
      exit(-1);
    }
#else
#error No semaphore type set
#endif

    if(sval < 50) {
      fprintf(stderr, "* Q %d\n", sval);
    }
  }

#if defined USE_POSIX_SEM
  if(sem_wait(&q_head->bufs_full) == -1) {
    perror("video_decode: get_q(), sem_wait()");
    return -1;
  }
#elif defined USE_SYSV_SEM
  {
    struct sembuf sops;
    sops.sem_num = BUFS_FULL;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    if(semop(q_head->semid_bufs, &sops, 1) == -1) {
      perror("video_decode: get_q(), semop() wait");
      return -1;
    }
  }
#else
#error No semaphore type set
#endif
  have_buf = 1;

  data_head = (data_buf_head_t *)data_buf_shmaddr;
  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));

  data_elem = &data_elems[q_elems[elem].data_elem_index];
  
  PTS_DTS_flags = data_elem->PTS_DTS_flags;
  if(PTS_DTS_flags & 0x2) {
    PTS = data_elem->PTS;
    scr_nr = data_elem->scr_nr;
  }
  if(PTS_DTS_flags & 0x1) {
    DTS = data_elem->DTS;
  }
  
#ifdef SYNCMASTER

    
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
#endif
  
  off = data_elem->off;
  len = data_elem->len;
  
  packet.offset = off;
  packet.length = len;
  
  //fprintf(stderr, "video_dec: got q off: %d, len: %d\n",
  //  packet.offset, packet.length);
  /*
  fprintf(stderr, "ac3: flags: %01x, pts: %llx, dts: %llx\noff: %d, len: %d\n",
	  PTS_DTS_flags, PTS, DTS, off, len);
  */
  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;



  return 0;
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

#ifdef HAVE_CLOCK_GETTIME

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
  
  fprintf(stderr, "video_stream: setting offset[%d]: %ld.%09ld\n",
	  scr_nr,
	  ctrl_time[scr_nr].realtime_offset.tv_sec,
	  ctrl_time[scr_nr].realtime_offset.tv_nsec);
  
  return 0;
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

  //fprintf(stderr, "\nac3: get offset: %ld.%09ld\n", offset.tv_sec, offset.tv_nsec);

  return offset;
}
  
#else

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
  
  fprintf(stderr, "video_stream: setting offset[%d]: %ld.%09ld\n",
	  scr_nr,
	  ctrl_time[scr_nr].realtime_offset.tv_sec,
	  ctrl_time[scr_nr].realtime_offset.tv_usec);
  
  return 0;
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

  //fprintf(stderr, "\nac3: get offset: %ld.%06ld\n", offset.tv_sec, offset.tv_usec);

  return offset;
}
  
#endif









