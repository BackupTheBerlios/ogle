

#ifndef HAVE_C_GETBITS_H
#define HAVE_C_GETBITS_H

#include <inttypes.h>

#include <glib.h> // Only for GUINT32_FROM_BE(...)

#include "video_types.h"


//#define GETBITS32
#define GETBITS64
#define GETBITSMMAP


#ifdef GETBITSMMAP // Mmap i/o
void setup_mmap(char *);
void get_next_packet();
extern uint32_t *buf;
extern uint32_t buf_size;
extern struct off_len_packet packet;
extern uint8_t *mmap_base;

#else // Normal i/o
#define READ_SIZE 1024*8
#define ALLOC_SIZE 2048
#define BUF_SIZE_MAX 1024*8
extern uint32_t buf[];

#endif // Common

extern FILE *infile;


#ifdef GETBITS32
void back_word(void);
void next_word(void);
extern unsigned int backed;
extern unsigned int buf_pos;
extern unsigned int bits_left;
extern uint32_t cur_word;
#endif

#ifdef GETBITS64
void read_buf(void);
extern int offs;
extern unsigned int bits_left;
extern uint64_t cur_word;
#endif



#ifdef DEBUG
#define GETBITS(a,b) getbits(a,b)
#else
#define GETBITS(a,b) getbits(a)
#endif

#ifdef STATS
extern uint64_t stats_bits_read;
#endif

/* ---------------------------------------------------------------------- */
#ifdef GETBITS64
#ifdef GETBITSMMAP // (64bit) Discontinuous buffers of variable size


/* 5.2.2 Definition of nextbits() function */
static inline
uint32_t nextbits(unsigned int nr)
{
  uint32_t result = (cur_word << (64-bits_left)) >> 32;
  return result >> (32-nr);
  //  return (cur_word << (64-bits_left)) >> (64-nr);
}

#ifdef DEBUG
static inline
uint32_t getbits(unsigned int nr, char *func)
#else
static inline
uint32_t getbits(unsigned int nr)
#endif
{
  uint32_t result;
#ifdef STATS
  stats_bits_read+=nr;
#endif
  result = (cur_word << (64-bits_left)) >> 32;
  result = result >> (32-nr);
  //  result = cur_word >> (64-nr); //+
  //  cur_word = cur_word << nr; //+
  
  bits_left -= nr;
  if(bits_left <= 32) {
    if(offs >= buf_size)
      read_buf();
    else {
      uint32_t new_word = GUINT32_FROM_BE(buf[offs++]);
      cur_word = (cur_word << 32) | new_word;
      //cur_word = cur_word | (((uint64_t)new_word) << (32-bits_left)); //+
      bits_left += 32;
    }
  }
  DPRINTF(5, "%s getbits(%u): %x, ", func, nr, result);
  DPRINTBITS(6, nr, result);
  DPRINTF(5, "\n");
  return result;
}


static inline
void dropbits(unsigned int nr)
{
#ifdef STATS
  stats_bits_read+=nr;
#endif
  //cur_word = cur_word << nr; //+
  bits_left -= nr;
  if(bits_left <= 32) {
    if(offs >= buf_size)
      read_buf();
    else {
      uint32_t new_word = GUINT32_FROM_BE(buf[offs++]);
      cur_word = (cur_word << 32) | new_word;
      //cur_word = cur_word | (((uint64_t)new_word) << (32-bits_left)); //+
      bits_left += 32;
    }
  }
}

#endif
#endif



/* ---------------------------------------------------------------------- */
#ifdef GETBITS64
#ifndef GETBITSMMAP // (64bit) Discontinuous buffers of *static* size


/* 5.2.2 Definition of nextbits() function */
static inline
uint32_t nextbits(unsigned int nr)
{
  uint32_t result = (cur_word << (64-bits_left)) >> 32;
  return result >> (32-nr);
}

#ifdef DEBUG
static inline
uint32_t getbits(unsigned int nr, char *func)
#else
static inline
uint32_t getbits(unsigned int nr)
#endif
{
  uint32_t result;
#ifdef STATS
  stats_bits_read+=nr;
#endif
  result = (cur_word << (64-bits_left)) >> 32;
  result = result >> (32-nr);
  bits_left -=nr;
  if(bits_left <= 32) {
    uint32_t new_word = GUINT_FROM_BE(buf[offs++]);
    if(offs >= READ_SIZE/4)
      read_buf();
    cur_word = (cur_word << 32) | new_word;
    bits_left += 32;
  }
  DPRINTF(5, "%s getbits(%u): %x, ", func, nr, result);
  DPRINTBITS(6, nr, result);
  DPRINTF(5, "\n");
  return result;
}


static inline
void dropbits(unsigned int nr)
{
#ifdef STATS
  stats_bits_read+=nr;
#endif
  bits_left -= nr;
  if(bits_left <= 32) {
    uint32_t new_word = GUINT32_FROM_BE(buf[offs++]);
    if(offs >= READ_SIZE/4)
      read_buf();
    cur_word = (cur_word << 32) | new_word;
    bits_left += 32;
  }
}

#endif
#endif


/* ---------------------------------------------------------------------- */
#ifdef GETBITS32  // (32bit) 'Normal' getbits, word based.


#ifdef DEBUG
static inline
uint32_t getbits(unsigned int nr, char *func)
#else
static inline
uint32_t getbits(unsigned int nr)
#endif
{
  uint32_t result;
  uint32_t rem;
#ifdef STATS
  stats_bits_read+=nr;
#endif
  if(nr <= bits_left) {
    result = (cur_word << (32-bits_left)) >> (32-nr);
    bits_left -=nr;
    if(bits_left == 0) {
      next_word();
      bits_left = 32;
    } 
  } else {
    rem = nr-bits_left;
    result = ((cur_word << (32-bits_left)) >> (32-bits_left)) << rem;

    next_word();
    bits_left = 32;
    result |= ((cur_word << (32-bits_left)) >> (32-rem));
    bits_left -=rem;
    if(bits_left == 0) {
      next_word();
      bits_left = 32;
    }
  }
  DPRINTF(5,"%s getbits(%u): %x ", func, nr, result);
  DPRINTBITS(6, nr, result);
  DPRINTF(5, "\n");
  return result;
}


static inline
void dropbits(unsigned int nr)
{
#ifdef STATS
  stats_bits_read+=nr;
#endif
  bits_left -= nr;
  if(bits_left <= 0) {
    next_word();
    bits_left += 32;
  } 
}


/* 5.2.2 Definition of nextbits() function */
static inline
uint32_t nextbits(unsigned int nr)
{
  uint32_t result;
  uint32_t rem;
  unsigned int t_bits_left = bits_left;

  if(nr <= t_bits_left) {
    result = (cur_word << (32-t_bits_left)) >> (32-nr);
  } else {
    rem = nr-t_bits_left;
    result = ((cur_word << (32-t_bits_left)) >> (32-t_bits_left)) << rem;
    t_bits_left = 32;
    next_word();
    result |= ((cur_word << (32-t_bits_left)) >> (32-rem));
    back_word();
  }
  return result;
}
#endif


/* ---------------------------------------------------------------------- */






/* 5.2.1 Definition of bytealigned() function */
static inline
int bytealigned(void)
{
  return !(bits_left%8);
}

#ifdef DEBUG
static inline
void marker_bit(void)
{
  if(!GETBITS(1, "markerbit")) {
    fprintf(stderr, "*** incorrect marker_bit in stream\n");
    exit_program(-1);
  }
}
#else // DEBUG
static inline
void marker_bit(void)
{
  dropbits(1);
}
#endif //DEBUG

#endif
