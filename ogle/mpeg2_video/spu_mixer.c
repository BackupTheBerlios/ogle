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
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/msg.h>
#include <errno.h>
#include <string.h>

#include "debug_print.h"
#include "common.h"
#include "queue.h"
#include "timemath.h"
#include "sync.h"

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif





#ifdef DEBUG
#define GETBYTES(a,b) getbytes(a,b)
#else
#define GETBYTES(a,b) getbytes(a)
#endif

typedef struct {
  int spu_size;
  uint16_t DCSQT_offset;
  int next_DCSQ_offset;
  int last_DCSQ;  
  uint16_t fieldoffset[2];

  unsigned char *buffer;
  unsigned char *next_buffer;
  int scr_nr;
  uint64_t base_time;
  uint64_t next_time;
  
  int start_time;
  int width;
  int height;
  int x_start;
  int x_end;
  int y_start;
  int y_end;
  int display_start;
  int has_highlight;
  uint8_t color[4];
  uint8_t contrast[4];
} spu_handle_t;

typedef struct {
  uint8_t color[4];
  uint8_t contrast[4];
  int x_start;
  int y_start;
  int x_end;
  int y_end;
} highlight_t;


extern ctrl_data_t *ctrl_data;
extern ctrl_time_t *ctrl_time;

static int stream_shmid;
static char *stream_shmaddr;

static int data_buf_shmid;
static char *data_buf_shmaddr;

static int aligned;
static uint16_t fieldoffset[2];
static uint16_t field = 0;

static spu_handle_t spu_info = { 0 };

static int initialized = 0;

static uint32_t palette_yuv[16];
static uint32_t palette_rgb[16];

static highlight_t highlight = {{0,1,2,3}, {0xf, 0xa, 0x6,0x2}, 2,2,718, 450};

extern int video_scr_nr;
extern int msgqid;

extern MsgEventQ_t *msgq;

static int flush_to_scrid = -1;

#define MAX_BUF_SIZE 65536


extern void redraw_request(void);
extern int register_event_handler(int(*eh)(MsgEventQ_t *, MsgEvent_t *));


static uint32_t yuv2rgb(uint32_t yuv_color)
{
  int Y,Cb,Cr;
  int Ey, Epb, Epr;
  int Eg, Eb, Er;
  uint32_t result;
  
  Y  = (yuv_color >> 16) & 0xff;
  Cb = (yuv_color      ) & 0xff;
  Cr = (yuv_color >> 8 ) & 0xff;
 
  Ey  = (Y-16);
  Epb = (Cb-128);
  Epr = (Cr-128);
  /* ITU-R 709
  Eg = (298*Ey - 55*Epb - 137*Epr)/256;
  Eb = (298*Ey + 543*Epb)/256;
  Er = (298*Ey + 460*Epr)/256;
  */
  /* FCC ~= mediaLib */
  Eg = (298*Ey - 100*Epb - 208*Epr)/256;
  Eb = (298*Ey + 516*Epb)/256;
  Er = (298*Ey + 408*Epr)/256;
  
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
  
  result = (Eb << 16) | (Eg << 8) | Er;
  
  return result;
}



static int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid)
{
  char *shmaddr;
  q_head_t *q_head;
  
  //DNOTE("spu_mixer: shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      ERROR("spu_mixer: attach_decoder_buffer()", 0);
      perror("shmat");
      return -1;
    }
    
    stream_shmid = shmid;
    stream_shmaddr = shmaddr;
  }    

  q_head = (q_head_t *)stream_shmaddr;
  shmid = q_head->data_buf_shmid;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      ERROR("spu: attach_data_buffer()", 0);
      perror("shmat");
      return -1;
    }
    
    data_buf_shmid = shmid;
    data_buf_shmaddr = shmaddr;
  }    
  
  initialized = 1;
  return 0;
}



static int handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  switch(ev->type) {
  case MsgEventQNotify:
    if((stream_shmaddr != NULL) &&
       (ev->notify.qid == ((q_head_t *)stream_shmaddr)->qid)) {
      DPRINTF(1, "spu_mixer: got notification\n");
      redraw_request();
    } else {
      return 0;
    }
    break;
  case MsgEventQDecodeStreamBuf:
    DPRINTF(1, "video_decode: got stream %x, %x buffer \n",
	    ev->decodestreambuf.stream_id,
	    ev->decodestreambuf.subtype);
  
    attach_stream_buffer(ev->decodestreambuf.stream_id,
			 ev->decodestreambuf.subtype,
			 ev->decodestreambuf.q_shmid);
    break;
  case MsgEventQSPUPalette:
    {
      int n;
      /* Should have PTS or SCR I think */
      for(n = 0; n < 16; n++) {
	palette_yuv[n] = ev->spupalette.colors[n];
	palette_rgb[n] = yuv2rgb(palette_yuv[n]);
      }
      redraw_request();
    }
    break;
  case MsgEventQSPUHighlight:
    {
      int n;
      
      /* Enable the highlight, should have PTS or scr I think */
      spu_info.has_highlight = 1;
      
      highlight.x_start = ev->spuhighlight.x_start;
      highlight.y_start = ev->spuhighlight.y_start;
      highlight.x_end = ev->spuhighlight.x_end;
      highlight.y_end = ev->spuhighlight.y_end;

      for(n = 0; n < 4; n++) {
	highlight.color[n] = ev->spuhighlight.color[n];
      }
      for(n = 0; n < 4; n++) {
	highlight.contrast[n] = ev->spuhighlight.contrast[n];
      }
      redraw_request();
    }
    break;
  default:
    /* DNOTE("spu_mixer: ignoring event type (%d)\n", ev->type); */
    return 0;
    break;
  }
  return 1;
}


static int get_q(char *dst, int readlen, uint64_t *display_base_time, 
		 int *new_scr_nr)
{
  MsgEvent_t ev;
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
  static int read_offset = 0;
  //  clocktime_t pts_time;
  int cpy_len;

  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;
  
  if(!read_offset) {    
    if(!q_elems[elem].in_use) {
      q_head->reader_requests_notification = 1;
      
      if(!q_elems[elem].in_use) {
	return 0;
      }
    }
    //DNOTE("spu_mixer: get element\n");
  }
  data_head = (data_buf_head_t *)data_buf_shmaddr;
  data_buffer = data_buf_shmaddr + data_head->buffer_start_offset;
  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
  
  data_elem = &data_elems[q_elems[elem].data_elem_index];

  off = data_elem->packet_data_offset+1;
  len = data_elem->packet_data_len-1;
    
  PTS_DTS_flags = data_elem->PTS_DTS_flags;
  if(PTS_DTS_flags & 0x2) {
    PTS = data_elem->PTS;
    scr_nr = data_elem->scr_nr;
    *display_base_time = PTS;
      /*
    PTS_TO_CLOCKTIME(pts_time, PTS);
    calc_realtime_from_scrtime(display_base_time, &pts_time, 
			       &ctrl_time[scr_nr].sync_point);
      */
    *new_scr_nr = scr_nr;
  }
  if(PTS_DTS_flags & 0x1) {
    DTS = data_elem->DTS;
  }
  
  //DNOTE("spu_mixer: len: %d\n", len);
  //DNOTE("spu_mixer: readlen: %d\n", readlen);
  //DNOTE("spu_mixer: read_offset: %d\n", read_offset);

  if((readlen + read_offset) > len) {
    cpy_len = len-read_offset;
    //DNOTE("spu_mixer: bigger than available\n");
  } else {
    cpy_len = readlen;
  }
  //DNOTE("spu_mixer: cpy_len: %d\n", cpy_len);
  memcpy(dst, data_buffer + off + read_offset, cpy_len);
  
  if(cpy_len + read_offset == len) {
    read_offset = 0;
  } else {
    read_offset += cpy_len;
  }
  
  if(read_offset) {
    return cpy_len;
  }
  
  // release elem
  //DNOTE("spu_mixer: release element\n");
  
  data_elem->in_use = 0;
  q_elems[elem].in_use = 0;
  
  if(q_head->writer_requests_notification) {
    q_head->writer_requests_notification = 0;
    ev.type = MsgEventQNotify;
    if(MsgSendEvent(msgq, q_head->writer, &ev, 0) == -1) {
      FATAL("spu_mixer: couldn't send notification\n", 0);
      exit(1);
    }
  }
  
  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;

  return cpy_len;
}




int init_spu(void)
{
  //DNOTE("spu_mixer: init\n");
  spu_info.buffer = malloc(MAX_BUF_SIZE);
  spu_info.next_buffer = malloc(MAX_BUF_SIZE);
  if(spu_info.buffer == NULL || spu_info.next_buffer == NULL) {
    ERROR("init_spu\n", 0);
    perror("malloc");
  }
  register_event_handler(handle_events);

  return 0;
}



static int get_data(uint8_t *databuf, int bufsize, 
		    uint64_t *dtime, int *scr_nr)
{
  int r;
  static int bytes_to_read = 0;
  static int state = 0;
  static int spu_size;
  /* Make sure to have a buffer bigger than this */
  /*
  if(bufsize < 2) {
    // databuf not big enough
    ERROR("buffer too small\n");
    return -1;
  }
  */

  if(bytes_to_read == 0) {
    // get first 2 bytes of spu (size of spu)
    bytes_to_read = 2;
  }
  if(state == 0) {
    while(bytes_to_read > 0) {
      r = get_q(&databuf[2-bytes_to_read], bytes_to_read, dtime, scr_nr);
      
      if(r > 0) {
	bytes_to_read -= r;
      } else if(r < 0) {
	perror("read");
	return -1;
      } else if(r == 0) {
	/* no more elements in q at this moment */
	//DNOTE("q empty, %d bytes read\n", 2-bytes_to_read);
	return 0;
      }
    }
    
    spu_size = (databuf[0]<<8) | databuf[1];
    
    if(spu_size > bufsize) {
      // databuf not big enough
      ERROR("buffer too small\n", 0);
      return -1;
    }
    
    // already read the first two bytes
    bytes_to_read = spu_size - 2;
    
    state = 1;
  }

  // get the rest of the spu
  while(bytes_to_read > 0) {
    r = get_q(&databuf[spu_size-bytes_to_read], bytes_to_read, dtime, scr_nr);
    
    if(r > 0) {
      bytes_to_read -= r;
    } else if(r < 0) {
      perror("read");
      return -1;
    } else  {
      /* no more elements in q at this time */
      /*
	DNOTE("q empty, %d of %d bytes read\n",
	      spu_size-bytes_to_read, spu_size);
      */
      return 0;
    }   
  }
  bytes_to_read = 0;
  state = 0;
  
  return spu_size;
}



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

  if(num > 4)
    ERROR("spu_mixer: getbytes used with more than 4 bytes\n", 0);

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



static void decode_dcsqt(spu_handle_t *spu_info)
{
  // init start position
  set_byte(spu_info->buffer);
  
  /* Reset state  */
  //DNOTE("spu: reset 1\n");
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
}


static void decode_dcsq(spu_handle_t *spu_info)
{ 
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
    case 0x00: /* forced display start (often a menu) */
      DPRINTF(3, "\t\t\t\tforced display start\n");
      spu_info->display_start = 1; // spu_info->menu = 1;
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
	  FATAL("unknown cmd 07 type\n", 0);
	  exit(1);
	}
	  
	if((u1 != 0) || (u2 != 0x0fff) || (u3 != 0xffff)) {
	  FATAL("unknown bits in cmd 7 used\n", 0);
	  exit(1);
	}
      }
      break;
    default:
      ERROR("\t\t\t\t*Unknown Display Control Command: %02x\n",
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

static void display_mix_function_bgr16(uint32_t color, uint32_t contrast, 
				       unsigned int length, uint16_t *pixel)
{
  uint32_t invcontrast = 256 - contrast;
  int n;
  
  /* if total transparency do nothing */
  if(contrast != 0) {
    unsigned int r, g, b;
    r = color & 0xff;
    g = (color >> 8) & 0xff;
    b = (color >> 16) & 0xff;
    
    /* if no transparancy just overwrite */
    if(contrast == (0xf<<4)) {
      uint16_t color2 = ((b << 8) & 0xf800) | ((g << 3) & 0x0770) | (r >> 3);
      for(n = 0; n < length; n++, pixel++) {
	*pixel = color2;
      }
    } else {
      for(n = 0; n < length; n++, pixel++) {
	unsigned int pr, pg, pb;
	uint16_t source = *pixel;
	pr = (source & 0x001f) << 3;
	pg = (source & 0x0770) >> 3; // or 0x037 >> 2, 0x076 >> 3 ???
	pb = (source & 0xf800) >> 8; // (source >> 8) & 0xf8;
	
	pr = (pr * invcontrast + r * contrast) >> 8;
	pg = (pg * invcontrast + g * contrast) >> 8;
	pb = (pb * invcontrast + b * contrast) >> 8;
	
	*pixel = ((pb << 8) & 0xf800) | ((pg << 3) & 0x0770) | (pr >> 3);
      }
    }
  }
}

static void display_mix_function_bgr24(uint32_t color, uint32_t contrast, 
				       unsigned int length, uint8_t *pixel)
{
  uint32_t invcontrast = 256 - contrast;
  int n;
  
  /* if total transparency do nothing */
  if(contrast != 0) {
    unsigned int r, g, b;
    r = color & 0xff;
    g = (color >> 8) & 0xff;
    b = (color >> 16) & 0xff;
    
    /* if no transparancy just overwrite */
    if(contrast == (0xf<<4)) {
      for(n = 0; n < length; n++, pixel++) {
	*pixel++ = b;
	*pixel++ = g;
	*pixel   = r;
      }
    } else {
      for(n = 0; n < length; n++, pixel++) {
	unsigned int pr, pg, pb;
	pb = pixel[0];
	pg = pixel[1];
	pr = pixel[2];
	
	pr = (pr * invcontrast + r * contrast) >> 8;
	pg = (pg * invcontrast + g * contrast) >> 8;
	pb = (pb * invcontrast + b * contrast) >> 8;
	
	*pixel++ = pb; *pixel++ = pg; *pixel = pr;
      }
    }
  }
}

static void display_mix_function_bgr32(uint32_t color, uint32_t contrast, 
				       unsigned int length, uint32_t *pixel)
{
  uint32_t invcontrast = 256 - contrast;
  int n;
  
  /* if total transparency do nothing */
  if(contrast != 0) {
    unsigned int r, g, b;
    r = color & 0xff;
    g = (color >> 8) & 0xff;
    b = (color >> 16) & 0xff;
    
    /* if no transparancy just overwrite */
    if(contrast == (0xf<<4)) {
      for(n = 0; n < length; n++, pixel++) {
	*pixel = color;
      }
    } else {
      for(n = 0; n < length; n++, pixel++) {
	unsigned int pr, pg, pb;
	uint32_t source = *pixel;
	pr = source & 0xff;
	pg = (source >> 8) & 0xff;
	pb = (source >> 16) & 0xff;
	
	pr = (pr * invcontrast + r * contrast) >> 8;
	pg = (pg * invcontrast + g * contrast) >> 8;
	pb = (pb * invcontrast + b * contrast) >> 8;
	
	*pixel = (pb << 16) | (pg << 8) | pr;
      }
    }
  }
}

/* Mixing function for YV12 mode */
static void display_mix_function_yuv(uint32_t color, uint32_t contrast, 
				     unsigned int length, uint8_t *y_pixel, 
				     int field, 
				     uint8_t *u_pixel, uint8_t *v_pixel)
{
  uint32_t invcontrast = 256 - contrast;
  int n;
  
  /* if total transparency do nothing */
  if(contrast != 0) {
    uint8_t y, u, v;
    
    y = (color >> 16) & 0xff;
    u = (color >> 8) & 0xff;
    v = color & 0xff;
    
    /* if no transparancy just overwrite */
    if(contrast == (0xf<<4)) {
      for(n = 0; n < length; n++) {
	y_pixel[n] = y;
      }
      /* only write uv on even columns and rows */
      if(!field) {
	int odd = (int)y_pixel & 1;
	for(n = odd; n < (length + 1 - odd)/2; n++) {
	  u_pixel[n] = u;
	  v_pixel[n] = v;
	}
      } /* (length+even)/2
	  10 och 1 (så 1, 10/2 -> 5 och blend på 5
	  11 och 1 (så 0, inte als
	  10 och 2 (så 1, 10/2 -> 5 och blend på 5
	  11 och 2 (så 1, men 11/2 -> 5 och blenda på 6
	  10 och 3 (så 2, 10/2 -> 5 och blend på 5,6
	  11 och 3 (så 1, men 11/2 -> 5 och blenda på 6
	  10 och 4 (så 2, 10/2 -> 5 och blend på 5,6
	  11 och 4 (så 2, men 11/2 -> 5 och blenda på 6,7
	  
	  10 och 1 (så 0,1, 10/2 -> 5 och blend på 5
	  11 och 1 (så 1,0, inte als
	  10 och 2 (så 0,1, 10/2 -> 5 och blend på 5
	  11 och 2 (så 1,1, men 11/2 -> 5 och blenda på 6
	  10 och 3 (så 0,2, 10/2 -> 5 och blend på 5,6
	  11 och 3 (så 1,1, men 11/2 -> 5 och blenda på 6
	  10 och 4 (så 0,2, 10/2 -> 5 och blend på 5,6
	  11 och 4 (så 1,2, men 11/2 -> 5 och blenda på 6,7
	*/
    } else {
      for(n = 0; n < length; n++) {
	uint32_t py;
	py = y_pixel[n];
	py = (py * invcontrast + y * contrast) >> 8;
	y_pixel[n] = py;
      }
      /* only write uv on even columns and rows */
      if(!field) {
	int odd = (int)y_pixel & 1;
	for(n = odd; n < (length + 1 - odd)/2; n++) {
	  uint32_t pu, pv;
	  pu = u_pixel[n];
	  pu = (pu * invcontrast + u * contrast) >> 8;
	  u_pixel[n] = pu;
	  pv = v_pixel[n];
	  pv = (pv * invcontrast + v * contrast) >> 8;
	  v_pixel[n] = pv;
	}
      }
    }    
  }
}


static void decode_display_data(spu_handle_t *spu_info, char *data, 
				int pixel_stride, int line_stride,
				int blendyuv, int image_stride) 
{
  unsigned int x;
  unsigned int y;

  fieldoffset[0] = spu_info->fieldoffset[0];
  fieldoffset[1] = spu_info->fieldoffset[1];
  field = 0;
  aligned = 1;
  set_byte(&spu_info->buffer[fieldoffset[field]]);
  
  //DNOTE("\nDecoding overlay picture data\n");
  
  //initialize(spu_info->width, spu_info->height);
  x = 0;
  y = 0;

  DPRINTF(5, "vlc decoding\n");
  while((fieldoffset[1] < spu_info->DCSQT_offset) && (y < spu_info->height)) {
    unsigned int vlc;
    unsigned int length;
    unsigned int colorid;
    uint32_t color;
    uint32_t contrast;
    
    /*
    DPRINTF(6, "fieldoffset[0]: %d, fieldoffset[1]: %d, DCSQT_offset: %d\n",
	    fieldoffset[0], fieldoffset[1], DCSQT_offset);
    */
    
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
      len: 64 - 255, 0 (to end of this line)
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
    
    DPRINTF(4, "send_rle: %08x\n", vlc);
    /* last two bits are colorid, the rest are run length */
    length = vlc >> 2;
    
    if(length == 0) {
      /* Fill current line with background color */
      length = spu_info->width - x;
    }
    if(length+x > spu_info->width) {
      WARNING("tried to write past line-end\n", 0);
      length = spu_info->width - x;
    }
    
    colorid = vlc & 3;
    
    if(spu_info->has_highlight
       && (y+spu_info->y_start >= highlight.y_start &&
	   y+spu_info->y_start <= highlight.y_end &&
	   x+spu_info->x_start + length >= highlight.x_start &&
	   x+spu_info->x_start <= highlight.x_end)) {
      color = highlight.color[colorid];
      contrast = highlight.contrast[colorid]<<4;
    } else {
      color = spu_info->color[colorid];
      contrast = spu_info->contrast[colorid]<<4;
    }
    
    /* mix spu and picture data */

    /* Only blend if not totally transparent. */
    if(contrast != 0) {
      unsigned int line_y;
      char *addr;
      
      line_y = (y + spu_info->y_start) * line_stride;
      // (width * bpp) == line_stride (for rgb or yuv)
      addr = data + line_y + (x + spu_info->x_start) * pixel_stride;
    
      if(!blendyuv) {
	/* Change this to call though a function pointer.. ? */
	switch(pixel_stride) {
	case 1: 
	case 2:
	  display_mix_function_bgr16(palette_rgb[color], 
				     contrast, length, (uint16_t *)addr);
	  break;
	case 3:
	  // Not supported yet.
	  display_mix_function_bgr24(palette_rgb[color], 
				     contrast, length, (uint8_t *)addr);
	  break;
	case 4:
	  display_mix_function_bgr32(palette_rgb[color], 
				     contrast, length, (uint32_t *)addr);
	  break;
	}
      } else {
	char *addr_u, *addr_v;
	// bpp == 1
	// line_uv == line_y/4 ?
	// yes if we make sure to only use the info for even lines..
	// data_u = data + (width * bpp) * height;
	// data_v = data + (width * bpp) * height * 5/4;
	// (width * bpp) == yuv_line_stride
	addr_u = data + image_stride + line_y/4 + (x + spu_info->x_start) / 2;
	addr_v = addr_u + image_stride / 4;
	// sinc bpp==1 addr1 will be even/odd for even/odd pixels..
	// for even/odd lines we still need the field variable
	display_mix_function_yuv(palette_yuv[color], contrast, length, addr, 
				 field, addr_u, addr_v);       
      }
    }
    
    x = x + length;
    
    if(x >= spu_info->width) {
      x = 0;
      y++;
      field = 1 - field;
      set_byte(&spu_info->buffer[fieldoffset[field]]);
    }
  }
}  


/*
 * Return 'true' if it is time for the next command sequence to 
 * be 'executed'. If no next sequence exist or if it is in the  
 * future, return 'false'.
 */

/* TODO
 * 
 *
 *
 *
 */
static int next_spu_cmd_pending(spu_handle_t *spu_info)
{
  int start_time, offset;
  clocktime_t realtime, errtime, next_time;

  /* If next_time haven't been set, try to set it. */
  if(spu_info->next_time == 0) {
    
    if(spu_info->next_DCSQ_offset == spu_info->last_DCSQ) {
      
      if(!get_data(spu_info->next_buffer, MAX_BUF_SIZE, 
		   &spu_info->base_time, &spu_info->scr_nr)) {
	if(flush_to_scrid != -1) {
	  // FIXME: assumes order of src_nr
	  if(ctrl_time[spu_info->scr_nr].scr_id < flush_to_scrid) {
	    /* Reset state  */
	    //DNOTE("spu: flush/reset 2\n");
	    spu_info->display_start = 0;
	    spu_info->has_highlight = 0;
	  }
	}
	return 0;
      }
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
    /* starttime measured in 1024/90000 seconds */
    //   PTS_TO_CLOCKTIME(spu_info->next_time, 1024 * start_time);
    spu_info->next_time = spu_info->base_time + start_time * 1024;
    //   timeadd(&spu_info->next_time, &spu_info->base_time, &spu_info->next_time);
  }

  if(flush_to_scrid != -1) {
    if(ctrl_time[spu_info->scr_nr].scr_id < flush_to_scrid) {

      
      /* Reset state  */
      //DNOTE("spu: flush/reset 3\n");
      spu_info->display_start = 0;
      spu_info->has_highlight = 0;
      
      return 1;
    } else {
      flush_to_scrid = -1;
    }
  }

  if(spu_info->scr_nr < video_scr_nr) { // FIXME: assumes order of src_nr
    //DNOTE("spu: spu_info->scr_nr != video_scr_nr\n");
    return 1;
  }

  if(ctrl_time[spu_info->scr_nr].offset_valid == OFFSET_NOT_VALID) {
    //DNOTE("scr_nr: %d, OFFSET_NOT_VALID\n", spu_info->scr_nr);
    return 0;
  }
  PTS_TO_CLOCKTIME(next_time, spu_info->next_time);
  clocktime_get(&realtime);
  calc_realtime_left_to_scrtime(&errtime, &realtime, &next_time,
				&ctrl_time[spu_info->scr_nr].sync_point);
  //  timesub(&errtime, &spu_info->next_time, &realtime);

  if(TIME_SS(errtime) < 0 || TIME_S(errtime) < 0) {
    //DNOTE("spu: errtime: %d.%09d\n",TIME_S(errtime),TIME_SS(errtime));
    return 1;
  }
  
  return 0;
}

void mix_subpicture_rgb(char *data, int width, int height, int pixel_stride)
{
  /*
   * Check for, and execute all pending spu command sequences.
   * If there then is an active overlay, call the actual mix function.
   */

  //ugly hack
  if(!initialized) {
    return;
  }
  
  while(next_spu_cmd_pending(&spu_info)) {

    if(spu_info.next_DCSQ_offset == spu_info.last_DCSQ) {
      unsigned char *swap;
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
    spu_info.next_time = 0;
  }


  if(spu_info.display_start /* || spu_info.menu */) {
    decode_display_data(&spu_info, data, pixel_stride, pixel_stride*width, 
			0, pixel_stride*width*height);
  }
}

int mix_subpicture_yuv(yuv_image_t *img, yuv_image_t *reserv)
{
  /*
   * Check for, and execute all pending spu command sequences.
   * If there then is an active overlay call the acutal mix function.
   */

  //ugly hack
  if(!initialized) {
    return 0;
  }
  
  while(next_spu_cmd_pending(&spu_info)) {

    if(spu_info.next_DCSQ_offset == spu_info.last_DCSQ) {
      unsigned char *swap;
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
    spu_info.next_time = 0;
  }


  if(spu_info.display_start /* || spu_info.menu */) {
    int width = img->info->picture.padded_width;
    int height = img->info->picture.padded_height;
    //DNOTE("decoding data\n");
    //ugly hack
    if(img->info->is_reference) {
      int size;
      size = width * height;
      memcpy(reserv->y, img->y, size);
      memcpy(reserv->u, img->u, size/4);
      memcpy(reserv->v, img->v, size/4);
      decode_display_data(&spu_info, reserv->y, 1, width, 1, width*height);
      return 1;
    } else {
      decode_display_data(&spu_info, img->y, 1, width, 1, width*height);
      return 0;
    }
  }
  return 0;
}



void flush_subpicture(int scr_id)
{
  /*
   * Check for, and execute all pending spu command sequences.
   */

  //ugly hack
  if(!initialized) {
    return;
  }
  
  //DNOTE("spu: flush_subpicture to scr_id: %d\n", scr_id);
  flush_to_scrid = scr_id;

  while(next_spu_cmd_pending(&spu_info)) {

    if(spu_info.next_DCSQ_offset == spu_info.last_DCSQ) {
      unsigned char *swap;
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
    spu_info.next_time = 0;
  }
}
