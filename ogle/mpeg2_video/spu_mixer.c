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
#include <errno.h>
#include <assert.h>

#include "common.h"
#include "msgtypes.h"
#include "queue.h"
#include "timemath.h"
#include "ip_sem.h"

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

typedef struct {
  int spu_size;
  uint16_t DCSQT_offset;
  int next_DCSQ_offset;
  int last_DCSQ;  
  uint16_t fieldoffset[2];

  unsigned char *buffer;
  char *next_buffer;
  clocktime_t base_time;
  clocktime_t next_time;
  
  int start_time;
  int width;
  int height;
  int x_start;
  int x_end;
  int y_start;
  int y_end;
  int display_start;
  int menu;
  uint8_t color[4];
  uint8_t contrast[4];
} spu_t;





static int ctrl_data_shmid;
static ctrl_data_t *ctrl_data;
static ctrl_time_t *ctrl_time;

static int stream_shmid;
static char *stream_shmaddr;

static int data_buf_shmid;
static char *data_buf_shmaddr;

static int filefd;
static char *mmap_base;

static int aligned;
static uint16_t fieldoffset[2];
static uint16_t field = 0;

static spu_t spu_info = { 0 };

static int initialized = 0;

static uint32_t palette_yuv[16];
static uint32_t palette_rgb[16];

extern int msgqid;

#define MAX_BUF_SIZE 65536










static uint32_t yuv2rgb(uint32_t yuv_color)
{
  int Y,Cb,Cr;
  int Ey, Epb, Epr;
  int Eg, Eb, Er;
  uint32_t result;
  
  Y  = (yuv_color >> 8)  & 0xff00;
  Cb = (yuv_color)       & 0xff00;
  Cr = (yuv_color << 8)  & 0xff00;
  
  Ey  = (Y-(16<<8));
  Epb = (Cb-(128<<8));
  Epr = (Cr-(128<<8));

  Eg = Ey/219 - 48*Epb/224 - 120*Epr/224;
  Eb = Ey/219 + 475*Epb/224;
  Er = Ey/219 + 403*Epr/224;
  
  if(Eg > 255)
    Eg = 255;
  if(Eg < 0)
    Eg = 0;

  if(Eb > 255)
    Eb = 255;
  if(Eb < 0)
    Eb = 0;

  if(Er > 255)
    Er = 255;
  if(Er < 0)
    Er = 0;
  
  result = ((Eb&0xff)<<16) | ((Eg&0xff)<<8) | (Er&0xff);
  
  return result;
}


static int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid)
{
  char *shmaddr;
  q_head_t *q_head;
  
  fprintf(stderr, "spu_mixer: shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("spu_mixer: attach_decoder_buffer(), shmat()");
      return -1;
    }
    
    stream_shmid = shmid;
    stream_shmaddr = shmaddr;
    
  }    


  q_head = (q_head_t *)stream_shmaddr;
  shmid = q_head->data_buf_shmid;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("spu: attach_data_buffer(), shmat()");
      return -1;
    }
    
    data_buf_shmid = shmid;
    data_buf_shmaddr = shmaddr;
    
  }    
  
  initialized |= 2;

  return 0;
  
}

static int attach_ctrl_shm(int shmid)
{
  char *shmaddr;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("spu_mixer: attach_ctrl_data(), shmat()");
      return -1;
    }
    
    ctrl_data_shmid = shmid;
    ctrl_data = (ctrl_data_t*)shmaddr;
    ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));

  }    
  initialized |= 4;
  return 0;
  
}

static int file_open(char *infile)
{
  struct stat statbuf;
  

  filefd = open(infile, O_RDONLY);
  fstat(filefd, &statbuf);
  mmap_base = (char *)mmap(NULL, statbuf.st_size, 
			   PROT_READ, MAP_SHARED, filefd,0);

  initialized |= 1;
  return 0;
}

static int get_q(char *dst, int readlen, clocktime_t *display_base_time)
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
  static int read_offset = 0;
  clocktime_t pts_time;
  int cpy_len;

  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;
  
  if(!read_offset) {
    
#if defined USE_POSIX_SEM
    if(sem_trywait(&q_head->queue.bufs[BUFS_FULL]) == -1) {
      switch(errno) {
      case EAGAIN:
	return 0;
      default:
	perror("spu_mixer: get_q(), sem_wait()");
	return -1;
      }
    }
#elif defined USE_SYSV_SEM
  {
    struct sembuf sops;
    sops.sem_num = BUFS_FULL;
    sops.sem_op = -1;
    sops.sem_flg = IPC_NOWAIT;
    if(semop(q_head->queue.semid_bufs, &sops, 1) == -1) {
      switch(errno) {
      case EAGAIN:
	return 0;
      default:
	perror("spu_mixer: get_q(), semop() trywait");
	return -1;
      }
    }
  }
#else
#error No semaphore type set
#endif
    
    fprintf(stderr, "spu_mixer: get element\n");
  }
  data_head = (data_buf_head_t *)data_buf_shmaddr;
  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
  
  data_elem = &data_elems[q_elems[elem].data_elem_index];

  off = data_elem->off;
  len = data_elem->len;

  
    
  PTS_DTS_flags = data_elem->PTS_DTS_flags;
  if(PTS_DTS_flags & 0x2) {
    PTS = data_elem->PTS;
    scr_nr = data_elem->scr_nr;
    PTS_TO_CLOCKTIME(pts_time, PTS)
    timeadd(display_base_time, &pts_time, &ctrl_time[scr_nr].realtime_offset);
  }
  if(PTS_DTS_flags & 0x1) {
    DTS = data_elem->DTS;
  }
  
  
  
  /*
  fwrite(mmap_base+off, len, 1, outfile);
  fflush(outfile);
  */
  //fprintf(stderr, "spu_mixer: len: %d\n", len);
  //fprintf(stderr, "spu_mixer: readlen: %d\n", readlen);
  //fprintf(stderr, "spu_mixer: read_offset: %d\n", read_offset);

  if((readlen+read_offset) > len) {
    cpy_len = len-read_offset;
    //fprintf(stderr, "spu_mixer: bigger than available\n");
  } else {
    cpy_len = readlen;
  }
  //fprintf(stderr, "spu_mixer: cpy_len: %d\n", cpy_len);
  memcpy(dst, mmap_base+off+read_offset, cpy_len);
  
  if(cpy_len+read_offset == len) {
    read_offset = 0;
  } else {
    read_offset+=cpy_len;
  }
  
  if(read_offset) {
    return cpy_len;
  }
  
  // release elem
  fprintf(stderr, "spu_mixer: release element\n");
  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
  data_elem->in_use = 0;

  if(ip_sem_post(&q_head->queue, BUFS_EMPTY) == -1) {
    perror("spu: get_q(), sem_post()");
    return -1;
  }
  
  return cpy_len;
}



static int eval_msg(cmd_t *cmd)
{
  msg_t sendmsg;
  cmd_t *sendcmd;
  
  sendcmd = (cmd_t *)&sendmsg.mtext;
  
  switch(cmd->cmdtype) {
  case CMD_FILE_OPEN:
    fprintf(stderr, "spu_mixer: CMD_FILE_OPEN\n");
    file_open(cmd->cmd.file_open.file);
    break;
  case CMD_CTRL_DATA:
    fprintf(stderr, "spu_mixer: CMD_CTRL_DATA\n");
    attach_ctrl_shm(cmd->cmd.ctrl_data.shmid);
    break;
  case CMD_DECODE_STREAM_BUFFER:
    fprintf(stderr, "spu_mixer: got stream %x, %x buffer \n",
	    cmd->cmd.stream_buffer.stream_id,
	    cmd->cmd.stream_buffer.subtype);
    attach_stream_buffer(cmd->cmd.stream_buffer.stream_id,
			  cmd->cmd.stream_buffer.subtype,
			  cmd->cmd.stream_buffer.q_shmid);
    break;
  case CMD_SPU_SET_PALETTE:
    {
      int n;
      for(n = 0; n < 16; n++) {
	palette_yuv[n] = cmd->cmd.spu_palette.colors[n];
	palette_rgb[n] = yuv2rgb(palette_yuv[n]);
      }
    }
    break;
  default:
    fprintf(stderr, "spu_mixer: unrecognized command cmdtype: %x\n",
	    cmd->cmdtype);
    return -1;
    break;
  }
  
  return 0;
}


static int chk_for_msg(void)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)(msg.mtext);
  cmd->cmdtype = CMD_NONE;
  
  if(msgrcv(msgqid, &msg, sizeof(msg.mtext),
	    MTYPE_SPU_DECODE, IPC_NOWAIT) == -1) {
    if(errno != ENOMSG) {
      perror("msgrcv");
    }
    return -1;
  } else {
    fprintf(stderr, "spu_mixer: got msg\n");
    eval_msg(cmd);
  }
  return 0;
}


int init_spu(void)
{
  fprintf(stderr, "spu_mixer: init\n");
  spu_info.buffer = malloc(MAX_BUF_SIZE);
  spu_info.next_buffer = malloc(MAX_BUF_SIZE);
  if(spu_info.buffer == NULL || spu_info.next_buffer == NULL)
    perror("init_spu");

  return 0;
}



int get_data(uint8_t *databuf, int bufsize, clocktime_t *dtime)
{
  int r;
  static int bytes_to_read = 0;
  static int state = 0;
  static int spu_size;
  /* Make sure to have a buffer bigger than this */
  /*
  if(bufsize < 2) {
    // databuf not big enough
    fprintf(stderr, "buffer too small\n");
    return -1;
  }
  */

  if(bytes_to_read == 0) {
  // get first 2 bytes of spu (size of spu)
    bytes_to_read = 2;
  }
  if(state == 0) {
    while(bytes_to_read > 0) {
      r = get_q(&databuf[2-bytes_to_read], bytes_to_read, dtime);
      
      if(r > 0) {
	bytes_to_read -= r;
      } else if(r < 0) {
	perror("read");
	return -1;
      } else if(r == 0) {
	/* no more elements in q at this moment */
	//fprintf(stderr, "q empty, %d bytes read\n", 2-bytes_to_read);
	return 0;
      }
    }
    
    spu_size = (databuf[0]<<8) | databuf[1];
    
    if(spu_size > bufsize) {
      // databuf not big enough
      fprintf(stderr, "buffer too small\n");
      return -1;
    }
    
    // already read the first two bytes
    bytes_to_read = spu_size - 2;
    
    state = 1;
  }

  // get the rest of the spu
  while(bytes_to_read > 0) {
    r = get_q(&databuf[spu_size-bytes_to_read], bytes_to_read, dtime);
    
    if(r > 0) {
      bytes_to_read -= r;
    } else if(r < 0) {
      perror("read");
      return -1;
    } else  {
      /* no more elements in q at this time */
      /*
	fprintf(stderr, "q empty, %d of %d bytes read\n",
	      spu_size-bytes_to_read, spu_size);
      */
      return 0;
    }   
  }
  bytes_to_read = 0;
  state = 0;
  return spu_size;
  
}


#ifdef DEBUG
int debug;
#endif

#ifdef DEBUG
#define DPRINTF(level, text...) \
if(debug > level) \
{ \
    fprintf(stderr, ## text); \
}
#else
#define DPRINTF(level, text...)
#endif

#ifdef DEBUG
#define DPRINTBITS(level, bits, value) \
{ \
  int n; \
  for(n = 0; n < bits; n++) { \
    DPRINTF(level, "%u", (value>>(bits-n-1)) & 0x1); \
  } \
}
#else
#define DPRINTBITS(level, bits, value)
#endif

#ifdef DEBUG
#define GETBYTES(a,b) getbytes(a,b)
#else
#define GETBYTES(a,b) getbytes(a)
#endif

static uint8_t *byte_pos;

static inline void set_byte (uint8_t *byte) {
  byte_pos = byte;
  aligned = 1;
}

#ifdef DEBUG
uint32_t getbytes(unsigned int num, char *func)
#else
static inline uint32_t getbytes(unsigned int num)
#endif
{
#ifdef DEBUG
  int tmpnum = num;
#endif
  uint32_t result = 0;

  assert(num <= 4);

  while(num > 0) {
    result <<= 8;
    result |= *byte_pos;
    byte_pos++;
    num--;
  }

  DPRINTF(5, "\n%s getbytes(%u): %i, 0x%0*x, ", 
          func, tmpnum, result, tmpnum*2, result);
  DPRINTBITS(5, tmpnum*8, result);
  DPRINTF(5, "\n");
  return result;
}

static inline uint8_t get_nibble (void)
{
  static uint8_t next = 0;
  if (aligned) {
    fieldoffset[field]++;
    next = GETBYTES(1, "get_nibble (aligned)");
    aligned = 0;
    return next >> 4;
  } else {
    aligned = 1;
    return next & 0xf;
  }
}



void decode_dcsqt(spu_t *spu_info) {
  // init start position
  set_byte(spu_info->buffer);
  
  /* Reset state  */
  spu_info->menu = 0;
  spu_info->display_start = 0;
  // Maybe some other...
  
  spu_info->spu_size = GETBYTES(2, "spu_size");
  DPRINTF(3, "SPU size: 0x%04x\n", spu_info->spu_size);
  
  spu_info->DCSQT_offset = GETBYTES(2, "DCSQT_offset");
  DPRINTF(3, "DCSQT offset: 0x%04x\n", spu_info->DCSQT_offset);
  
  DPRINTF(3, "Display Control Sequence Table:\n");
  
  /* parse the DCSQT */
  set_byte(spu_info->buffer+spu_info->DCSQT_offset);
  
  spu_info->next_DCSQ_offset = spu_info->DCSQT_offset;
  
  return;
}


void decode_dcsq(spu_t *spu_info) { 
  uint8_t command;
  uint32_t dummy;
  
  /* DCSQ */
  set_byte(spu_info->buffer+spu_info->next_DCSQ_offset);
  DPRINTF(3, "\tDisplay Control Sequence:\n");
    
  spu_info->start_time = GETBYTES(2, "start_time");
  DPRINTF(3, "\t\tStart time: 0x%04x\n", spu_info->start_time);
  spu_info->last_DCSQ = spu_info->next_DCSQ_offset;
  spu_info->next_DCSQ_offset = GETBYTES(2, "next_DCSQ_offset");
  DPRINTF(3, "\t\tNext DCSQ offset: 0x%04x\n", spu_info->next_DCSQ_offset);
    
  DPRINTF(3, "\t\tCommand Sequence Start:\n");
    
  while((command = GETBYTES(1, "command")) != 0xff) {
    /* Command Sequence */
      
    DPRINTF(3, "\t\t\tDisplay Control Command: 0x%02x\n", command);
      
    switch (command) {
    case 0x00: /* Menu */
      DPRINTF(3, "\t\t\t\tMenu...\n");
      spu_info->menu = 1;
      break;
    case 0x01: /* display start */
      DPRINTF(3, "\t\t\t\tdisplay start\n");
      spu_info->display_start = 1;
      break;
    case 0x02: /* display stop */
      DPRINTF(3, "\t\t\t\tdisplay stop\n");
      spu_info->display_start = 0;
      break;
    case 0x03: /* set colors */
      DPRINTF(3, "\t\t\t\tset colors");
      dummy = GETBYTES(2, "set_colors");
      spu_info->color[0] = dummy      & 0xf;
      spu_info->color[1] = (dummy>>4) & 0xf;
      spu_info->color[2] = (dummy>>8) & 0xf;
      spu_info->color[3] = (dummy>>12);
      DPRINTF(4, "0x%x 0x%x 0x%x 0x%x",
	      spu_info->color[0], spu_info->color[1],
	      spu_info->color[2], spu_info->color[3]);
      DPRINTF(3, "\n");
      break;
    case 0x04: /* set contrast */
      DPRINTF(3, "\t\t\t\tset contrast");
      dummy = GETBYTES(2, "set_contrast");
      spu_info->contrast[0] = dummy      & 0xf;
      spu_info->contrast[1] = (dummy>>4) & 0xf;
      spu_info->contrast[2] = (dummy>>8) & 0xf;
      spu_info->contrast[3] = (dummy>>12);
      DPRINTF(4, "0x%x 0x%x 0x%x 0x%x",
	      spu_info->contrast[0], spu_info->contrast[1],
	      spu_info->contrast[2], spu_info->contrast[3]);
      DPRINTF(3, "\n");
      break;
    case 0x05: /* set sp screen position */
      DPRINTF(3, "\t\t\t\tset sp screen position\n");
      dummy = GETBYTES(3, "x coordinates");
      spu_info->x_start = dummy>>12;
      spu_info->x_end   = dummy & 0xfff;
      dummy = GETBYTES(3, "y coordinates");
      spu_info->y_start = dummy>>12;
      spu_info->y_end   = dummy & 0xfff;
      spu_info->width  = spu_info->x_end - spu_info->x_start + 1;
      spu_info->height = spu_info->y_end - spu_info->y_start + 1;
      DPRINTF(4, "\t\t\t\t\tx_start=%i x_end=%i, y_start=%i, y_end=%i "
	      "width=%i height=%i\n",
	      spu_info->x_start, spu_info->x_end,
	      spu_info->y_start, spu_info->y_end,
	      spu_info->width, spu_info->height);
      break;
    case 0x06: /* set start address in PXD */
      DPRINTF(3, "\t\t\t\tset start address in PXD\n");
      spu_info->fieldoffset[0] = GETBYTES(2, "field 0 start");
      spu_info->fieldoffset[1] = GETBYTES(2, "field 1 start");
      DPRINTF(4, "\t\t\t\t\tfield_0=%i field_1=%i\n",
	      spu_info->fieldoffset[0], spu_info->fieldoffset[1]);
      break;
    case 0x07: /* wipe */
      {
	uint16_t num;
	uint8_t color_left[4];
	uint8_t contrast_left[4];
	uint8_t color_right[4];
	uint8_t contrast_right[4];
	uint16_t y_start;
	uint16_t y_end;
	uint16_t x_pos;
	uint16_t u1, u2, u3; /* x_start, x_end, ? */
	int type;
	  
	DPRINTF(3, "\t\t\t\twipe\n");
	  
	num = GETBYTES(2,"cmd 7 length");
	  
	y_start = GETBYTES(2, "wipe y_start");
	  
	dummy = GETBYTES(2, "wipe y_end");
	type = (dummy >> 12) & 0xf;
	y_end = dummy & 0xfff;
	  
	u1 = GETBYTES(2, "wipe x_start 1?");
	  
	dummy = GETBYTES(2, "wipe color_left");
	color_left[0] = dummy & 0xf;
	color_left[1] = (dummy >> 4) & 0xf;
	color_left[2] = (dummy >> 8) & 0xf;
	color_left[3] = (dummy >> 12) & 0xf;
	  
	dummy = GETBYTES(2, "wipe contrast_left");
	contrast_left[0] = dummy & 0xf;
	contrast_left[1] = (dummy >> 4) & 0xf;
	contrast_left[2] = (dummy >> 8) & 0xf;
	contrast_left[3] = (dummy >> 12) & 0xf;
	  
	if(type == 2) {
	    
	  x_pos = GETBYTES(2, "wipe x_pos");
	    
	  dummy = GETBYTES(2, "wipe color_right");
	  color_right[0] = dummy & 0xf;
	  color_right[1] = (dummy >> 4) & 0xf;
	  color_right[2] = (dummy >> 8) & 0xf;
	  color_right[3] = (dummy >> 12) & 0xf;
	    
	  dummy = GETBYTES(2, "wipe contrast_right");
	  contrast_right[0] = dummy & 0xf;
	  contrast_right[1] = (dummy >> 4) & 0xf;
	  contrast_right[2] = (dummy >> 8) & 0xf;
	  contrast_right[3] = (dummy >> 12) & 0xf;
	    
	} 
	
	if(type == 1 || type == 2) {
	    
	  u2 = GETBYTES(2, "wipe x_end 1?");
	  u3 = GETBYTES(2, "wipe x_end 2?");
	    
	} else {
	  fprintf(stderr, "unknown cmd 07 type\n");
	  exit(-1);
	}
	  
	if((u1 != 0) || (u2 != 0x0fff) || (u3 != 0xffff)) {
	  fprintf(stderr, "unknown bits in cmd 7 used\n");
	  exit(-1);
	}
	  
      }
      break;
    default:
      fprintf(stderr,
	      "\t\t\t\t*Unknown Display Control Command: %02x\n",
	      command);
      //exit(1);
      break;
    }
  }
  DPRINTF(3, "\t\tEnd of Command Sequence\n");
    
    
  if(spu_info->last_DCSQ == spu_info->next_DCSQ_offset) {
    DPRINTF(3, "End of Display Control Sequence Table\n");
  }
}


void decode_display_data(spu_t *spu_info, char *data, int width, int height) {
  unsigned int x;
  unsigned int y;
  int nr_vlc;
  uint32_t color;
  uint32_t contrast;
  uint32_t invcontrast;
  uint32_t *pixel;

  fieldoffset[0] = spu_info->fieldoffset[0];
  fieldoffset[1] = spu_info->fieldoffset[1];
  field=0;
  aligned = 1;
  set_byte(&spu_info->buffer[fieldoffset[field]]);
  
  //    fprintf(stderr, "\nReading picture data\n");
  
  //    initialize(spu_info->width, spu_info->height);
  x=0;
  y=0;
  nr_vlc = 0;
  
  {
    int i;
    for(i = 0; i < 16; i++) {
      fprintf(stderr, "[%d] %08x %08x\n", i, palette_yuv[i], palette_rgb[i]);
    }
  }
  
  DPRINTF(5, "vlc decoding\n");
  while((fieldoffset[1] < spu_info->DCSQT_offset) && (y < spu_info->height)) {
    unsigned int vlc;
    unsigned int length;
    static unsigned int colorid;
    unsigned char pixel_data;
    
    DPRINTF(6, "fieldoffset[0]: %d, fieldoffset[1]: %d, DCSQT_offset: %d\n",
	    fieldoffset[0], fieldoffset[1], DCSQT_offset);
    
    
    /*
      llcc
      
      01cc - 11cc
      len: 1 - 3
      
      00ll llcc
      
      0001 00cc - 0011 11cc
      len: 4 - 15
      
	
      0000 llll llcc
      
      0000 0100 00cc - 0000 1111 11cc
      len: 16 - 63
      
      
      0000 00ll llll llcc
      
      0000 0001 0000 00cc - 0000 0011 1111 11cc
      len: 64 - 255, 0

    */
    
    vlc = get_nibble();
    if(vlc < 0x4) {   //  vlc!= 0xN
      vlc = (vlc << 4) | get_nibble();
      if(vlc < 0x10) {  // vlc != 0xN-
	vlc = (vlc << 4) | get_nibble();
	if(vlc < 0x40) {  // vlc != 0xNN-
	  vlc = (vlc << 4) | get_nibble();  // vlc != 0xNN--
	}
      }
    }
    
    nr_vlc++;
    DPRINTF(4, "send_rle: %08x\n", vlc);
    /* last two bits are colorid, the rest are run length */
    length = vlc >> 2;
    
    colorid = vlc & 3;
    pixel_data = ((spu_info->contrast[colorid] << 4) 
		  | (spu_info->color[colorid] & 0x0f));
  
    color = palette_rgb[spu_info->color[colorid]];
    contrast = spu_info->contrast[colorid]<<4;
    invcontrast = 256-contrast;
    if(length==0) { // new line
      //   if (y >= height)
      // return;
      /* Fill current line with background color */
      length = spu_info->width-x;
    }
    if(length+x > spu_info->width) {
      fprintf(stderr, "tried to write past line-end\n");
      length = spu_info->width-x;
    }

    /* mix spu and picture data */
    {
      int n;
      
      /* if total transparency do nothing */
      if(contrast != 0) {
	uint32_t r,g,b;
	r = color&0xff;
	g = (color>>8) & 0xff;
	b = (color>>16) & 0xff;

	pixel = &(((uint32_t *)data)[(y+spu_info->y_start)*width
				    +(x+spu_info->x_start)]);
	
	for(n = 0; n < length; n++,pixel++) {
	  
	  /* if no transparancy just overwrite */
	  if(contrast == (0xf<<4)) {
	    *pixel = color;
	  } else {
	    uint32_t pr,pg,pb;
	    pr = *pixel&0xff;
	    pg = (*pixel>>8)&0xff;
	    pb = (*pixel>>16)&0xff;
	    
	    pr = (pr*(invcontrast)+r*contrast)>>8;
	    pg = (pg*(invcontrast)+g*contrast)>>8;
	    pb = (pb*(invcontrast)+b*contrast)>>8;
	    
	    *pixel = pb<<16 | pg<<8 | pr;
	  }
	}

      }
    }

    x = x+length;
    
    if(x >= spu_info->width) {
      x=0;
      y++;
      field = 1-field;
      set_byte(&spu_info->buffer[fieldoffset[field]]);
    }
    
  }
}  


/*
 * Return 'true' if it is time for the next command sequence to 
 * be 'executed'. If no next sequence exist or if it is in the  
 * future, return 'false'.
 */
int next_spu_cmd_pending(spu_t *spu_info) {
  int start_time, offset;
  clocktime_t realtime, errtime;

  /* I next_time haven't been set, try to set it. */
  if(TIME_S(spu_info->next_time) == 0) {
    
    if(spu_info->next_DCSQ_offset == spu_info->last_DCSQ) {
      
      if(!get_data(spu_info->next_buffer, MAX_BUF_SIZE, &spu_info->base_time))
	return 0;
      
      /* The offset to the first DCSQ */
      offset = ((spu_info->next_buffer[2] << 8) | (spu_info->next_buffer[3]));
      
      /* The Start time is located at the start of the DCSQ */ 
      start_time = ((spu_info->next_buffer[offset] << 8) 
		    | spu_info->next_buffer[offset + 1]);
    } else {
      /* The Start time is located at the start of the DCSQ */ 
      start_time = ((spu_info->buffer[spu_info->next_DCSQ_offset] << 8) 
		    | spu_info->buffer[spu_info->next_DCSQ_offset + 1]);
    }
    
    TIME_S(spu_info->next_time)  = start_time/100;
    TIME_SS(spu_info->next_time) = (start_time%100) * CT_FRACTION/100;  
    timeadd(&spu_info->next_time, &spu_info->base_time, &spu_info->next_time);
  }

  clocktime_get(&realtime);
  timesub(&errtime, &spu_info->next_time, &realtime);

  if(TIME_SS(errtime) < 0 || TIME_S(errtime) < 0)
    return 1;

  return 0;
}


void mix_subpicture(char *data, int width, int height)
{
  /*
   * Check for, and process, messages. Exit if not 
   * Then execute all pending spu command sequences.
   * If there then is an active overlay call the acutal mix function.
   */

  chk_for_msg();
  
  if((initialized & 3) != 3) {
    return;
  }

  while(next_spu_cmd_pending(&spu_info)) {

    if(spu_info.next_DCSQ_offset == spu_info.last_DCSQ) {
      char *swap;
      /* Change to the next buffer and recycle the old */
      swap = spu_info.buffer;
      spu_info.buffer = spu_info.next_buffer;
      spu_info.next_buffer = swap;
      
      decode_dcsqt(&spu_info);
    }

    decode_dcsq(&spu_info);

    /* 
     * The 'next command' is no longer the same, the time needs to
     * be recomputed. 
     */
    TIME_S(spu_info.next_time) = 0;
  }


  if(spu_info.display_start || spu_info.menu) {
    //fprintf(stderr, "decoding data\n");
    decode_display_data(&spu_info, data, width, height);
  }
  
}



