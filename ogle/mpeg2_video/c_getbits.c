
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "c_getbits.h"
#include "../include/common.h"




FILE *infile;

#ifdef GETBITSMMAP // Mmap i/o
uint32_t *buf;
uint32_t buf_size;
struct off_len_packet packet;
uint8_t *mmap_base;

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
  if (rv == -1) {
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

void get_next_packet()
{
  struct { 
    uint32_t type;
    uint32_t offset;
    uint32_t length;
  } ol_packet;
  
  fread(&ol_packet, 12, 1, infile);
  
  if(ol_packet.type == PACK_TYPE_OFF_LEN) {
    
    packet.offset = ol_packet.offset;
    packet.length = ol_packet.length;

    return;
  }
  else if( ol_packet.type == PACK_TYPE_LOAD_FILE ) {
    char filename[200];
    int length = ol_packet.offset;           // Use lots of trickery here...
    memcpy( filename, &ol_packet.length, 4);
    fread(filename+4, length-4, 1, infile);
    filename[length] = 0;

    setup_mmap( filename );
  }  
}  

void read_buf()
{
  uint8_t *packet_base = &mmap_base[packet.offset];
  // How many bytes are there left? (0, 1, 2 or 3).
  int end_bytes = &packet_base[packet.length] - (uint8_t *)&buf[buf_size];
  int i = 0;
  
  // Read them, as we have at least 32 bits free they will fit.
  while( i < end_bytes ) {
    cur_word=(cur_word << 8) | packet_base[packet.length - end_bytes + i++];
    //cur_word=cur_word|(((uint64_t)packet_base[packet.length-end_bytes+i++])<<(56-bits_left)); //+
    bits_left += 8;
  }
   
  // If we have enough 'free' bits so that we always can align
  // the buff[] pointer to a 4 byte boundary. 
  if( bits_left <= 40 ) {
    int start_bytes;
    get_next_packet(); // Get new packet struct
    packet_base = &mmap_base[packet.offset];
    // How many bytes to the next 4 byte boundary? (0, 1, 2 or 3).
    start_bytes = (4 - ((long)packet_base % 4)) % 4; 
    i = 0;
    
    // Read them, as we have at least 24 bits free they will fit.
    while( i < start_bytes ) {
      cur_word  = (cur_word << 8) | packet_base[i++];
      //cur_word=cur_word|(((uint64_t)packet_base[i++])<<(56-bits_left)); //+
      bits_left += 8;
    }
     
    buf = (uint32_t *)&packet_base[start_bytes];
    buf_size = (packet.length - start_bytes) / 4;// number of 32 bit words
    offs = 0;

    if(bits_left <= 32) {
      uint32_t new_word = GUINT32_FROM_BE(buf[offs++]);
      cur_word = (cur_word << 32) | new_word;
      //cur_word = cur_word | (((uint64_t)new_word) << (32-bits_left)); //+
      bits_left += 32;
    }
  } else {
    // The trick!! 
    // We have enough data to return. Infact it's so much data that we 
    // can't be certain that we can read enough of the next packet to 
    // align the buff[ ] pointer to a 4 byte boundary.
    // Fake it so that we still are at the end of the packet but make
    // sure that we don't read the last bytes again.
    
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


