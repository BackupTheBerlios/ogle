#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <inttypes.h>

#include "video_stream.h"

#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>
#include <mlib_algebra.h>


#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "../include/common.h"

int dctstat[128];
int total_pos = 0;
int total_calls = 0;


static int shmem_flag = 1;
static XVisualInfo vinfo;
XShmSegmentInfo shm_info;
XShmSegmentInfo shm_info_ref1;
XShmSegmentInfo shm_info_ref2;

unsigned char *ImageData;
unsigned char *imagedata_ref1;
unsigned char *imagedata_ref2;
XImage *myximage;
XImage *ximage_ref1;
XImage *ximage_ref2;
Display *mydisplay;
Window mywindow;
Window window_ref1;
Window window_ref2;
Window window_stat;


double time_pic[4] = { 0.0, 0.0, 0.0, 0.0};
double time_max[4] = { 0.0, 0.0, 0.0, 0.0};
double time_min[4] = { 1.0, 1.0, 1.0, 1.0};
double num_pic[4] = { 0.0, 0.0, 0.0, 0.0};


GC mygc;
GC statgc;
int bpp, mode;
XWindowAttributes attribs;

#define READ_SIZE 1024*8
#define ALLOC_SIZE 2048
#define BUF_SIZE_MAX 1024*8

//#define DEBUG
//#define STATS


uint8_t intra_inverse_quantiser_matrix_changed = 1;

uint8_t non_intra_inverse_quantiser_matrix_changed = 1;


#ifdef STATS
uint64_t stats_bits_read = 0;
uint8_t stats_intra_inverse_quantiser_matrix_reset = 0;
uint8_t stats_intra_inverse_quantiser_matrix_loaded = 0;

uint32_t stats_intra_inverse_quantiser_matrix_changes_possible = 0;
uint32_t stats_intra_inverse_quantiser_matrix_loaded_nr = 0;
uint32_t stats_intra_inverse_quantiser_matrix_reset_nr = 0;


uint8_t stats_non_intra_inverse_quantiser_matrix_reset = 0;
uint8_t stats_non_intra_inverse_quantiser_matrix_loaded = 0;

uint32_t stats_non_intra_inverse_quantiser_matrix_changes_possible = 0;
uint32_t stats_non_intra_inverse_quantiser_matrix_loaded_nr = 0;
uint32_t stats_non_intra_inverse_quantiser_matrix_reset_nr = 0;

uint32_t stats_quantiser_scale_possible = 0;
uint32_t stats_quantiser_scale_nr = 0;

uint32_t stats_intra_quantiser_scale_possible = 0;
uint32_t stats_intra_quantiser_scale_nr = 0;

uint32_t stats_non_intra_quantiser_scale_possible = 0;
uint32_t stats_non_intra_quantiser_scale_nr = 0;

uint32_t stats_block_non_intra_nr = 0;
uint32_t stats_f_non_intra_compute_first_nr = 0;
uint32_t stats_f_non_intra_compute_subseq_nr = 0;

uint32_t stats_block_intra_nr = 0;
uint32_t stats_f_intra_compute_subseq_nr = 0;
uint32_t stats_f_intra_compute_first_nr = 0;

uint32_t stats_f_non_intra_subseq_escaped_run_nr = 0;
uint32_t stats_f_non_intra_first_escaped_run_nr = 0;

void statistics_init()
{
  
  stats_intra_inverse_quantiser_matrix_reset = 0;
  stats_intra_inverse_quantiser_matrix_loaded = 0;

  stats_intra_inverse_quantiser_matrix_changes_possible = 0;
  stats_intra_inverse_quantiser_matrix_loaded_nr = 0;
  stats_intra_inverse_quantiser_matrix_reset_nr = 0;


  non_intra_inverse_quantiser_matrix_changed = 1;
  stats_non_intra_inverse_quantiser_matrix_reset = 0;
  stats_non_intra_inverse_quantiser_matrix_loaded = 0;

  stats_non_intra_inverse_quantiser_matrix_changes_possible = 0;
  stats_non_intra_inverse_quantiser_matrix_loaded_nr = 0;
  stats_non_intra_inverse_quantiser_matrix_reset_nr = 0;

  stats_quantiser_scale_possible = 0;
  stats_quantiser_scale_nr = 0;

  stats_intra_quantiser_scale_possible = 0;
  stats_intra_quantiser_scale_nr = 0;

  stats_non_intra_quantiser_scale_possible = 0;
  stats_non_intra_quantiser_scale_nr = 0;


  stats_block_non_intra_nr = 0;
  stats_f_non_intra_compute_first_nr = 0;
  stats_f_non_intra_compute_subseq_nr = 0;

  stats_block_intra_nr = 0;
  stats_f_intra_compute_subseq_nr = 0;
  stats_f_intra_compute_first_nr = 0;

  stats_f_non_intra_subseq_escaped_run_nr = 0;
  stats_f_non_intra_first_escaped_run_nr = 0;
}

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


unsigned int debug = 0;
int show_window[3] = {1,1,1};
int show_stat = 1;
int run = 0;
//prototypes
int bytealigned(void);
void back_word(void);
void next_word(void);
void next_start_code(void);
void resync(void);
void video_sequence(void);
void marker_bit(void);
void sequence_header(void);
void sequence_extension(void);
void sequence_display_extension();
void extension_and_user_data(unsigned int i);
void picture_coding_extension(void);
void user_data(void);
void group_of_pictures_header(void);
void picture_header(void);
void picture_data(void);
void slice(void);
void macroblock(void);
void macroblock_modes(void);
void coded_block_pattern(void);
void reset_dc_dct_pred(void);
void get_dct(runlevel_t *runlevel, int first_subseq, uint8_t intra_block,
	     uint8_t intra_vlc_format, char *func);
int get_vlc(const vlc_table_t *table, char *func);
void block_intra(unsigned int i);
void block_non_intra(unsigned int i);
void reset_PMV();
void reset_vectors();
void motion_vectors(unsigned int s);
void motion_vector(int r, int s);

void reset_to_default_intra_quantiser_matrix();
void reset_to_default_non_intra_quantiser_matrix();
void reset_to_default_quantiser_matrix();
int sign(int16_t num);


void display_init();
void Display_Image(Window win, XImage *myximage, unsigned char *ImageData);
void motion_comp();

void extension_data(unsigned int i);

void frame_done();

void exit_program();

// Not implemented
void quant_matrix_extension();
void picture_display_extension();
void picture_spatial_scalable_extension();
void picture_temporal_scalable_extension();
void sequence_scalable_extension();


#define GETBITSMMAP



#ifndef GETBITSMMAP
uint32_t buf[BUF_SIZE_MAX] __attribute__ ((aligned (8)));
#else
uint32_t *buf;
uint32_t buf_size;
uint8_t *mmap_base;
struct off_len_packet packet;
#endif
unsigned int buf_len = 0;
unsigned int buf_start = 0;
unsigned int buf_fill = 0;
unsigned int bytes_read = 0;
unsigned int bit_start = 0;




FILE *infile;

//#define GETBITS32

#ifdef GETBITS32
unsigned int backed = 0;
unsigned int buf_pos = 0;
unsigned int bits_left = 32;
uint32_t cur_word = 0;
#else
unsigned int bits_left = 64;
uint64_t cur_word = 0;
#endif
//uint8_t bytealign = 1;

uint8_t new_scaled = 0;





typedef struct {
  uint16_t macroblock_type;
  uint8_t spatial_temporal_weight_code;
  uint8_t frame_motion_type;
  uint8_t field_motion_type;
  uint8_t dct_type;

  uint8_t macroblock_quant;
  uint8_t macroblock_motion_forward;
  uint8_t macroblock_motion_backward;
  uint8_t macroblock_pattern;
  uint8_t macroblock_intra;
  uint8_t spatial_temporal_weight_code_flag;
  
} macroblock_modes_t;


typedef struct {
  uint16_t macroblock_escape;
  uint16_t macroblock_address_increment;
  //  uint8_t quantiser_scale_code; // in slice_data
  macroblock_modes_t modes; 
  
  uint8_t pattern_code[12];
  uint8_t cbp;
  uint8_t coded_block_pattern_1;
  uint8_t coded_block_pattern_2;
  uint16_t dc_dct_pred[3];

  int16_t dmv;
  int16_t mv_format;
  int16_t prediction_type;
  
  int16_t dmvector[2];
  int16_t motion_code[2][2][2];
  int16_t motion_residual[2][2][2];
  int16_t vector[2][2][2];


  int16_t f[6][8*8] __attribute__ ((aligned (8)));

  //int16_t f[6][8*8]; // CC

  int8_t motion_vector_count;
  int8_t motion_vertical_field_select[2][2];

  int16_t delta[2][2][2];

  int8_t skipped;

  uint8_t quantiser_scale;
  int intra_dc_mult;

  int16_t QFS[64]  __attribute__ ((aligned (8)));
  //int16_t QF[8][8];
  //int16_t F_bis[8][8];
} macroblock_t;

macroblock_t mb;


/* image data */
typedef struct {
  mlib_u8 *y; //[480][720];  //y-component image
  mlib_u8 *u; //[480/2][720/2]; //u-component
  mlib_u8 *v; //[480/2][720/2]; //v-component
} yuv_image_t;




yuv_image_t r1_img;
yuv_image_t r2_img;
yuv_image_t b_img;

yuv_image_t *ref_image1;
yuv_image_t *ref_image2;
yuv_image_t *dst_image;
yuv_image_t *b_image;
yuv_image_t *current_image;





/*macroblock*/
mlib_u8 abgr[16*16*4];
mlib_u8 y_blocks[64*4];
mlib_u8 u_block[64];
mlib_u8 v_block[64];






macroblock_t *cur_mbs;
macroblock_t *ref1_mbs;
macroblock_t *ref2_mbs;





XFontStruct *xfont;

void read_buf(void);



#define NR_BLOCKS 64
#define BLOCK_SIZE 1024
static int offs = 0;

void fprintbits(FILE *fp, unsigned int bits, uint32_t value)
{
  int n;
  for(n = 0; n < bits; n++) {
    fprintf(fp, "%u", (value>>(bits-n-1)) & 0x1);
  }
}
  

/* 5.2.1 Definition of bytealigned() function */
int bytealigned(void)
{
  return !(bits_left%8);
}

#ifdef DEBUG
uint32_t getbits(unsigned int nr, char *func)
#else
static inline   
uint32_t getbits(unsigned int nr)
#endif
#ifndef GETBITS32
#ifdef GETBITSMMAP
{
  uint32_t result;
#ifdef STATS
  stats_bits_read+=nr;
#endif
  result = (cur_word << (64-bits_left)) >> 32;
  result = result >> (32-nr);
  bits_left -=nr;
  if(bits_left <= 32) {
    if(offs >= buf_size)
      read_buf();
    else {
      uint32_t new_word = buf[offs++];
      cur_word = (cur_word << 32) | new_word;
      bits_left += 32;
    }
  }
  return result;
}
#else
{
  uint32_t result;
#ifdef STATS
  stats_bits_read+=nr;
#endif
  result = (cur_word << (64-bits_left)) >> 32;
  result = result >> (32-nr);
  bits_left -=nr;
  if(bits_left <= 32) {
    uint32_t new_word = buf[offs++];
    if(offs >= READ_SIZE/4)
      read_buf();
    cur_word = (cur_word << 32) | new_word;
    bits_left = bits_left+32;
  }
  return result;
}
#endif
#else
{
  uint32_t result;
  uint32_t rem;

  if(nr <= bits_left) {
    result = (cur_word << (32-bits_left)) >> (32-nr);
    bits_left -=nr;
    if(bits_left == 0) {
      next_word();
      bits_left = 32;
    } 
    //    bytealign = !(bits_left%8);
    DPRINTF(5, "%s getbits(%u): %x, ", func, nr, result);
    DPRINTBITS(6, nr, result);
    DPRINTF(5, "\n");
    return result;
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
    //    bytealign = !(bits_left%8);
    DPRINTF(5,"%s getbits(%u): %x ", func, nr, result);
    DPRINTBITS(6, nr, result);
    DPRINTF(5, "\n");
    return result;
  }
}
#endif


//static inline   
void dropbits(unsigned int nr)
#ifndef GETBITS32
#ifdef GETBITSMMAP
{
  bits_left -= nr;
  if(bits_left <= 32) {
    if(offs >= buf_size)
      read_buf();
    else {
      uint32_t new_word = buf[offs++];
      cur_word = (cur_word << 32) | new_word;
      bits_left += 32;
    }
  }
}
#else
{
  bits_left -= nr;
  if(bits_left <= 32) {
    uint32_t new_word = buf[offs++];
    if(offs >= READ_SIZE/4)
      read_buf();
    cur_word = (cur_word << 32) | new_word;
    bits_left += 32;
  }
}
#endif
#else
{
  bits_left -= nr;
  if(bits_left <= 0) {
    next_word();
    bits_left += 32;
  } 
}
#endif


#ifdef GETBITS32
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
#else


#ifdef GETBITSMMAP
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
  rv = madvise(mmap_base, statbuf.st_size, MADV_SEQUENTIAL);
  if(rv == -1) {
    perror("madvise");
    exit(1);
  }
  DPRINTF(1, "All mmap systems ok!\n");
}

void get_next_packet()
{
  uint32_t packet_type;
  struct off_len_packet ol_packet;
  
  fread(&packet_type, 4, 1, infile);
  
  if(packet_type == PACK_TYPE_OFF_LEN) {
    fread(&ol_packet, 8, 1, infile);
  
    packet.offset = ol_packet.offset;
    packet.length = ol_packet.length;

    return;
  }
  else if( packet_type == PACK_TYPE_LOAD_FILE ) {
    uint32_t length;
    char filename[200];
    
    fread(&length, 4, 1, infile);
    fread(&filename, length, 1, infile);
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
#ifdef DEBUG
  DPRINTF(0, "-- read_buf --\n");
  if( (64 - bits_left) < 32 )
    DPRINTF(0, "read_buf: assert 1 failed\n");
  if( end_bytes < 0 )
    DPRINTF(0, "read_buf: assert 5 failed\n");
#endif
  
  // Read them, as we have at least 32 bits free they will fit.
  while( i < end_bytes ) {
    cur_word 
      = (cur_word << 8) | packet_base[packet.length - end_bytes + i++];
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
    
#ifdef DEBUG
  if( (64 - bits_left) < 24 )
    DPRINTF(0, "read_buf: assert 2 failed\n");
#endif
    
    // Read them, as we have at least 24 bits free they will fit.
    while( i < start_bytes ) {
      cur_word 
        = (cur_word << 8) | packet_base[i++];
      bits_left += 8;
    }
     
    buf = (uint32_t *)&packet_base[start_bytes];
    buf_size = (packet.length - start_bytes) / 4;// number of 32 bit words
    offs = 0;

#ifdef DEBUG
  if( ((long)buf & 0x3) != 0 )
    DPRINTF(0, "read_buf: assert 3 failed\n");
  if( (uint8_t *)&buf[buf_size] < &packet_base[packet.length] )
    DPRINTF(0, "read_buf: assert 6 failed\n");
#endif
     
    if(bits_left <= 32) {
      uint32_t new_word = buf[offs++];
      cur_word = (cur_word << 32) | new_word;
      bits_left += 32;
    }
  } else {
    // The trick!! 
    // We have enough data to return. Infact it's so much data that we 
    // can't be certain that we can read enough of the next packet to 
    // align the buff[ ] pointer to a 4 byte boundary.
    // Fake it so that we still are at the end of the packet but make
    // sure that we don't read the last bytes again.
    
#ifdef DEBUG
    if( bits_left >= 32 )
      DPRINTF(0, "read_buf: assert 4 failed\n");
#endif
    DPRINTF(0, "Hepp! Stora buffertricket.\n" );
    
    packet.length -= end_bytes;
  }

}
#else

void read_buf()
{
  if(!fread(&buf[0], READ_SIZE, 1 , infile)) {
    if(feof(infile)) {
      fprintf(stderr, "*End Of File\n");
      exit_program();
    } else {
      fprintf(stderr, "**File Error\n");
      exit(0);
    }
  }
  offs = 0;
}

#endif
#endif

#ifdef GETBITS32
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
	exit_program();
      } else {
	fprintf(stderr, "**File Error\n");
	exit(0);
      }
    }
  } else {
    backed = 0;
  }
  cur_word = buf[buf_pos];

}
#endif


/* 5.2.2 Definition of nextbits() function */
static inline
uint32_t nextbits(unsigned int nr)
#ifndef GETBITS32
{
  uint32_t result = (cur_word << (64-bits_left)) >> 32;
  return result >> (32-nr);
}
#else
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


#ifdef DEBUG
#define GETBITS(a,b) getbits(a,b)
#else
#define GETBITS(a,b) getbits(a)
#endif

int get_vlc(const vlc_table_t *table, char *func) {
  int pos=0;
  int numberofbits=1;
  int vlc;
  
  while(table[pos].numberofbits != VLC_FAIL) {
    vlc=nextbits(numberofbits);
    while(table[pos].numberofbits == numberofbits) {
      if(table[pos].vlc == vlc) {
	DPRINTF(3, "%s ", func);
	dropbits(numberofbits);
	return (table[pos].value);
      }
      pos++;
    }
    numberofbits++;
  }
  fprintf(stderr, "*** get_vlc(vlc_table *table): no matching bitstream found.\nnext 32 bits: %08x, ", nextbits(32));
  fprintbits(stderr, 32, nextbits(32));
  fprintf(stderr, "\n");
  exit(-1);
  return VLC_FAIL;
}


void program_init()
{
  
  cur_mbs = malloc(36*45*sizeof(macroblock_t));
  ref1_mbs = malloc(36*45*sizeof(macroblock_t));
  ref2_mbs = malloc(36*45*sizeof(macroblock_t));
  
  
  ref_image1 = &r1_img;
  ref_image2 = &r2_img;
  b_image = &b_img;
  display_init();

#ifdef STATS
  statistics_init();
#endif
  return;
}
int main(int argc, char **argv)
{
  int c;

  /* Parse command line options */
  while ((c = getopt(argc, argv, "d:hs")) != EOF) {
    switch (c) {
    case 'd':
      debug = atoi(optarg);
      break;
    case 's':
      shmem_flag = 0;
      break;
    case 'h':
    case '?':
      printf ("Usage: %s [-d <level] [-s]\n\n"
	      "  -d <level> set debug level (default 0)\n"
	      "  -s         disable shared memory\n", 
	      argv[0]);
      return 1;
    }
  }
  if(optind < argc) {
    infile = fopen(argv[optind], "r");
  } else {
    infile = stdin;
  }

  program_init();
  
#ifdef GETBITSMMAP
  get_next_packet(); // Realy, open and mmap the FILE
  packet.offset = 0;
  packet.length = 0;  
  buf = (uint32_t *)mmap_base;
  buf_size = 0;
  offs = 0;
  bits_left = 0;
  read_buf(); // Read firs packet and fill 'curent_word'.
  while( 1 ) {
    DPRINTF(1, "Looking for new Video Sequence\n");
    video_sequence();
  }  
#else
#ifdef GETBITS32
  next_word();
#else
  fread(&buf[0], READ_SIZE, 1, infile);
  {
    uint32_t new_word1 = buf[offs++];
    uint32_t new_word2;
    if(offs >= READ_SIZE/4)
      read_buf();
    new_word2 = buf[offs++];
    if(offs >= READ_SIZE/4)
      read_buf();
    cur_word = ((uint64_t)(new_word1) << 32) | new_word2;
  }
#endif
  while(!feof(infile)) {
    DPRINTF(1, "Looking for new Video Sequence\n");
    video_sequence();
  }
#endif
  return 0;
}



/* 5.2.3 Definition of next_start_code() function */
void next_start_code(void)
{
  while(!bytealigned()) {
    GETBITS(1, "next_start_code");
  }
  while(nextbits(24) != 0x000001) {
    GETBITS(8, "next_start_code");
  }
}

void resync(void) {
  DPRINTF(1, "Resyncing\n");
  GETBITS(8, "resync");

}

/* 6.2.2 Video Sequence */
void video_sequence(void) {
  next_start_code();
  if(nextbits(32) == MPEG2_VS_SEQUENCE_HEADER_CODE) {
    DPRINTF(1, "Found Sequence Header\n");

    sequence_header();

    if(nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) {
      sequence_extension();
      do {
	extension_and_user_data(0);
	do {
	  if(nextbits(32) == MPEG2_VS_GROUP_START_CODE) {
	    group_of_pictures_header();
	    extension_and_user_data(1);
	  }
	  picture_header();
	  picture_coding_extension();

	  extension_and_user_data(2);

	  picture_data();

       	} while((nextbits(32) == MPEG2_VS_PICTURE_START_CODE) ||
		(nextbits(32) == MPEG2_VS_GROUP_START_CODE));
	if(nextbits(32) != MPEG2_VS_SEQUENCE_END_CODE) {

	  if(nextbits(32) != MPEG2_VS_SEQUENCE_HEADER_CODE) {
	    DPRINTF(1, "*** not a sequence header\n");

	    break;
	  }
	  sequence_header();

	  sequence_extension();

	}
      } while(nextbits(32) != MPEG2_VS_SEQUENCE_END_CODE);
    } else {
      fprintf(stderr, "ERROR: This is an ISO/IEC 11172-2 Stream\n");
    }
    if(nextbits(32) == MPEG2_VS_SEQUENCE_END_CODE) {
      GETBITS(32, "Sequence End Code");
      DPRINTF(1, "Found Sequence End\n");

    } else {
      DPRINTF(1, "*** Didn't find Sequence End\n");

    }
  } else {
    resync();
  }

}
  

void marker_bit(void)
{
  if(!GETBITS(1, "markerbit")) {
    fprintf(stderr, "*** incorrect marker_bit in stream\n");
    exit(-1);
  }
}


typedef struct {

  uint16_t horizontal_size_value;
  uint16_t vertical_size_value;
  uint8_t aspect_ratio_information;
  uint8_t frame_rate_code;
  uint32_t bit_rate_value;
  uint16_t vbv_buffer_size_value;
  uint8_t constrained_parameters_flag;
  uint8_t load_intra_quantiser_matrix;
  uint8_t intra_quantiser_matrix[64];
  uint8_t load_non_intra_quantiser_matrix;
  uint8_t non_intra_quantiser_matrix[64];
  
  /***/
  uint8_t intra_inverse_quantiser_matrix[64];
  uint8_t non_intra_inverse_quantiser_matrix[64];

  /***/
  int16_t scaled_intra_inverse_quantiser_matrix[8][8];

} sequence_header_t;


typedef struct {
  uint8_t extension_start_code_identifier;
  uint8_t profile_and_level_indication;
  uint8_t progressive_sequence;
  uint8_t chroma_format;
  uint8_t horizontal_size_extension;
  uint8_t vertical_size_extension;
  uint16_t bit_rate_extension;
  uint8_t vbv_buffer_size_extension;
  uint8_t low_delay;
  uint8_t frame_rate_extension_n;
  uint8_t frame_rate_extension_d;
  
} sequence_extension_t;

typedef struct {
  sequence_header_t header;
  sequence_extension_t ext;

  /***/
  int16_t horizontal_size;
  int16_t vertical_size;

  int16_t mb_width;
  int16_t mb_height;
  int16_t mb_row;
  int16_t macroblock_address;
  int16_t previous_macroblock_address;
  int16_t mb_column;
  


} sequence_t;

sequence_t seq;

/* 6.2.2.1 Sequence header */
void sequence_header(void)
{
  uint32_t sequence_header_code;
  
  DPRINTF(1, "sequence_header\n");
  
  sequence_header_code = GETBITS(32, "sequence header code");
  
  /* When a sequence_header_code is decoded all matrices shall be reset
     to their default values */
  
  //  reset_to_default_quantiser_matrix();
  
  seq.header.horizontal_size_value = GETBITS(12, "horizontal_size_value");
  seq.header.vertical_size_value = GETBITS(12, "vertical_size_value");
  seq.header.aspect_ratio_information = GETBITS(4, "aspect_ratio_information");
  DPRINTF(1, "vertical: %u\n", seq.header.horizontal_size_value);
  DPRINTF(1, "horisontal: %u\n", seq.header.vertical_size_value);
  seq.header.frame_rate_code = GETBITS(4, "frame_rate_code");
  seq.header.bit_rate_value = GETBITS(18, "bit_rate_value");  
  marker_bit();
  seq.header.vbv_buffer_size_value = GETBITS(10, "vbv_buffer_size_value");
  seq.header.constrained_parameters_flag = GETBITS(1, "constrained_parameters_flag");
  seq.header.load_intra_quantiser_matrix = GETBITS(1, "load_intra_quantiser_matrix");
  if(seq.header.load_intra_quantiser_matrix) {
    int n;
    intra_inverse_quantiser_matrix_changed = 1;
#ifdef STATS
    stats_intra_inverse_quantiser_matrix_loaded = 1;
#endif
    for(n = 0; n < 64; n++) {
      seq.header.intra_quantiser_matrix[n] = GETBITS(8, "intra_quantiser_matrix[n]");
    }
    
    /* 7.3.1 Inverse scan for matrix download */
    {
      int v, u;
      for (v=0; v<8; v++) {
	for (u=0; u<8; u++) {
	  seq.header.intra_inverse_quantiser_matrix[v*8+u] =
	    seq.header.intra_quantiser_matrix[scan[0][v][u]];
	}
      }
    }
    
  } else {
    if(intra_inverse_quantiser_matrix_changed) {
      reset_to_default_intra_quantiser_matrix();
      intra_inverse_quantiser_matrix_changed = 0;
#ifdef STATS
      stats_intra_inverse_quantiser_matrix_reset = 1;
#endif
    }
  }
  
#ifdef STATS
  stats_intra_inverse_quantiser_matrix_changes_possible++;

  if(stats_intra_inverse_quantiser_matrix_loaded) {
    stats_intra_inverse_quantiser_matrix_loaded_nr++;
    stats_intra_inverse_quantiser_matrix_loaded = 0;
  }
  if(stats_intra_inverse_quantiser_matrix_reset) {
    stats_intra_inverse_quantiser_matrix_reset_nr++;
    stats_intra_inverse_quantiser_matrix_reset = 0;
  }
#endif
  
  seq.header.load_non_intra_quantiser_matrix = GETBITS(1, "load_non_intra_quantiser_matrix");
  if(seq.header.load_non_intra_quantiser_matrix) {
    int n;
    non_intra_inverse_quantiser_matrix_changed = 1;
#ifdef STATS
    stats_non_intra_inverse_quantiser_matrix_loaded = 1;
#endif
    for(n = 0; n < 64; n++) {
      seq.header.non_intra_quantiser_matrix[n] = GETBITS(8, "non_intra_quantiser_matrix[n]");
    }
   
    /* inverse scan for matrix download */
    {
      int v, u;
      for (v=0; v<8; v++) {
	for (u=0; u<8; u++) {
	  seq.header.non_intra_inverse_quantiser_matrix[v*8+u] =
	    seq.header.non_intra_quantiser_matrix[scan[0][v][u]];
	}
      }
    }
    
  } else {
    if(non_intra_inverse_quantiser_matrix_changed) {
      reset_to_default_non_intra_quantiser_matrix();
      non_intra_inverse_quantiser_matrix_changed = 0;
#ifdef STATS
      stats_non_intra_inverse_quantiser_matrix_reset = 1;
#endif
    }
  }

#ifdef STATS
  stats_non_intra_inverse_quantiser_matrix_changes_possible++;

  if(stats_non_intra_inverse_quantiser_matrix_loaded) {
    stats_non_intra_inverse_quantiser_matrix_loaded_nr++;
    stats_non_intra_inverse_quantiser_matrix_loaded = 0;
  }
  if(stats_non_intra_inverse_quantiser_matrix_reset) {
    stats_non_intra_inverse_quantiser_matrix_reset_nr++;
    stats_non_intra_inverse_quantiser_matrix_reset = 0;
  }
#endif


  seq.horizontal_size = seq.header.horizontal_size_value;
  seq.vertical_size = seq.header.vertical_size_value;
  

}

/* 6.2.2.3 Sequence extension */
void sequence_extension(void) {

  uint32_t extension_start_code;
  
  
  extension_start_code = GETBITS(32, "extension_start_code");
  
  seq.ext.extension_start_code_identifier = GETBITS(4, "extension_start_code_identifier");
  seq.ext.profile_and_level_indication = GETBITS(8, "profile_and_level_indication");

#ifdef DEBUG
  DPRINTF(2, "profile_and_level_indication: ");
  if(seq.ext.profile_and_level_indication & 0x80) {
    DPRINTF(2, "(reserved)\n");
  } else {
    DPRINTF(2, "Profile: ");
    switch((seq.ext.profile_and_level_indication & 0x70)>>4) {
    case 0x5:
      DPRINTF(2, "Simple");
      break;
    case 0x4:
      DPRINTF(2, "Main");
      break;
    case 0x3:
      DPRINTF(2, "SNR Scalable");
      break;
    case 0x2:
      DPRINTF(2, "Spatially Scalable");
      break;
    case 0x1:
      DPRINTF(2, "High");
      break;
    default:
      DPRINTF(2, "(reserved)");
      break;
    }

    DPRINTF(2, ", Level: ");
    switch(seq.ext.profile_and_level_indication & 0x0f) {
    case 0xA:
      DPRINTF(2, "Low");
      break;
    case 0x8:
      DPRINTF(2, "Main");
      break;
    case 0x6:
      DPRINTF(2, "High 1440");
      break;
    case 0x4:
      DPRINTF(2, "High");
      break;
    default:
      DPRINTF(2, "(reserved)");
      break;
    }
    DPRINTF(2, "\n");
  }
 
#endif
      
  seq.ext.progressive_sequence = GETBITS(1, "progressive_sequence");

  DPRINTF(1, "progressive seq: %01x\n", seq.ext.progressive_sequence);

  seq.ext.chroma_format = GETBITS(2, "chroma_format");
  seq.ext.horizontal_size_extension = GETBITS(2, "horizontal_size_extension");
  seq.ext.vertical_size_extension = GETBITS(2, "vertical_size_extension");
  seq.ext.bit_rate_extension = GETBITS(12, "bit_rate_extension");
  marker_bit();
  seq.ext.vbv_buffer_size_extension = GETBITS(8, "vbv_buffer_size_extension");
  seq.ext.low_delay = GETBITS(1, "low_delay");
  seq.ext.frame_rate_extension_n = GETBITS(2, "frame_rate_extension_n");
  seq.ext.frame_rate_extension_d = GETBITS(5, "frame_rate_extension_d");
  next_start_code();

  seq.horizontal_size |= (seq.ext.horizontal_size_extension << 12);
  seq.vertical_size |= (seq.ext.vertical_size_extension << 12);

  { 
    int num_pels = seq.horizontal_size * seq.vertical_size;
   
    if(ref_image1->y == NULL) {
      DPRINTF(1, "allocateing buffers\n");
      ref_image1->y = memalign(8, num_pels);
      ref_image1->u = memalign(8, num_pels/4);
      ref_image1->v = memalign(8, num_pels/4);
      
      ref_image2->y = memalign(8, num_pels);
      ref_image2->u = memalign(8, num_pels/4);
      ref_image2->v = memalign(8, num_pels/4);
      
      b_image->y = memalign(8, num_pels);
      b_image->u = memalign(8, num_pels/4);
      b_image->v = memalign(8, num_pels/4);

      if (shmem_flag) {
        /* Create shared memory image */
        myximage = XShmCreateImage(mydisplay, vinfo.visual, bpp,
                                   ZPixmap, NULL, &shm_info,
                                   seq.horizontal_size,
                                   seq.vertical_size);

        ximage_ref1 = XShmCreateImage(mydisplay, vinfo.visual, bpp,
				      ZPixmap, NULL, &shm_info_ref1,
				      seq.horizontal_size,
				      seq.vertical_size);

        ximage_ref2 = XShmCreateImage(mydisplay, vinfo.visual, bpp,
				      ZPixmap, NULL, &shm_info_ref2,
				      seq.horizontal_size,
				      seq.vertical_size);
        if (myximage == NULL) {
          fprintf(stderr, 
                  "Shared memory: couldn't create Shm image\n");
          goto shmemerror;
        }
        
        /* Get a shared memory segment */
        shm_info.shmid = shmget(IPC_PRIVATE,
                                myximage->bytes_per_line * myximage->height, 
                                IPC_CREAT | 0777);

        shm_info_ref1.shmid = shmget(IPC_PRIVATE,
				     myximage->bytes_per_line * myximage->height, 
				     IPC_CREAT | 0777);

        shm_info_ref2.shmid = shmget(IPC_PRIVATE,
				     myximage->bytes_per_line * myximage->height, 
				     IPC_CREAT | 0777);
        if (shm_info.shmid < 0) {
          XDestroyImage(myximage);
          fprintf(stderr, "Shared memory: Couldn't get segment\n");
          goto shmemerror;
        }
        
        /* Attach shared memory segment */
        shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);
        shm_info_ref1.shmaddr = (char *) shmat(shm_info_ref1.shmid, 0, 0);
        shm_info_ref2.shmaddr = (char *) shmat(shm_info_ref2.shmid, 0, 0);
        if (shm_info.shmaddr == ((char *) -1)) {
          XDestroyImage(myximage);
          fprintf(stderr, "Shared memory: Couldn't attach segment\n");
          goto shmemerror;
        }

        myximage->data = shm_info.shmaddr;
        ximage_ref1->data = shm_info_ref1.shmaddr;
        ximage_ref2->data = shm_info_ref2.shmaddr;
        shm_info.readOnly = False;
        shm_info_ref1.readOnly = False;
        shm_info_ref2.readOnly = False;
        XShmAttach(mydisplay, &shm_info);
        XShmAttach(mydisplay, &shm_info_ref1);
        XShmAttach(mydisplay, &shm_info_ref2);
        XSync(mydisplay, 0);
      } else {
      shmemerror:
        shmem_flag = 0;
        myximage = XGetImage(mydisplay, mywindow, 0, 0,
                             seq.horizontal_size,
                             seq.vertical_size,
                             AllPlanes, ZPixmap);
      }
      ImageData = myximage->data;
      imagedata_ref1 = ximage_ref1->data;
      imagedata_ref2 = ximage_ref2->data;
    } 
  }
}

/* 6.2.2.2 Extension and user data */
void extension_and_user_data(unsigned int i) {
  
  DPRINTF(3, "extension_and_user_data(%u)\n", i);


  while((nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) ||
	(nextbits(32) == MPEG2_VS_USER_DATA_START_CODE)) {
    if(i != 1) {
      if(nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) {
	extension_data(i);
      }
    }
    if(nextbits(32) == MPEG2_VS_USER_DATA_START_CODE) {
      user_data();
    }
  }
}


typedef struct {
  uint8_t extension_start_code_identifier;
  uint8_t f_code[2][2];
  uint8_t intra_dc_precision;
  uint8_t picture_structure;
  uint8_t top_field_first;
  uint8_t frame_pred_frame_dct;
  uint8_t concealment_motion_vectors;
  uint8_t q_scale_type;
  uint8_t intra_vlc_format;
  uint8_t alternate_scan;
  uint8_t repeat_first_field;
  uint8_t chroma_420_type;
  uint8_t progressive_frame;
  uint8_t composite_display_flag;
  uint8_t v_axis;
  uint8_t field_sequence;
  uint8_t sub_carrier;
  uint8_t burst_amplitude;
  uint8_t sub_carrier_phase;
} picture_coding_extension_t;


typedef struct {
  uint16_t temporal_reference;
  uint8_t picture_coding_type;
  uint16_t vbv_delay;
  uint8_t full_pel_forward_vector;
  uint8_t forward_f_code;
  uint8_t full_pel_backward_vector;
  uint8_t backward_f_code;
  uint8_t extra_bit_picture;
  uint8_t extra_information_picture;  
} picture_header_t;



typedef struct {
  picture_header_t header;
  picture_coding_extension_t coding_ext;

  int16_t PMV[2][2][2];
} picture_t;

picture_t pic;

/* 6.2.3.1 Picture coding extension */
void picture_coding_extension(void)
{
  uint32_t extension_start_code;

  DPRINTF(3, "picture_coding_extension\n");

  extension_start_code = GETBITS(32, "extension_start_code");
  pic.coding_ext.extension_start_code_identifier = GETBITS(4, "extension_start_code_identifier");
  pic.coding_ext.f_code[0][0] = GETBITS(4, "f_code[0][0]");
  pic.coding_ext.f_code[0][1] = GETBITS(4, "f_code[0][1]");
  pic.coding_ext.f_code[1][0] = GETBITS(4, "f_code[1][0]");
  pic.coding_ext.f_code[1][1] = GETBITS(4, "f_code[1][1]");
  pic.coding_ext.intra_dc_precision = GETBITS(2, "intra_dc_precision");

  /** opt4 **/
  switch(pic.coding_ext.intra_dc_precision) {
  case 0x0:
    mb.intra_dc_mult = 8;
    break;
  case 0x1:
    mb.intra_dc_mult = 4;
    break;
  case 0x2:
    mb.intra_dc_mult = 2;
    break;
  case 0x3:
    mb.intra_dc_mult = 1;
    break;
  }
  /**/
  pic.coding_ext.picture_structure = GETBITS(2, "picture_structure");

#ifdef DEBUG
  /* Table 6-14 Meaning of picture_structure */
  DPRINTF(1, "picture_structure: ");
  switch(pic.coding_ext.picture_structure) {
  case 0x0:
    DPRINTF(1, "reserved");
    break;
  case 0x1:
    DPRINTF(1, "Top Field");
    break;
  case 0x2:
    DPRINTF(1, "Bottom Field");
    break;
  case 0x3:
    DPRINTF(1, "Frame Picture");
    break;
  }
  DPRINTF(1, "\n");
#endif

  pic.coding_ext.top_field_first = GETBITS(1, "top_field_first");
  DPRINTF(1, "top_field_first: %01x\n", pic.coding_ext.top_field_first);

  pic.coding_ext.frame_pred_frame_dct = GETBITS(1, "frame_pred_frame_dct");
  DPRINTF(1, "frame_pred_frame_dct: %01x\n", pic.coding_ext.frame_pred_frame_dct);
  pic.coding_ext.concealment_motion_vectors = GETBITS(1, "concealment_motion_vectors");
  pic.coding_ext.q_scale_type = GETBITS(1, "q_scale_type");
  pic.coding_ext.intra_vlc_format = GETBITS(1, "intra_vlc_format");
  DPRINTF(1, "intra_vlc_format: %01x\n", pic.coding_ext.intra_vlc_format);
  pic.coding_ext.alternate_scan = GETBITS(1, "alternate_scan");
  DPRINTF(1, "alternate_scan: %01x\n", pic.coding_ext.alternate_scan);
  pic.coding_ext.repeat_first_field = GETBITS(1, "repeat_first_field");
  DPRINTF(1, "repeat_first_field: %01x\n", pic.coding_ext.repeat_first_field);
  pic.coding_ext.chroma_420_type = GETBITS(1, "chroma_420_type");
  pic.coding_ext.progressive_frame = GETBITS(1, "progressive_frame");

  DPRINTF(1, "progressive_frame: %01x\n", pic.coding_ext.progressive_frame);
  
  pic.coding_ext.composite_display_flag = GETBITS(1, "composite_display_flag");
  DPRINTF(1, "composite_display_flag: %01x\n", pic.coding_ext.composite_display_flag);
  if(pic.coding_ext.composite_display_flag) {
    pic.coding_ext.v_axis = GETBITS(1, "v_axis");
    pic.coding_ext.field_sequence = GETBITS(3, "field_sequence");
    pic.coding_ext.sub_carrier = GETBITS(1, "sub_carrier");
    pic.coding_ext.burst_amplitude = GETBITS(7, "burst_amplitude");
    pic.coding_ext.sub_carrier_phase = GETBITS(8, "sub_carrier_phase");
  }
  next_start_code();

  
  seq.mb_width = (seq.horizontal_size+15)/16;
  
  if(seq.ext.progressive_sequence) {
    seq.mb_height = (seq.vertical_size+15)/16;
  } else {
    if(pic.coding_ext.picture_structure == 0x03) {
      /* frame pic */
      seq.mb_height = 2*((seq.vertical_size+31)/32);
    } else {
      /* field_picture */
      seq.mb_height = ((seq.vertical_size+31)/32);
    }
  }
  
}

/* 6.2.2.2.2 User data */
void user_data(void)
{
  uint32_t user_data_start_code;
  uint8_t user_data;
  
  DPRINTF(3, "user_data\n");



  user_data_start_code = GETBITS(32, "user_date_start_code");
  while(nextbits(24) != 0x000001) {
    user_data = GETBITS(8, "user_data");
  }
  next_start_code();
  
}

/* 6.2.2.6 Group of pictures header */
void group_of_pictures_header(void)
{
  uint32_t group_start_code;
  uint32_t time_code;
  uint8_t closed_gop;
  uint8_t broken_link;
  
  DPRINTF(3, "group_of_pictures_header\n");


  group_start_code = GETBITS(32, "group_start_code");
  time_code = GETBITS(25, "time_code");

  /* Table 6-11 --- time_code */
  DPRINTF(2, "time_code: %02d:%02d.%02d'%02d\n",
	  (time_code & 0x00f80000)>>19,
	  (time_code & 0x0007e000)>>13,
	  (time_code & 0x00000fc0)>>6,
	  (time_code & 0x0000003f));
  
  closed_gop = GETBITS(1, "closed_gop");
  broken_link = GETBITS(1, "broken_link");
  next_start_code();
  
}

/* 6.2.3 Picture header */
void picture_header(void)
{
  uint32_t picture_start_code;

  DPRINTF(3, "picture_header\n");

  picture_start_code = GETBITS(32, "picture_start_code");
  pic.header.temporal_reference = GETBITS(10, "temporal_reference");
  pic.header.picture_coding_type = GETBITS(3, "picture_coding_type");

#ifdef DEBUG
  /* Table 6-12 --- picture_coding_type */
  DPRINTF(1, "picture coding type: %01x, ", pic.header.picture_coding_type);

  switch(pic.header.picture_coding_type) {
  case 0x00:
    DPRINTF(1, "forbidden\n");
    break;
  case 0x01:
    DPRINTF(1, "intra-coded (I)\n");
    break;
  case 0x02:
    DPRINTF(1, "predictive-coded (P)\n");
    break;
  case 0x03:
    DPRINTF(1, "bidirectionally-predictive-coded (B)\n");
    break;
  case 0x04:
    DPRINTF(1, "shall not be used (dc intra-coded (D) in ISO/IEC11172-2)\n");
    break;
  default:
    DPRINTF(1, "reserved\n");
    break;
  }
#endif

  pic.header.vbv_delay = GETBITS(16, "vbv_delay");

  if((pic.header.picture_coding_type == 2) ||
     (pic.header.picture_coding_type == 3)) {
    pic.header.full_pel_forward_vector = GETBITS(1, "full_pel_forward_vector");
    pic.header.forward_f_code = GETBITS(3, "forward_f_code");
  }
  
  if(pic.header.picture_coding_type == 3) {
    pic.header.full_pel_backward_vector = GETBITS(1, "full_pel_backward_vector");
    pic.header.backward_f_code = GETBITS(3, "backward_f_code");
  }
  
  while(nextbits(1) == 1) {
    pic.header.extra_bit_picture = GETBITS(1, "extra_bit_picture");
    pic.header.extra_information_picture = GETBITS(8, "extra_information_picture");
  }
  pic.header.extra_bit_picture = GETBITS(1, "extra_bit_picture");
  next_start_code();


}


/* 6.2.3.6 Picture data */
void picture_data(void)
{
  
  yuv_image_t *tmp_img;
  
 
  
  DPRINTF(3, "picture_data\n");
  
  
  
  

  switch(pic.header.picture_coding_type) {
  case 0x1:
  case 0x2:
    //I,P picture

    tmp_img = ref_image1;
    ref_image1 = ref_image2;
    ref_image2 = tmp_img;

    dst_image = ref_image2;
    current_image = ref_image1;
    break;
  case 0x3:
    // B-picture
    dst_image = b_image;
    current_image = b_image;
    break;
  }
  
  DPRINTF(2," switching buffers\n");
  
  // HH - 2000-02-10
  //memset(dst_image->y, 0, seq.horizontal_size*seq.vertical_size);
  //memset(dst_image->u, 0, seq.horizontal_size*seq.vertical_size/4);
  //memset(dst_image->v, 0, seq.horizontal_size*seq.vertical_size/4);

  switch(pic.header.picture_coding_type) {
  case 0x1:
  case 0x2:
    if(show_stat) {
      memcpy(&ref1_mbs[0], &ref2_mbs[0], sizeof(mb)*45*36);
      memcpy(&cur_mbs[0], &ref1_mbs[0], sizeof(mb)*45*36);
    }
    break;
  }
 
  do {
    slice();
  } while((nextbits(32) >= MPEG2_VS_SLICE_START_CODE_LOWEST) &&
	  (nextbits(32) <= MPEG2_VS_SLICE_START_CODE_HIGHEST));
  


  frame_done();
  

  {
    static int first = 1;
    static int frame_nr = 0;
    static struct timeval tv;
    static struct timeval otv;
    static struct timeval tva;
    static struct timeval otva;

#ifdef STATS
    static unsigned long long bits_read_old = 0;
    static unsigned long long bits_read_new = 0;
#endif

    double diff;
    
    otv.tv_sec = tv.tv_sec;
    otv.tv_usec = tv.tv_usec;
    
    gettimeofday(&tv, NULL);
    
    diff = (((double)tv.tv_sec + (double)(tv.tv_usec)/1000000.0)-
	    ((double)otv.tv_sec + (double)(otv.tv_usec)/1000000.0));
    
    if(first) {
      first = 0;
      gettimeofday(&tva, NULL);
      frame_nr = 25;
    } else {
      switch(pic.header.picture_coding_type) {
      case 0x1:
	time_pic[0] += diff;
	num_pic[0]++;
	if(diff > time_max[0]) {
	  time_max[0] = diff;
	}
	if(diff < time_min[0]) {
	  time_min[0] = diff;
	}
	break;
      case 0x2:
	time_pic[1] += diff;
	num_pic[1]++;
	if(diff > time_max[1]) {
	  time_max[1] = diff;
	}
	if(diff < time_min[1]) {
	  time_min[1] = diff;
	}
	break;
      case 0x3:
	time_pic[2] += diff;
	num_pic[2]++;
	if(diff > time_max[2]) {
	  time_max[2] = diff;
	}
	if(diff < time_min[2]) {
	  time_min[2] = diff;
	}
	break;
      }
      time_pic[3] += diff;
      num_pic[3]++;
      if(diff > time_max[3]) {
	time_max[3] = diff;
      }
      if(diff < time_min[3]) {
	time_min[3] = diff;
      }

      
      if(frame_nr == 0) {
	
	frame_nr = 25;
	otva.tv_sec = tva.tv_sec;
	otva.tv_usec = tva.tv_usec;
	gettimeofday(&tva, NULL);
#ifdef STATS
	bits_read_old = bits_read_new;
	bits_read_new = stats_bits_read;
#endif
	fprintf(stderr, "frame rate: %f fps\t",
		25.0/(((double)tva.tv_sec+
		       (double)(tva.tv_usec)/1000000.0)-
		      ((double)otva.tv_sec+
		       (double)(otva.tv_usec)/1000000.0))
		);
#ifdef STATS
	fprintf(stderr, "bit rate: %.2f Mb/s, (%.2f), ",
		(((double)(bits_read_new-bits_read_old))/1000000.0)/
		(((double)tva.tv_sec+
		  (double)(tva.tv_usec)/1000000.0)-
		 ((double)otva.tv_sec+
		  (double)(otva.tv_usec)/1000000.0)),
		(((double)(bits_read_new-bits_read_old))/1000000.0)/25.0*24.0);
	
	
	fprintf(stderr, "(%.2f), %.2f Mb/f\n",
		(((double)(bits_read_new-bits_read_old))/1000000.0)/25.0*30.0,
		(((double)(bits_read_new-bits_read_old))/1000000.0)/25.0);
#else
	fprintf(stderr, "\n");
#endif
      }
      frame_nr--;
    }
    
  }

    
  
  next_start_code();
}

/* 6.2.2.2.1 Extension data */
void extension_data(unsigned int i)
{

  uint32_t extension_start_code;


  DPRINTF(3, "extension_data(%d)", i);
  
  while(nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) {
    extension_start_code = GETBITS(32, "extension_start_code");
    if(i == 0) {
      /* follows sequence_extension() */
      
      if(nextbits(4) == MPEG2_VS_SEQUENCE_DISPLAY_EXTENSION_ID) {
	sequence_display_extension();
      }
      if(nextbits(4) == MPEG2_VS_SEQUENCE_SCALABLE_EXTENSION_ID) {
	sequence_scalable_extension();
      }
    }

    /* extension never follows a group_of_picture_header() */
    
    if(i == 2) {
      /* follows picture_coding_extension() */

      if(nextbits(4) == MPEG2_VS_QUANT_MATRIX_EXTENSION_ID) {
	quant_matrix_extension();
      }
      if(nextbits(4) == MPEG2_VS_PICTURE_DISPLAY_EXTENSION_ID) {
	/* the draft says picture_pan_scan_extension_id (must be wrong?) */
	picture_display_extension();
      }
      if(nextbits(4) == MPEG2_VS_PICTURE_SPATIAL_SCALABLE_EXTENSION_ID) {
	picture_spatial_scalable_extension();
      }
      if(nextbits(4) == MPEG2_VS_PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID) {
	picture_temporal_scalable_extension();
      }
    }
  }
}


typedef struct {
  uint8_t slice_vertical_position;
  uint8_t slice_vertical_position_extension;
  uint8_t priority_breakpoint;
  uint8_t quantiser_scale_code;
  uint8_t intra_slice_flag;
  uint8_t intra_slice;
  uint8_t reserved_bits;
  uint8_t extra_bit_slice;
  uint8_t extra_information_slice;
} slice_t;

slice_t slice_data;

/* 6.2.4 Slice */
void slice(void)
{
  uint32_t slice_start_code;
  
  DPRINTF(3, "slice\n");

  
  reset_dc_dct_pred();
  reset_PMV();

  DPRINTF(3, "start of slice\n");
  
  slice_start_code = GETBITS(32, "slice_start_code");
  slice_data.slice_vertical_position = slice_start_code & 0xff;
  
  if(seq.vertical_size > 2800) {
    slice_data.slice_vertical_position_extension = GETBITS(3, "slice_vertical_position_extension");
  }
  
  if(seq.vertical_size > 2800) {
    seq.mb_row = (slice_data.slice_vertical_position_extension << 7) +
      slice_data.slice_vertical_position - 1;
  } else {
    seq.mb_row = slice_data.slice_vertical_position - 1;
  }

  seq.previous_macroblock_address = (seq.mb_row * seq.mb_width)-1;

  //TODO
  if(0) {//sequence_scalable_extension_present) {
    if(0) { //scalable_mode == DATA_PARTITIONING) {
      slice_data.priority_breakpoint = GETBITS(7, "priority_breakpoint");
    }
  }
  slice_data.quantiser_scale_code = GETBITS(5, "quantiser_scale_code");
  /** opt4 **/
  mb.quantiser_scale =
    q_scale[slice_data.quantiser_scale_code][pic.coding_ext.q_scale_type];
  new_scaled = 1;
  /**/

  if(nextbits(1) == 1) {
    slice_data.intra_slice_flag = GETBITS(1, "intra_slice_flag");
    slice_data.intra_slice = GETBITS(1, "intra_slice");
    slice_data.reserved_bits = GETBITS(7, "reserved_bits");
    while(nextbits(1) == 1) {
      slice_data.extra_bit_slice = GETBITS(1, "extra_bit_slice");
      slice_data.extra_information_slice = GETBITS(8, "extra_information_slice");
    }
  }
  slice_data.extra_bit_slice = GETBITS(1, "extra_bit_slice");
  
  do {
    macroblock();
  } while(nextbits(23) != 0);
  next_start_code();
}



/* 6.2.5 Macroblock */
void macroblock(void)
{
  unsigned int block_count = 0;
  uint16_t inc_add = 0;
  
  DPRINTF(3, "macroblock()\n");

  while(nextbits(11) == 0x0008) {
    mb.macroblock_escape = GETBITS(11, "macroblock_escape");
    inc_add+=33;
  }

  mb.macroblock_address_increment = get_vlc(table_b1, "macroblock_address_increment");

  mb.macroblock_address_increment+= inc_add;
  
  seq.macroblock_address =  mb.macroblock_address_increment + seq.previous_macroblock_address;
  
  seq.previous_macroblock_address = seq.macroblock_address;

  seq.mb_column = seq.macroblock_address % seq.mb_width;
  
  DPRINTF(2, " Macroblock: %d, row: %d, col: %d\n",
	  seq.macroblock_address,
	  seq.mb_row,
	  seq.mb_column);
  
#ifdef DEBUG
  if(mb.macroblock_address_increment > 1) {
    DPRINTF(3, "Skipped %d macroblocks\n",
	    mb.macroblock_address_increment);
  }
#endif
  
  
  if(pic.header.picture_coding_type == 0x2) {
    /* In a P-picture when a macroblock is skipped */
    if(mb.macroblock_address_increment > 1) {
      reset_PMV();
      reset_vectors();
    }
  }
  
  
  
  
  if(mb.macroblock_address_increment > 1) {
    
    int x,y;
    int old_col = seq.mb_column;
    int old_address = seq.macroblock_address;
    
    mb.skipped = 1;

    for(seq.macroblock_address = seq.macroblock_address-mb.macroblock_address_increment+1; seq.macroblock_address < old_address; seq.macroblock_address++) {
      switch(pic.header.picture_coding_type) {
      case 0x1:
      case 0x2:
	if(show_stat) {
	  memcpy(&ref2_mbs[seq.macroblock_address], &mb, sizeof(mb));
	}
	break;
      case 0x3:
	if(show_stat) {
	  memcpy(&cur_mbs[seq.macroblock_address], &mb, sizeof(mb));
	}
	break;
      }
    }
    
    
    switch(pic.header.picture_coding_type) {
    case 0x2:
      
      
      DPRINTF(3,"skipped in P-frame\n");
      for(x = (seq.mb_column-mb.macroblock_address_increment+1)*16, y = seq.mb_row*16;
	  y < (seq.mb_row+1)*16; y++) {
	memcpy(&dst_image->y[y*720+x], &ref_image1->y[y*720+x], (mb.macroblock_address_increment-1)*16);
      }
      
      for(x = (seq.mb_column-mb.macroblock_address_increment+1)*8, y = seq.mb_row*8;
	  y < (seq.mb_row+1)*8; y++) {
	memcpy(&dst_image->u[y*720/2+x], &ref_image1->u[y*720/2+x], (mb.macroblock_address_increment-1)*8);
      }
      
      for(x = (seq.mb_column-mb.macroblock_address_increment+1)*8, y = seq.mb_row*8;
	  y < (seq.mb_row+1)*8; y++) {
	memcpy(&dst_image->v[y*720/2+x], &ref_image1->v[y*720/2+x], (mb.macroblock_address_increment-1)*8);
      }
      
      break;
    case 0x3:
      DPRINTF(3,"skipped in B-frame\n");
      for(seq.mb_column = seq.mb_column-mb.macroblock_address_increment+1;
	  seq.mb_column < old_col; seq.mb_column++) {
	if(pic.coding_ext.picture_structure == 0x03 /*frame*/) {
	  /* 7.6.6.4  B frame picture */
	  mb.prediction_type = PRED_TYPE_FRAME_BASED;
	  mb.motion_vector_count = 1;
	  mb.mv_format = MV_FORMAT_FRAME;
	  mb.dmv = 0;
	}
	motion_comp();
      }
      seq.mb_column = old_col;
      break;
    default:
      fprintf(stderr, "*** skipped blocks in I-frame\n");
      break;
    }
  }

  mb.skipped = 0;
  




  macroblock_modes();
  
  if(mb.modes.macroblock_intra == 0) {
    reset_dc_dct_pred();
    
    DPRINTF(3, "non_intra macroblock\n");
    
  }

  if(mb.macroblock_address_increment > 1) {
    reset_dc_dct_pred();
    DPRINTF(3, "skipped block\n");
  }
    
   


  if(mb.modes.macroblock_quant) {
    slice_data.quantiser_scale_code = GETBITS(5, "quantiser_scale_code");
    /** opt4 **/
    mb.quantiser_scale =
      q_scale[slice_data.quantiser_scale_code][pic.coding_ext.q_scale_type];
    /**/
    new_scaled = 1;
  }
  
#ifdef STATS
  stats_quantiser_scale_possible++;
  if(new_scaled) {
    stats_quantiser_scale_nr++;
  }
#endif
  if(mb.modes.macroblock_intra) {
#ifdef STATS
    stats_intra_quantiser_scale_possible++;
#endif
    if(new_scaled) {
#ifdef STATS
      stats_intra_quantiser_scale_nr++;
#endif
      new_scaled = 0;
    }
    
  } else {
#ifdef STATS
    stats_non_intra_quantiser_scale_possible++;
#endif
    if(new_scaled) {
#ifdef STATS
      stats_non_intra_quantiser_scale_nr++;
#endif
      new_scaled = 0;
    }
  }    
  if(mb.modes.macroblock_motion_forward ||
     (mb.modes.macroblock_intra &&
      pic.coding_ext.concealment_motion_vectors)) {
    motion_vectors(0);
  }
  if(mb.modes.macroblock_motion_backward) {
    motion_vectors(1);
  }
  if(mb.modes.macroblock_intra && pic.coding_ext.concealment_motion_vectors) {
    marker_bit();
  }

  /* All motion vectors for the block has been
     decoded. Update predictors
  */
  
  if(pic.coding_ext.picture_structure == 0x03 /*frame*/) {
    switch(mb.prediction_type) {
    case PRED_TYPE_FRAME_BASED:
      if(mb.modes.macroblock_intra) {
	if(pic.coding_ext.concealment_motion_vectors == 0) {
	  reset_PMV();
	  DPRINTF(4, "* 1\n");
	} else {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
      } else {
	if(mb.modes.macroblock_motion_forward) {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
	if(mb.modes.macroblock_motion_backward) {
	  pic.PMV[1][1][1] = pic.PMV[0][1][1];
	  pic.PMV[1][1][0] = pic.PMV[0][1][0];
	}
	if(pic.coding_ext.frame_pred_frame_dct != 0) {
	  if((mb.modes.macroblock_motion_forward == 0) &&
	     (mb.modes.macroblock_motion_backward == 0)) {
	    reset_PMV();
	    DPRINTF(4, "* 2\n");
	  }
	}
      }
      break;
    case PRED_TYPE_FIELD_BASED:
      break;
    case PRED_TYPE_DUAL_PRIME:
      if(mb.modes.macroblock_motion_forward) {
	pic.PMV[1][0][1] = pic.PMV[0][0][1];
	pic.PMV[1][0][0] = pic.PMV[0][0][0];
      }      
      break;
    default:
      fprintf(stderr, "*** invalid pred_type\n");
      exit(-1);
      break;
    }	
  } else {
    switch(mb.prediction_type) {
    case PRED_TYPE_FIELD_BASED:
      if(mb.modes.macroblock_intra) {
	if(pic.coding_ext.concealment_motion_vectors == 0) {
	  DPRINTF(4, "* 3\n");
	  reset_PMV();
	} else {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
      } else {
	if(mb.modes.macroblock_motion_forward) {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
	if(mb.modes.macroblock_motion_backward) {
	  pic.PMV[1][1][1] = pic.PMV[0][1][1];
	  pic.PMV[1][1][0] = pic.PMV[0][1][0];
	}
	if(pic.coding_ext.frame_pred_frame_dct != 0) {
	  if((mb.modes.macroblock_motion_forward == 0) &&
	     (mb.modes.macroblock_motion_backward == 0)) {
	    reset_PMV();
	    DPRINTF(4, "* 4\n");

	  }
	}
      }
      break;
    case PRED_TYPE_16x8_MC:
      break;
    case PRED_TYPE_DUAL_PRIME:
      if(mb.modes.macroblock_motion_forward) {
	pic.PMV[1][0][1] = pic.PMV[0][0][1];
	pic.PMV[1][0][0] = pic.PMV[0][0][0];
      }      
      break;
    default:
      fprintf(stderr, "*** invalid pred_type\n");
      exit(-1);
      break;
    }
	
  }
  
  /* Whenever an intra macroblock is decoded which has
     no concealment motion vectors */
  
  if(mb.modes.macroblock_intra) {
    if(pic.coding_ext.concealment_motion_vectors == 0) {
      DPRINTF(4, "* 5\n");
      reset_PMV();
    }
  }
  

  if(pic.header.picture_coding_type == 0x02) {

    /* In a P-picture when a non-intra macroblock is decoded
       in which macroblock_motion_forward is zero */

    if(mb.modes.macroblock_intra == 0) {
      if(mb.modes.macroblock_motion_forward == 0) {
	reset_PMV();
	DPRINTF(4, "* 6\n");
	
      }
    }

  }
    
  

  /* Table 6-20 block_count as a function of chroma_format */
  if(seq.ext.chroma_format == 0x01) {
    block_count = 6;
  } else if(seq.ext.chroma_format == 0x02) {
    block_count = 8;
  } else if(seq.ext.chroma_format == 0x03) {
    block_count = 12;
  }
  



  if(mb.modes.macroblock_pattern) {
    coded_block_pattern();
  } else {
    mb.cbp = 0;
  }

  {
    int i;
    for(i = 0; i < 12; i++) {  // !!!!
      mb.pattern_code[i] = 0;
    } 
    
    if(mb.modes.macroblock_intra != 0) {
      for(i = 0; i < block_count; i++) {  
	mb.pattern_code[i] = 1;
	block_intra(i);
      }
    } else {
      for(i = 0; i < 6; i++) {
	if(mb.cbp & (1<<(5-i))) {
	  DPRINTF(4, "cbpindex: %d set\n", i);
	  mb.pattern_code[i] = 1;
	  block_non_intra(i);
	}
      }
      if(seq.ext.chroma_format == 0x02) {
	for(i = 6; i < 8; i++) {
	  if(mb.coded_block_pattern_1 & (1<<(7-i))) {
	    mb.pattern_code[i] = 1;
	    block_non_intra(i);
	  }
	}
      }
      if(seq.ext.chroma_format == 0x03) {
	for(i = 6; i < 12; i++) {
	  if(mb.coded_block_pattern_2 & (1<<(11-i))) {
	    mb.pattern_code[i] = 1;
	    block_non_intra(i);
	  }
	}
      }
    } 
  }
  

  

  /*** 7.6.3.5 Prediction in P-pictures ***/

  if(pic.header.picture_coding_type == 0x2) {
    /* P-picture */
    if((!mb.modes.macroblock_motion_forward) && (!mb.modes.macroblock_intra)) {
      DPRINTF(2, "prediction mode Frame-base, \nresetting motion vector predictor and motion vector\n");
      DPRINTF(2, "motion_type: %02x\n", mb.modes.frame_motion_type);
      if(pic.coding_ext.picture_structure == 0x3) {
	
	mb.prediction_type = PRED_TYPE_FRAME_BASED;
	mb.mv_format = MV_FORMAT_FRAME;
	reset_PMV();
      } else {

	mb.prediction_type = PRED_TYPE_FIELD_BASED;
	mb.mv_format = MV_FORMAT_FIELD;
      }
      mb.modes.macroblock_motion_forward = 1;
      mb.vector[0][0][0] = 0;
      mb.vector[0][0][1] = 0;
      mb.vector[1][0][0] = 0;
      mb.vector[1][0][1] = 0;
       
      

    }

    /* never happens */
    /*
      if(mb.macroblock_address_increment > 1) {
      
      fprintf(stderr, "prediction mode shall be Frame-based\n");
      fprintf(stderr, "motion vector predictors shall be reset to zero\n");
      fprintf(stderr, "motion vector shall be zero\n");
      
      // *** TODO
      //mb.vector[0][0][0] = 0;
      //mb.vector[0][0][1] = 0;

      }
    */
    
  }
 
  switch(pic.header.picture_coding_type) {
  case 0x1:
  case 0x2:
    if(show_stat) {
      memcpy(&ref2_mbs[seq.macroblock_address], &mb, sizeof(mb));
    }
    break;
  case 0x3:
    if(show_stat) {
      memcpy(&cur_mbs[seq.macroblock_address], &mb, sizeof(mb));
    }
    break;

  }
  
  // HH - 2000-02-10
  // Intra blocks are already handled directly in block()
  if( !mb.modes.macroblock_intra )
    motion_comp();

}

/* 6.2.5.1 Macroblock modes */
void macroblock_modes(void)
{
  

  DPRINTF(3, "macroblock_modes\n");


  if(pic.header.picture_coding_type == 0x01) {
    /* I-picture */
    mb.modes.macroblock_type = get_vlc(table_b2, "macroblock_type (b2)");

  } else if(pic.header.picture_coding_type == 0x02) {
    /* P-picture */
    mb.modes.macroblock_type = get_vlc(table_b3, "macroblock_type (b3)");

  } else if(pic.header.picture_coding_type == 0x03) {
    /* B-picture */

    mb.modes.macroblock_type = get_vlc(table_b4, "macroblock_type (b4)");
    
  } else {
    fprintf(stderr, "*** Unsupported picture type %02x\n", pic.header.picture_coding_type);
    exit(-1);
  }
  
  mb.modes.macroblock_quant = mb.modes.macroblock_type & MACROBLOCK_QUANT;
  mb.modes.macroblock_motion_forward =
    mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD;
  mb.modes.macroblock_motion_backward =
    mb.modes.macroblock_type & MACROBLOCK_MOTION_BACKWARD;
  mb.modes.macroblock_pattern = mb.modes.macroblock_type & MACROBLOCK_PATTERN;
  mb.modes.macroblock_intra = mb.modes.macroblock_type & MACROBLOCK_INTRA;
  mb.modes.spatial_temporal_weight_code_flag =
    mb.modes.macroblock_type & SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG;
  
  DPRINTF(5, "spatial_temporal_weight_code_flag: %01x\n", mb.modes.spatial_temporal_weight_code_flag);

  if((mb.modes.spatial_temporal_weight_code_flag == 1) &&
     ( 1 /*spatial_temporal_weight_code_table_index != 0*/)) {
    mb.modes.spatial_temporal_weight_code = GETBITS(2, "spatial_temporal_weight_code");
  }

  if(mb.modes.macroblock_motion_forward ||
     mb.modes.macroblock_motion_backward) {
    if(pic.coding_ext.picture_structure == 0x03 /*frame*/) {
      if(pic.coding_ext.frame_pred_frame_dct == 0) {
	mb.modes.frame_motion_type = GETBITS(2, "frame_motion_type");
      } else {
	/* frame_motion_type omitted from the bitstream */
	mb.modes.frame_motion_type = 0x2;
      }
    } else {
      mb.modes.field_motion_type = GETBITS(2, "field_motion_type");
    }
  }

  /* if(decode_dct_type) */
  if((pic.coding_ext.picture_structure == 0x03 /*frame*/) &&
     (pic.coding_ext.frame_pred_frame_dct == 0) &&
     (mb.modes.macroblock_intra || mb.modes.macroblock_pattern)) {
    
    mb.modes.dct_type = GETBITS(1, "dct_type");
  } else {
    /* Table 6-19. Value of dct_type if dct_type is not in the bitstream.*/
    if(pic.coding_ext.frame_pred_frame_dct == 1) {
      mb.modes.dct_type = 0;
    }
    /* else dct_type is unused, either field picture or mb not coded */
  }
}

/* 6.2.5.3 Coded block pattern */
void coded_block_pattern(void)
{
  //  uint16_t coded_block_pattern_420;
  uint8_t cbp = 0;
  
  DPRINTF(3, "coded_block_pattern\n");

  cbp = get_vlc(table_b9, "cbp (b9)");
  

  if((cbp == 0) && (seq.ext.chroma_format == 0x1)) {
    fprintf(stderr, "** shall not be used with 4:2:0 chrominance\n");
    exit(-1);
  }
  mb.cbp = cbp;
  
  DPRINTF(4, "cpb = %u\n", mb.cbp);

  if(seq.ext.chroma_format == 0x02) {
    mb.coded_block_pattern_1 = GETBITS(2, "coded_block_pattern_1");
  }
 
  if(seq.ext.chroma_format == 0x03) {
    mb.coded_block_pattern_2 = GETBITS(6, "coded_block_pattern_2");
  }
  
}


#if 0
void get_dct(runlevel_t *runlevel, int first_subseq, uint8_t intra_block,
	     uint8_t intra_vlc_format, char *func) 
{

  int code;
  DCTtab *tab;
  int run;
  signed int val;
  int non_intra_dc;
  
  //this routines handles intra AC and non-intra AC/DC coefficients
  code = nextbits(16);
  
  non_intra_dc = ((intra_vlc_format) && (intra_block)); // bad name !!!
  
  if (code>=16384 && !(non_intra_dc))
    {
      if(first_subseq == DCT_DC_FIRST)
	tab = &DCTtabfirst[(code>>12)-4]; // 14 
      else
	tab = &DCTtabnext[(code>>12)-4];  // 14 
    }
  else if(code>=1024)
    {
      if (non_intra_dc)
	tab = &DCTtab0a[(code>>8)-4];   // 15
      else
	tab = &DCTtab0[(code>>8)-4];   // 14
    }
  else if(code>=512)
    {
      if(non_intra_dc)
	tab = &DCTtab1a[(code>>6)-8];  // 15
      else
	tab = &DCTtab1[(code>>6)-8];  // 14
    }
  else if(code>=256)
    tab = &DCTtab2[(code>>4)-16];
  else if(code>=128)
    tab = &DCTtab3[(code>>3)-16];
  else if(code>=64)
    tab = &DCTtab4[(code>>2)-16];
  else if(code>=32)
    tab = &DCTtab5[(code>>1)-16];
  else if(code>=16)
    tab = &DCTtab6[code-16];
  else {
    fprintf(stderr,
	    "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	    code);
    exit(1);
  }
  
  GETBITS(tab->len, "(get_dct)");
  
  if (tab->run==64) { // end_of_block 
    run = VLC_END_OF_BLOCK;
    val = VLC_END_OF_BLOCK;
  } 
  else if (tab->run==65) { /* escape */
    run = GETBITS(6, "(get_dct escape - run )");
    val = GETBITS(12, "(get_dct escape - level )");
    
    if ((val&2047)==0) {
      fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
      exit(1);
    }
    
    if(val >= 2048)
      val =  val - 4096;
  }
  else {
    run = tab->run;
    val = tab->level; 
    if(GETBITS(1, "(get_dct sign )")) //sign bit
      val = -val;
  }
  
  runlevel->run   = run;
  runlevel->level = val;


}

#endif

#define DCT_INTRA_SPLIT
#ifndef DCT_INTRA_SPLIT

void get_dct_intra(runlevel_t *runlevel, uint8_t intra_vlc_format, char *func) 
{
  int code;
  DCTtab *tab;
  int run;
  signed int val;
  
  //this routines handles intra AC and non-intra AC/DC coefficients
  code = nextbits(16);
  
  if(code>=16384)
    {
      if (intra_vlc_format) 
	tab = &DCTtab0a[(code>>8)-4];   // 15
      else
	tab = &DCTtabnext[(code>>12)-4];  // 14 
    }
  else if(code>=1024)
    {
      if (intra_vlc_format)
	tab = &DCTtab0a[(code>>8)-4];   // 15
      else
	tab = &DCTtab0[(code>>8)-4];   // 14
    }
  else if(code>=512)
    {
      if(intra_vlc_format)
	tab = &DCTtab1a[(code>>6)-8];  // 15
      else
	tab = &DCTtab1[(code>>6)-8];  // 14
    }
  else if(code>=256)
    tab = &DCTtab2[(code>>4)-16];
  else if(code>=128)
    tab = &DCTtab3[(code>>3)-16];
  else if(code>=64)
    tab = &DCTtab4[(code>>2)-16];
  else if(code>=32)
    tab = &DCTtab5[(code>>1)-16];
  else if(code>=16)
    tab = &DCTtab6[code-16];
  else {
    fprintf(stderr,
	    "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	    code);
    exit(1);
  }
  
  dropbits(tab->len);
  
  if (tab->run==64) { // end_of_block 
    run = VLC_END_OF_BLOCK;
    val = VLC_END_OF_BLOCK;
  } 
  else if (tab->run==65) { /* escape */
    run = GETBITS(6, "(get_dct escape - run )");
    val = GETBITS(12, "(get_dct escape - level )");
    
    if ((val&2047)==0) {
      fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
      exit(1);
    }
    
    if(val >= 2048)
      val =  val - 4096;
  }
  else {
    run = tab->run;
    val = tab->level; 
    if(GETBITS(1, "(get_dct sign )")) //sign bit
      val = -val;
  }
  
  runlevel->run   = run;
  runlevel->level = val;


}

#else

static inline
void get_dct_intra_vlcformat_0(runlevel_t *runlevel, char *func) 
{
  int code;
  const DCTtab *tab;
  int run;
  signed int val;
  
  //this routines handles intra AC and non-intra AC/DC coefficients
  code = nextbits(16);
  
  if(code>=16384)
    tab = &DCTtabnext[(code>>12)-4];  // 14 
  else if(code>=1024)
    tab = &DCTtab0[(code>>8)-4];   // 14
  else if(code>=512)
    tab = &DCTtab1[(code>>6)-8];  // 14
  else if(code>=256)
    tab = &DCTtab2[(code>>4)-16];
  else if(code>=128)
    tab = &DCTtab3[(code>>3)-16];
  else if(code>=64)
    tab = &DCTtab4[(code>>2)-16];
  else if(code>=32)
    tab = &DCTtab5[(code>>1)-16];
  else if(code>=16)
    tab = &DCTtab6[code-16];
  else {
    fprintf(stderr,
	    "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	    code);
    exit(1);
  }
  
  dropbits(tab->len);
  
  if (tab->run==64) { // end_of_block 
    run = VLC_END_OF_BLOCK;
    val = VLC_END_OF_BLOCK;
  } 
  else if (tab->run==65) { /* escape */
    uint32_t tmp = GETBITS(6+12, "(get_dct escape - run & level )");
    //    run = GETBITS(6, "(get_dct escape - run )");
    //    val = GETBITS(12, "(get_dct escape - level )");
    run = tmp >> 12;
    val = tmp & 0xfff;
    
    if ((val&2047)==0) {
      fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
      exit(1);
    }
    
    if(val >= 2048)
      val =  val - 4096;
  }
  else {
    run = tab->run;
    val = tab->level; 
    if(GETBITS(1, "(get_dct sign )")) //sign bit
      val = -val;
  }
  
  runlevel->run   = run;
  runlevel->level = val;


}

static inline
void get_dct_intra_vlcformat_1(runlevel_t *runlevel, char *func) 
{
  int code;
  const DCTtab *tab;
  int run;
  signed int val;
  
  //this routines handles intra AC and non-intra AC/DC coefficients
  code = nextbits(16);
  
  if(code>=1024)
    tab = &DCTtab0a[(code>>8)-4];   // 15
  else if(code>=512)
    tab = &DCTtab1a[(code>>6)-8];  // 15
  else if(code>=256)
    tab = &DCTtab2[(code>>4)-16];
  else if(code>=128)
    tab = &DCTtab3[(code>>3)-16];
  else if(code>=64)
    tab = &DCTtab4[(code>>2)-16];
  else if(code>=32)
    tab = &DCTtab5[(code>>1)-16];
  else if(code>=16)
    tab = &DCTtab6[code-16];
  else {
    fprintf(stderr,
	    "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	    code);
    exit(1);
    return;
  }
  
  dropbits(tab->len);
  
  if (tab->run==64) { // end_of_block 
    run = VLC_END_OF_BLOCK;
    val = VLC_END_OF_BLOCK;
  } 
  else if (tab->run==65) { /* escape */
    uint32_t tmp = GETBITS(6+12, "(get_dct escape - run & level )");
    //    run = GETBITS(6, "(get_dct escape - run )");
    //    val = GETBITS(12, "(get_dct escape - level )");
    run = tmp >> 12;
    val = tmp & 0xfff;
    
    if ((val&2047)==0) {
      fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
      return;
    }
    
    if(val >= 2048)
      val =  val - 4096;
  }
  else {
    run = tab->run;
    val = tab->level; 
    if(GETBITS(1, "(get_dct sign )")) //sign bit
      val = -val;
  }
  
  runlevel->run   = run;
  runlevel->level = val;


}

#endif

static inline
void get_dct_non_intra_first(runlevel_t *runlevel, char *func) 
{
  int code;
  const DCTtab *tab;
  int run;
  signed int val;
  
  //this routines handles intra AC and non-intra AC/DC coefficients
  code = nextbits(16);
  
  if (code>=16384)
    tab = &DCTtabfirst[(code>>12)-4]; // 14 
  else if(code>=1024)
    tab = &DCTtab0[(code>>8)-4];   // 14
  else if(code>=512)
    tab = &DCTtab1[(code>>6)-8];  // 14
  else if(code>=256)
    tab = &DCTtab2[(code>>4)-16];
  else if(code>=128)
    tab = &DCTtab3[(code>>3)-16];
  else if(code>=64)
    tab = &DCTtab4[(code>>2)-16];
  else if(code>=32)
    tab = &DCTtab5[(code>>1)-16];
  else if(code>=16)
    tab = &DCTtab6[code-16];
  else {
    fprintf(stderr,
	    "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	    code);
    exit(1);
  }
  
  
  if (tab->run==65) { /* escape */
    uint32_t tmp = GETBITS(6+6+12, "(get_dct escape - run & level )");
#ifdef STATS
    stats_f_non_intra_first_escaped_run_nr++;
#endif
    //    dropbits(tab->len);
    //    run = GETBITS(6, "(get_dct escape - run )");
    //    val = GETBITS(12, "(get_dct escape - level )");
    run = (tmp >> 12) & 0x3f;
    val = tmp & 0xfff;
    
    if ((val&2047)==0) {
      fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
      return;
    }
    
    if(val >= 2048)
      val =  val - 4096;
  }
  else {
    //    dropbits(tab->len);
    run = tab->run;
    val = tab->level; 
    if( 0x1 & GETBITS(tab->len+1, "(get_dct sign )")) //sign bit
      val = -val;
  }
  
  runlevel->run   = run;
  runlevel->level = val;
  

}

#if 0
void get_dct_non_intra_subseq(runlevel_t *runlevel, char *func) 
{
  int code;
  const DCTtab *tab;
  int run;
  signed int val;
  
  //this routines handles intra AC and non-intra AC/DC coefficients
  code = nextbits(16);
  
  if (code>=16384)
    tab = &DCTtabnext[(code>>12)-4];  // 14 
  else if(code>=1024)
    tab = &DCTtab0[(code>>8)-4];   // 14
  else if(code>=512)
    tab = &DCTtab1[(code>>6)-8];  // 14
  else if(code>=256)
    tab = &DCTtab2[(code>>4)-16];
  else if(code>=128)
    tab = &DCTtab3[(code>>3)-16];
  else if(code>=64)
    tab = &DCTtab4[(code>>2)-16];
  else if(code>=32)
    tab = &DCTtab5[(code>>1)-16];
  else if(code>=16)
    tab = &DCTtab6[code-16];
  else {
    fprintf(stderr,
	    "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	    code);
    exit(1);
  }
  
  dropbits(tab->len);
  
  if (tab->run==64) { // end_of_block 
    run = VLC_END_OF_BLOCK;
    val = VLC_END_OF_BLOCK;
  } 
  else if (tab->run==65) { /* escape */
    run = GETBITS(6, "(get_dct escape - run )");
    val = GETBITS(12, "(get_dct escape - level )");
    
    if ((val&2047)==0) {
      fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
      exit(1);
    }
    
    if(val >= 2048)
      val =  val - 4096;
  }
  else {
    run = tab->run;
    val = tab->level; 
    if(GETBITS(1, "(get_dct sign )")) //sign bit
      val = -val;
  }
  
  runlevel->run   = run;
  runlevel->level = val;
  
}
#endif


void inverse_quantisation_final(int sum)
{
  if((sum & 1) == 0) {
    if((mb.QFS[63]&1) != 0) {
      mb.QFS[63] = mb.QFS[63]-1;
    } else {
      mb.QFS[63] = mb.QFS[63]+1;
    }
  }
}





 
/* 6.2.6 Block */
void block_intra(unsigned int i)
{
  uint8_t cc;
  
  /* Table 7-1. Definition of cc, colour component index */ 
  if(i < 4)
    cc = 0;
  else
    cc = (i%2)+1;
  
#ifdef STATS
  stats_block_intra_nr++;
#endif  
  {
    int x = seq.mb_column;
    int y = seq.mb_row;
    int width = seq.horizontal_size;
    int d, stride;
    uint8_t *dst;
    
    {
      uint16_t dct_dc_size;
      uint16_t dct_dc_differential = 0;
      int n = 0;
      
      int16_t dct_diff;
      int16_t half_range;
      runlevel_t runlevel;
      int eob_not_read;
      int m;
      
      int inverse_quantisation_sum;
      
      DPRINTF(3, "pattern_code(%d) set\n", i);
      
      for(m=0;m<16;m++) {
	*(((int64_t *)mb.QFS) + m) = 0L;  // !!! Ugly hack !!!
      }
      //memset( mb.F_bis, 0, 64 * sizeof( int16_t ) );
      
      
      if(i < 4) {
	dct_dc_size = get_vlc(table_b12, "dct_dc_size_luminance (b12)");
	DPRINTF(4, "luma_size: %d\n", dct_dc_size);
      } else {
	dct_dc_size = get_vlc(table_b13, "dct_dc_size_chrominance (b13)");
	DPRINTF(4, "chroma_size: %d\n", dct_dc_size);
      } 
      
      if(dct_dc_size != 0) {
	dct_dc_differential = GETBITS(dct_dc_size, "dct_dc_differential");
	DPRINTF(4, "diff_val: %d, ", dct_dc_differential);
	
	half_range = 1<<(dct_dc_size-1);
	
	if(dct_dc_differential >= half_range) {
	  dct_diff = (int16_t) dct_dc_differential;
	} else {
	  dct_diff = (int16_t)((dct_dc_differential+1)-(2*half_range));
	}
	DPRINTF(4, "%d\n", dct_diff);  
	
      } else {
	dct_diff = 0;
      }
      
      {
	// qfs is always between 0 and 2^(8+dct_dc_size)-1, i.e unsigned.
	unsigned int qfs = mb.dc_dct_pred[cc]+dct_diff;
	mb.dc_dct_pred[cc] = qfs;
	DPRINTF(4, "QFS[0]: %d\n", qfs);
	
	/* inverse quantisation */
	{
	  // mb.intra_dc_mult is 1, 2 , 4 or 8, i.e unsigned.
	  unsigned int f = mb.intra_dc_mult * qfs;
	  if(f > 2047) {
	    f = 2047;
	  } 
	  mb.QFS[0] = f;
	  inverse_quantisation_sum = f;
	}
#ifdef STATS
	stats_f_intra_compute_first_nr++;
#endif
	n++;
      }
      
      /* 7.2.2.4 Summary */
      eob_not_read = 1;
#ifdef DCT_INTRA_SPLIT
      if(pic.coding_ext.intra_vlc_format) {
	while( 1 ) {
	  //fprintf(stderr, "Subsequent dct_dc\n");
	  //Subsequent DCT coefficients
	  get_dct_intra_vlcformat_1(&runlevel, 
				    "dct_dc_subsequent");
	
#ifdef DEBUG
	  if(runlevel.run != VLC_END_OF_BLOCK) {
	    DPRINTF(4, "coeff run: %d, level: %d\n",
		    runlevel.run, runlevel.level);
	  }
#endif
    
	  if(runlevel.run == VLC_END_OF_BLOCK) {
	    break;
	  } else {
	    n += runlevel.run;
	  
	    /* inverse quantisation */
	    {
	      uint8_t i = inverse_scan[pic.coding_ext.alternate_scan][n];
	      int f = (runlevel.level 
		       * seq.header.intra_inverse_quantiser_matrix[i]
		       * mb.quantiser_scale)/16;


#ifdef STATS
	      stats_f_intra_compute_subseq_nr++;
#endif

	      if(f > 2047)
		f = 2047;
	      else if(f < -2048)
		f = -2048;

	      mb.QFS[i] = f;
	      inverse_quantisation_sum += f;
	    }
	  
	    n++;      
	  }
	}
      } else {
	while( 1 ) {
	  //fprintf(stderr, "Subsequent dct_dc\n");
	  //Subsequent DCT coefficients
	  get_dct_intra_vlcformat_0(&runlevel, 
				    "dct_dc_subsequent");
	
#ifdef DEBUG
	  if(runlevel.run != VLC_END_OF_BLOCK) {
	    DPRINTF(4, "coeff run: %d, level: %d\n",
		    runlevel.run, runlevel.level);
	  }
#endif
    
	  if(runlevel.run == VLC_END_OF_BLOCK) {
	    break;
	  } else {
	    n += runlevel.run;
	  
	    /* inverse quantisation */
	    {
	      uint8_t i = inverse_scan[pic.coding_ext.alternate_scan][n];
	      int f = (runlevel.level 
		       * seq.header.intra_inverse_quantiser_matrix[i]
		       * mb.quantiser_scale)/16;

#ifdef STATS
	      stats_f_intra_compute_subseq_nr++;
#endif
	    
	      if(f > 2047)
		f = 2047;
	      else if(f < -2048)
		f = -2048;

	      mb.QFS[i] = f;
	      inverse_quantisation_sum += f;
	    }
	  
	    n++;      
	  }
	}
      }
#else
      while( 1 ) {
	//fprintf(stderr, "Subsequent dct_dc\n");
	//Subsequent DCT coefficients
	get_dct_intra(&runlevel, pic.coding_ext.intra_vlc_format, 
		      "dct_dc_subsequent");
	
#ifdef DEBUG
	if(runlevel.run != VLC_END_OF_BLOCK) {
	  DPRINTF(4, "coeff run: %d, level: %d\n",
		  runlevel.run, runlevel.level);
	}
#endif
    
	if(runlevel.run == VLC_END_OF_BLOCK) {
	  break;
	} else {
	  n += runlevel.run;
	  
	  /* inverse quantisation */
	  {
	    uint8_t i = inverse_scan[pic.coding_ext.alternate_scan][n];
	    int f = (runlevel.level 
		     * seq.header.intra_inverse_quantiser_matrix[i]
		     * mb.quantiser_scale)/16;
	    if(f > 2047)
	      f = 2047;
	    else if(f < -2048)
	      f = -2048;

	    mb.QFS[i] = f;
	    inverse_quantisation_sum += f;
	  }
	  
	  n++;      
	}
      }
#endif
      inverse_quantisation_final(inverse_quantisation_sum);
  
      DPRINTF(4, "nr of coeffs: %d\n", n);
    }


    /* Shortcut to write the IDCT data directly into the picture buffer */ 
    if (mb.modes.dct_type) {
      d = 1;
      stride = width * 2;
    } else {
      d = 8;
      stride = width;
    }
    
    if( i < 4 ) {
      dst = &dst_image->y[x * 16 + y * width * 16];
      if( i & 1 ) 
	dst += 8;
      if( i >= 2 ) 
	dst += width * d;
    }
    else {
      stride = width / 2;
      if( i == 4 )
	dst = &dst_image->u[x * 8 + y * width/2 * 8];
      else
	dst = &dst_image->v[x * 8 + y * width/2 * 8];
    }
    mlib_VideoIDCT_U8_S16(dst, (mlib_s16 *)mb.QFS, stride);
  }
}

void block_non_intra(unsigned int b)
{
  uint8_t cc;
  uint8_t i;
  int f;
  /* Table 7-1. Definition of cc, colour component index */ 
  if(b < 4)
    cc = 0;
  else
    cc = (b%2)+1;

#ifdef STATS
  stats_block_non_intra_nr++;
#endif
  
  {
    int n;
    
    runlevel_t runlevel;
    int m;
    
    int inverse_quantisation_sum = 0;
    
    DPRINTF(3, "pattern_code(%d) set\n", b);
    
    for(m=0;m<16;m++) {
      *(((int64_t *)mb.QFS) + m) = 0L;  // !!! Ugly hack !!!
    }
    //memset( mb.F_bis, 0, 64 * sizeof( int16_t ) );
    
    
    /* 7.2.2.4 Summary */
    
    get_dct_non_intra_first(&runlevel, "dct_dc_subsequent");
    n = runlevel.run;
    
    /* inverse quantisation */
    i = inverse_scan[pic.coding_ext.alternate_scan][n];
    
    f = ( ((runlevel.level*2)+(runlevel.level > 0 ? 1 : -1))
	  * seq.header.non_intra_inverse_quantiser_matrix[i]
	  * mb.quantiser_scale )/32;

#ifdef STATS
    stats_f_non_intra_compute_first_nr++;
#endif
    
    if(f > 2047)
      f = 2047;
    else if(f < -2048)
      f = -2048;
    
    mb.QFS[i] = f;
    inverse_quantisation_sum += f;
    
    n++;
    
    
    while(1) {
      //      get_dct_non_intra_subseq(&runlevel, "dct_dc_subsequent");
      int code;
      const DCTtab *tab;
      int run;
      unsigned int val;
      int sgn;
      
      code = nextbits(16);
	
      if (code>=16384)
	tab = &DCTtabnext[(code>>12)-4];  // 14 
      else if(code>=1024)
	tab = &DCTtab0[(code>>8)-4];   // 14
      else if(code>=512)
	tab = &DCTtab1[(code>>6)-8];  // 14
      else if(code>=256)
	tab = &DCTtab2[(code>>4)-16];
      else if(code>=128)
	tab = &DCTtab3[(code>>3)-16];
      else if(code>=64)
	tab = &DCTtab4[(code>>2)-16];
      else if(code>=32)
	tab = &DCTtab5[(code>>1)-16];
      else if(code>=16)
	tab = &DCTtab6[code-16];
      else {
	fprintf(stderr,
		"(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
		code);
	exit(1);
      }
  
#ifdef DEBUG
      if(runlevel.run != VLC_END_OF_BLOCK) {
	DPRINTF(4, "coeff run: %d, level: %d\n",
		runlevel.run, runlevel.level);
      }
#endif
   
      if (tab->run == 64) { // end_of_block 
	//	run = VLC_END_OF_BLOCK;
	//	val = VLC_END_OF_BLOCK;
	dropbits( 2 ); // tab->len, end of block always = 2bits
	break;
      } 
      else {
	if (tab->run == 65) { /* escape */

#ifdef STATS
	  stats_f_non_intra_subseq_escaped_run_nr++;
#endif

	  //	  run = GETBITS(6, "(get_dct escape - run )");
	  //	  val = GETBITS(12, "(get_dct escape - level )");
	  //	  dropbits(tab->len); escape always = 6 bits
	  val = GETBITS(6+12+6, "(escape run + val)" );
	  run = (val >> 12) & 0x3f;
	  val &= 0xfff;
	  
	  if ((val&2047)==0) {
	    fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
	    exit(1);
	  }
	  
	  sgn = (val >= 2048);
	  if( val >= 2048 )                // !!!! ?sgn? 
	    val =  4096 - val;// - 4096;
	}
	else {
	  //	  dropbits(tab->len);
	  run = tab->run;
	  val = tab->level; 
	  sgn = 0x1 & GETBITS(tab->len+1, "(get_dct sign )"); //sign bit
	  //	    val = -val;
	}
	
	n += run;
	
	/* inverse quantisation */
	i = inverse_scan[pic.coding_ext.alternate_scan][n];
	// flytta ut &inverse_scan[pic.coding_ext.alternate_scan] ??

	f = ( ((val*2)+1)
	      * seq.header.non_intra_inverse_quantiser_matrix[i]
	      * mb.quantiser_scale )/32;
	// flytta ut mb.quantiser_scale ??
	
#ifdef STATS
	stats_f_non_intra_compute_subseq_nr++;
#endif

	if(f > 2047) {
	  //	  fprintf(stderr,"clip");
	  //	  if( sgn )
	  //	    fprintf(stderr,"==2048");
	  f = 2047;
	}
	if( sgn )
	  f = -f;
	
	mb.QFS[i] = f;
	inverse_quantisation_sum += f;
	
	n++;      
      }
    }
    inverse_quantisation_final(inverse_quantisation_sum);
    
    DPRINTF(4, "nr of coeffs: %d\n", n);
  }
  mlib_VideoIDCT_S16_S16(mb.f[b], (mlib_s16 *)mb.QFS);
}

/* 6.2.5.2 Motion vectors */
void motion_vectors(unsigned int s)
{
  

  DPRINTF(3, "motion_vectors(%u)\n", s);

  if(pic.coding_ext.picture_structure == 0x03 /*frame*/) {
    if(pic.coding_ext.frame_pred_frame_dct == 0) {
      
      /* Table 6-17 Meaning of frame_motion_type */
      switch(mb.modes.frame_motion_type) {
      case 0x0:
	fprintf(stderr, "*** reserved prediction type\n");
	exit(-1);
	break;
      case 0x1:
	mb.prediction_type = PRED_TYPE_FIELD_BASED;
	mb.motion_vector_count = 2;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 0;
	break;
      case 0x2:
	/* spatial_temporal_weight_class always 0 for now */
	mb.prediction_type = PRED_TYPE_FRAME_BASED;
	mb.motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FRAME;
	mb.dmv = 0;
	break;
      case 0x3:
	mb.prediction_type = PRED_TYPE_DUAL_PRIME;
	mb.motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 1;
	break;
      default:
	fprintf(stderr, "*** impossible prediction type\n");
	exit(-1);
	break;
      }
    }
    if(mb.modes.macroblock_intra &&
       pic.coding_ext.concealment_motion_vectors) {
      mb.prediction_type = PRED_TYPE_FRAME_BASED;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FRAME;
      mb.dmv = 0;
    }
      
    if(pic.coding_ext.frame_pred_frame_dct == 1) {
      mb.prediction_type = PRED_TYPE_FRAME_BASED;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FRAME;
      mb.dmv = 0;
    }
    
  } else {
    /* Table 6-18 Meaning of field_motion_type */
    switch(mb.modes.field_motion_type) {
    case 0x0:
      fprintf(stderr, "*** reserved field prediction type\n");
      exit(-1);
      break;
    case 0x1:
      mb.prediction_type = PRED_TYPE_FIELD_BASED;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FIELD;
      mb.dmv = 0;
      break;
    case 0x2:
      mb.prediction_type = PRED_TYPE_16x8_MC;
      mb.motion_vector_count = 2;
      mb.mv_format = MV_FORMAT_FIELD;
      mb.dmv = 0;
      break;
    case 0x3:
      mb.prediction_type = PRED_TYPE_DUAL_PRIME;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FIELD;
      mb.dmv = 1;
      break;
    default:
      fprintf(stderr, "*** impossible prediction type\n");
      exit(-1);
      break;
    }
  }


  if(mb.motion_vector_count == 1) {
    if((mb.mv_format == MV_FORMAT_FIELD) && (mb.dmv != 1)) {
      mb.motion_vertical_field_select[0][s] = GETBITS(1, "motion_vertical_field_select[0][s]");
    }
    motion_vector(0, s);
  } else {
    mb.motion_vertical_field_select[0][s] = GETBITS(1, "motion_vertical_field_select[0][s]");
    motion_vector(0, s);
    mb.motion_vertical_field_select[1][s] = GETBITS(1, "motion_vertical_field_select[1][s]");
    motion_vector(1, s);
  }
}
 
/* 6.2.5.2.1 Motion vector */
void motion_vector(int r, int s)
{
  int16_t r_size;
  int16_t f;
  int16_t high;
  int16_t low;
  int16_t range;
  int16_t delta;
  int16_t prediction;
  int t;
  
  DPRINTF(2, "motion_vector(%d, %d)\n", r, s);

  mb.motion_code[r][s][0] = get_vlc(table_b10, "motion_code[r][s][0] (b10)");
  if((pic.coding_ext.f_code[s][0] != 1) && (mb.motion_code[r][s][0] != 0)) {
    r_size = pic.coding_ext.f_code[s][0] - 1;
    mb.motion_residual[r][s][0] = GETBITS(r_size, "motion_residual[r][s][0]");
  }
  if(mb.dmv == 1) {
    mb.dmvector[0] = get_vlc(table_b11, "dmvector[0] (b11)");
  }
  mb.motion_code[r][s][1] = get_vlc(table_b10, "motion_code[r][s][1] (b10)");
  // The reference code has f_code[s][0] here, that is probably wrong....
  if((pic.coding_ext.f_code[s][1] != 1) && (mb.motion_code[r][s][1] != 0)) {
    r_size = pic.coding_ext.f_code[s][1] - 1;
    mb.motion_residual[r][s][1] = GETBITS(r_size, "motion_residual_[r][s][1]");
  }
  if(mb.dmv == 1) {
    mb.dmvector[1] = get_vlc(table_b11, "dmvector[1] (b11)");
  }

  

  for(t = 0; t < 2; t++) {   
    r_size = pic.coding_ext.f_code[s][t] - 1;
    f = 1 << r_size;
    high = (16 * f) - 1;
    low = ((-16) * f);
    range = (32 * f);
    
    if((f == 1) || (mb.motion_code[r][s][t] == 0)) { 
      delta = mb.motion_code[r][s][t];
    } else { 
      delta = ((abs(mb.motion_code[r][s][t]) - 1) * f) +
	mb.motion_residual[r][s][t] + 1;
      if(mb.motion_code[r][s][t] < 0) {
	delta = - delta;
      }
    }
    
    prediction = pic.PMV[r][s][t];
    if((mb.mv_format ==  MV_FORMAT_FIELD) && (t==1) &&
       (pic.coding_ext.picture_structure ==  0x3)) {
      prediction = (pic.PMV[r][s][t]) >> 1;         /* DIV */
    }
    
    /** test **/
    mb.delta[r][s][t] = delta;
    /***/
    mb.vector[r][s][t] = prediction + delta;
    if(mb.vector[r][s][t] < low) {
      mb.vector[r][s][t] = mb.vector[r][s][t] + range;
    }
    if(mb.vector[r][s][t] > high) {
      mb.vector[r][s][t] = mb.vector[r][s][t] - range;
    }
    if((mb.mv_format ==  MV_FORMAT_FIELD) && (t==1) &&
       (pic.coding_ext.picture_structure ==  0x3)) {
      pic.PMV[r][s][t] = mb.vector[r][s][t] * 2;
    } else {
      pic.PMV[r][s][t] = mb.vector[r][s][t];
    }
  }
}


void reset_dc_dct_pred(void)
{
  
  DPRINTF(3, "Resetting dc_dct_pred\n");
  
  /* Table 7-2. Relation between intra_dc_precision and the predictor
     reset value */
  switch(pic.coding_ext.intra_dc_precision) {
  case 0:
    mb.dc_dct_pred[0] = 128;
    mb.dc_dct_pred[1] = 128;
    mb.dc_dct_pred[2] = 128;
    break;
  case 1:
    mb.dc_dct_pred[0] = 256;
    mb.dc_dct_pred[1] = 256;
    mb.dc_dct_pred[2] = 256;
    break;
  case 2:
    mb.dc_dct_pred[0] = 512;
    mb.dc_dct_pred[1] = 512;
    mb.dc_dct_pred[2] = 512;
    break;
  case 3:
    mb.dc_dct_pred[0] = 1024;
    mb.dc_dct_pred[1] = 1024;
    mb.dc_dct_pred[2] = 1024;
    break;
  default:
    fprintf(stderr, "*** reset_dc_dct_pred(), invalid intra_dc_precision\n");
    exit(-1);
    break;
  }
}

void reset_PMV()
{
  
  DPRINTF(3, "Resetting PMV\n");

  
  pic.PMV[0][0][0] = 0;
  pic.PMV[0][0][1] = 0;
  
  pic.PMV[0][1][0] = 0;
  pic.PMV[0][1][1] = 0;

  pic.PMV[1][0][0] = 0;
  pic.PMV[1][0][1] = 0;

  pic.PMV[1][1][0] = 0;
  pic.PMV[1][1][1] = 0;

}


void reset_vectors()
{

  DPRINTF(3, "Resetting motion vectors\n");
  mb.vector[0][0][0] = 0;
  mb.vector[0][0][1] = 0;
  mb.vector[1][0][0] = 0;
  mb.vector[1][0][1] = 0;
  mb.vector[0][1][0] = 0;
  mb.vector[0][1][1] = 0;
  mb.vector[1][1][0] = 0;
  mb.vector[1][1][1] = 0;

}





void reset_to_default_quantiser_matrix()
{
  memcpy(seq.header.intra_inverse_quantiser_matrix,
	 default_intra_inverse_quantiser_matrix,
	 sizeof(seq.header.intra_inverse_quantiser_matrix));

  memcpy(seq.header.non_intra_inverse_quantiser_matrix,
	 default_non_intra_inverse_quantiser_matrix,
	 sizeof(seq.header.non_intra_inverse_quantiser_matrix));
  
}

void reset_to_default_intra_quantiser_matrix()
{
  memcpy(seq.header.intra_inverse_quantiser_matrix,
	 default_intra_inverse_quantiser_matrix,
	 sizeof(seq.header.intra_inverse_quantiser_matrix));
}

void reset_to_default_non_intra_quantiser_matrix()
{
  memcpy(seq.header.non_intra_inverse_quantiser_matrix,
	 default_non_intra_inverse_quantiser_matrix,
	 sizeof(seq.header.non_intra_inverse_quantiser_matrix));
}


int sign(int16_t num)
{
  if(num > 0) {
    return 1;
  } else if(num < 0) {
    return -1;
  } else {
    return 0;
  }

}


#define WORDS_BIGENDIAN 

void display_init()
{
  int screen;
  unsigned int fg, bg;
  char *hello = "I hate X11";
  XSizeHints hint;
  XEvent xev;

  XGCValues xgcv;
  Colormap theCmap;
  XSetWindowAttributes xswa;
  unsigned long xswamask;

  int image_height = 480;
  int image_width = 720;


  mydisplay = XOpenDisplay(NULL);

  if (mydisplay == NULL)
    fprintf(stderr,"Can not open display\n");

  if (shmem_flag) {
    /* Check for availability of shared memory */
    if (!XShmQueryExtension(mydisplay)) {
      shmem_flag = 0;
      fprintf(stderr, "No shared memory available!\n");
    }
  }

  screen = DefaultScreen(mydisplay);

  hint.x = 0;
  hint.y = 0;
  hint.width = image_width;
  hint.height = image_height;
  hint.flags = PPosition | PSize;

  /* Get some colors */

  bg = WhitePixel(mydisplay, screen);
  fg = BlackPixel(mydisplay, screen);

  /* Make the window */

  XGetWindowAttributes(mydisplay, DefaultRootWindow(mydisplay), &attribs);
  bpp = attribs.depth;
  if (bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32) {
    fprintf(stderr,"Only 15,16,24, and 32bpp supported. Trying 24bpp!\n");
    bpp = 24;
  }
  //BEGIN HACK
  //mywindow = XCreateSimpleWindow(mydisplay, DefaultRootWindow(mydisplay),
  //hint.x, hint.y, hint.width, hint.height, 4, fg, bg);
  //
  XMatchVisualInfo(mydisplay,screen,bpp,TrueColor,&vinfo);
  printf("visual id is  %lx\n",vinfo.visualid);

  theCmap   = XCreateColormap(mydisplay, RootWindow(mydisplay,screen), 
			      vinfo.visual, AllocNone);

  xswa.background_pixel = 0;
  xswa.border_pixel     = 1;
  xswa.colormap         = theCmap;
  xswamask = CWBackPixel | CWBorderPixel | CWColormap;

  mywindow = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
			   hint.x, hint.y, hint.width, hint.height, 0, bpp,
			   CopyFromParent, vinfo.visual, xswamask, &xswa);

  window_ref1 = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
			      hint.x, hint.y, hint.width, hint.height, 0, bpp,
			      CopyFromParent, vinfo.visual, xswamask, &xswa);

  window_ref2 = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
			      hint.x, hint.y, hint.width, hint.height, 0, bpp,
			      CopyFromParent, vinfo.visual, xswamask, &xswa);


  window_stat = XCreateSimpleWindow(mydisplay, RootWindow(mydisplay,screen),
				    hint.x, hint.y, 200, 200, 0,
				    0);

  XSelectInput(mydisplay, mywindow, StructureNotifyMask | KeyPressMask | ButtonPressMask | ExposureMask);
  XSelectInput(mydisplay, window_ref1, StructureNotifyMask | KeyPressMask | ButtonPressMask | ExposureMask);
  XSelectInput(mydisplay, window_ref2, StructureNotifyMask | KeyPressMask | ButtonPressMask | ExposureMask);
  XSelectInput(mydisplay, window_stat, StructureNotifyMask | KeyPressMask | ButtonPressMask | ExposureMask);

  /* Tell other applications about this window */

  XSetStandardProperties(mydisplay, mywindow, hello, hello, None, NULL, 0, &hint);
  XSetStandardProperties(mydisplay, window_ref1, "ref1", "ref1", None, NULL, 0, &hint);
  XSetStandardProperties(mydisplay, window_ref2, "ref2", "ref2", None, NULL, 0, &hint);
  XSetStandardProperties(mydisplay, window_stat, "stat", "stat", None, NULL, 0, &hint);

  /* Map window. */

  XMapWindow(mydisplay, mywindow);
  XMapWindow(mydisplay, window_ref1);
  XMapWindow(mydisplay, window_ref2);
  XMapWindow(mydisplay, window_stat);

  /* Wait for map. */
  do {
    XNextEvent(mydisplay, &xev);
  }
  while (xev.type != MapNotify || xev.xmap.event != mywindow);
   
  //   XSelectInput(mydisplay, mywindow, NoEventMask);

  XFlush(mydisplay);
  XSync(mydisplay, False);
   
   
  mygc = XCreateGC(mydisplay, mywindow, 0L, &xgcv);
  statgc = XCreateGC(mydisplay, window_stat, 0L, &xgcv);
   
   
  /*   
       myximage = XGetImage(mydisplay, mywindow, 0, 0,
       image_width, image_height, AllPlanes, ZPixmap);
       ImageData = myximage->data;
   
  */
  //   bpp = myximage->bits_per_pixel;
  // If we have blue in the lowest bit then obviously RGB 
  //mode = ((myximage->blue_mask & 0x01) != 0) ? 1 : 2;
#ifdef WORDS_BIGENDIAN 
  // if (myximage->byte_order != MSBFirst)
#else
  //  if (myximage->byte_order != LSBFirst) 
#endif
  //   {
  //	 fprintf( stderr, "No support for non-native XImage byte order!\n" );
  //	 exit(1);
  //   }
  //   yuv2rgb_init(bpp, mode);
   
}










void Display_Image(Window win, XImage *myximage, unsigned char *ImageData)
{

  if (shmem_flag) {
    XShmPutImage(mydisplay, win, mygc, myximage, 
                 0, 0, 0, 0, myximage->width, myximage->height, 1);
  } else {
    XPutImage(mydisplay, win, mygc, myximage, 0, 0,
              0, 0, myximage->width, myximage->height);
  }
  XFlush(mydisplay);
}



void motion_comp()
{
  int width,x,y;
  int stride;
  int d;
  uint8_t *dst_y,*dst_u,*dst_v;
  int half_flag_y[2];
  int half_flag_uv[2];
  int int_vec_y[2];
  int int_vec_uv[2];

  uint8_t *pred_y;
  uint8_t *pred_u;
  uint8_t *pred_v;
  int field;


  width = seq.horizontal_size;

  
  DPRINTF(2, "dct_type: %d\n", mb.modes.dct_type);
  //handle interlaced blocks
  if (mb.modes.dct_type) { // skicka med dct_type som argument
    d = 1;
    stride = width * 2;
  } else {
    d = 8;
    stride = width;
  }

  x = seq.mb_column;
  y = seq.mb_row;
    
  dst_y = &dst_image->y[x * 16 + y * width * 16];
  dst_u = &dst_image->u[x * 8 + y * width/2 * 8];
  dst_v = &dst_image->v[x * 8 + y * width/2 * 8];
    
  //mb.prediction_type = PRED_TYPE_FIELD_BASED;
    
  if(mb.modes.macroblock_motion_forward) {

    int apa, i;


    if(mb.prediction_type == PRED_TYPE_DUAL_PRIME) {
      fprintf(stderr, "**** DP remove this and check if working\n");
      //exit(-1);
    }


    if(mb.prediction_type == PRED_TYPE_FIELD_BASED) {
      apa = mb.motion_vector_count;
    } else { 
      apa = 1;
    }


    for(i = 0; i < apa; i++) {
      
      field = mb.motion_vertical_field_select[i][0];
      
      DPRINTF(2, "forward_motion_comp\n");

      half_flag_y[0] = (mb.vector[i][0][0] & 1);
      half_flag_y[1] = (mb.vector[i][0][1] & 1);
      half_flag_uv[0] = ((mb.vector[i][0][0]/2) & 1);
      half_flag_uv[1] = ((mb.vector[i][0][1]/2) & 1);
      int_vec_y[0] = (mb.vector[i][0][0] >> 1) + (signed int)x * 16;
      int_vec_y[1] = (mb.vector[i][0][1] >> 1)*apa + (signed int)y * 16;
      int_vec_uv[0] = ((mb.vector[i][0][0]/2) >> 1)  + x * 8;
      int_vec_uv[1] = ((mb.vector[i][0][1]/2) >> 1)*apa + y * 8;
      //int_vec_uv[0] = int_vec_y[0] / 2 ;
      //  int_vec_uv[1] = int_vec_y[1] / 2 ;
    
    



      DPRINTF(3, "start: 0, end: %d\n",
	      seq.horizontal_size * seq.vertical_size);
      
      DPRINTF(3, "p_vec x: %d, y: %d\n",
	      (mb.vector[i][0][0] >> 1),
	      (mb.vector[i][0][1] >> 1));
    
      pred_y  =
	&ref_image1->y[int_vec_y[0] + int_vec_y[1] * width];
    
      DPRINTF(3, "ypos: %d\n",
	      int_vec_y[0] + int_vec_y[1] * width);
    
      DPRINTF(3, "start: 0, end: %d\n",
	      seq.horizontal_size * seq.vertical_size/4);
    
      pred_u =
	&ref_image1->u[int_vec_uv[0] + int_vec_uv[1] * width/2];
    
    
      DPRINTF(3, "uvpos: %d\n",
	      int_vec_uv[0] + int_vec_uv[1] * width/2);
    
      pred_v =
	&ref_image1->v[int_vec_uv[0] + int_vec_uv[1] * width/2];
    
      DPRINTF(3, "x: %d, y: %d\n", x, y);
    
	
      if (half_flag_y[0] && half_flag_y[1]) {
	if(apa == 2) {
	  mlib_VideoInterpXY_U8_U8_16x8(dst_y + i*width, pred_y+field*width,
					width*2, width*2);
	} else {
	  mlib_VideoInterpXY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
	}
      } else if (half_flag_y[0]) {
	if(apa == 2) {
	  mlib_VideoInterpX_U8_U8_16x8(dst_y + i*width, pred_y+field*width,
				       width*2, width*2);
	} else {
	  mlib_VideoInterpX_U8_U8_16x16(dst_y,  pred_y,  width,   width);
	}
      } else if (half_flag_y[1]) {
	if(apa == 2) {
	  mlib_VideoInterpY_U8_U8_16x8(dst_y + i*width, pred_y+field*width,
				       width*2, width*2);
	} else {
	  mlib_VideoInterpY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
	}
      } else {
	if(apa == 2) {
	  mlib_VideoCopyRef_U8_U8_16x8(dst_y + i*width, pred_y+field*width,
				       width*2);
	} else {
	  mlib_VideoCopyRef_U8_U8_16x16(dst_y,  pred_y,  width);
	}
      }

      if (half_flag_uv[0] && half_flag_uv[1]) {
	if(apa == 2) {
	  mlib_VideoInterpXY_U8_U8_8x4(dst_u + i*width/2, pred_u+field*width/2,
				       width*2/2, width*2/2);
	  mlib_VideoInterpXY_U8_U8_8x4(dst_v + i*width/2, pred_v+field*width/2,
				       width*2/2, width*2/2);
	} else {
	  mlib_VideoInterpXY_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
	  mlib_VideoInterpXY_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
	}
      } else if (half_flag_uv[0]) {
	if(apa == 2) {
	  mlib_VideoInterpX_U8_U8_8x4(dst_u + i*width/2, pred_u+field*width/2,
				      width*2/2, width*2/2);
	  mlib_VideoInterpX_U8_U8_8x4(dst_v + i*width/2, pred_v+field*width/2,
				      width*2/2, width*2/2);
	} else {
	  mlib_VideoInterpX_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
	  mlib_VideoInterpX_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
	}
      } else if (half_flag_uv[1]) {
	if(apa == 2) {
	  mlib_VideoInterpY_U8_U8_8x4(dst_u + i*width/2, pred_u+field*width/2,
				      width*2/2, width*2/2);
	  mlib_VideoInterpY_U8_U8_8x4(dst_v + i*width/2, pred_v+field*width/2,
				      width*2/2, width*2/2);
	} else {
	  mlib_VideoInterpY_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
	  mlib_VideoInterpY_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
	}
      } else {
	if(apa == 2) {
	  mlib_VideoCopyRef_U8_U8_8x4(dst_u + i*width/2, pred_u+field*width/2,
				      width*2/2);
	  mlib_VideoCopyRef_U8_U8_8x4(dst_v + i*width/2, pred_v+field*width/2,
				      width*2/2);
	} else {
	  mlib_VideoCopyRef_U8_U8_8x8  (dst_u, pred_u, width/2);
	  mlib_VideoCopyRef_U8_U8_8x8  (dst_v, pred_v, width/2);
	}
      }
    }
  }   
  if(mb.modes.macroblock_motion_backward) {
    int apa, i;
    
    DPRINTF(2, "backward_motion_comp\n");
    
    if(mb.prediction_type == PRED_TYPE_FIELD_BASED) {
      apa = mb.motion_vector_count;
    } else { 
      apa = 1;
    }
    
    for(i = 0; i < apa; i++) {
      
      field = mb.motion_vertical_field_select[i][1];
      
      half_flag_y[0]   = (mb.vector[i][1][0] & 1);
      half_flag_y[1]   = (mb.vector[i][1][1] & 1);
      half_flag_uv[0] = ((mb.vector[i][1][0]/2) & 1);
      half_flag_uv[1] = ((mb.vector[i][1][1]/2) & 1);
      int_vec_y[0] = (mb.vector[i][1][0] >> 1) + (signed int)x * 16;
      int_vec_y[1] = (mb.vector[i][1][1] >> 1)*apa + (signed int)y * 16;
      int_vec_uv[0] = ((mb.vector[i][1][0]/2) >> 1)  + x * 8;
      int_vec_uv[1] = ((mb.vector[i][1][1]/2) >> 1)*apa  + y * 8;
      //int_vec_uv[0] = int_vec_y[0] / 2;
      //int_vec_uv[1] = int_vec_y[1] / 2;
      
      pred_y  =
	&ref_image2->y[int_vec_y[0] + int_vec_y[1] * width];
      pred_u =
	&ref_image2->u[int_vec_uv[0] + int_vec_uv[1] * width/2];
      pred_v =
	&ref_image2->v[int_vec_uv[0] + int_vec_uv[1] * width/2];
      
      if(mb.modes.macroblock_motion_forward) {
	if (half_flag_y[0] && half_flag_y[1]) {
	  if(apa == 2) {
	    mlib_VideoInterpAveXY_U8_U8_16x8(dst_y + i*width,
					     pred_y+field*width,
					     width*2, width*2);
	  } else {
	    mlib_VideoInterpAveXY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
	  }
	} else if (half_flag_y[0]) {
	  if(apa == 2) {
	    mlib_VideoInterpAveX_U8_U8_16x8(dst_y + i*width,
					    pred_y+field*width,
					    width*2, width*2);
	  } else {
	    mlib_VideoInterpAveX_U8_U8_16x16(dst_y,  pred_y,  width,   width);
	  }
	} else if (half_flag_y[1]) {
	  if(apa == 2) {
	    mlib_VideoInterpAveY_U8_U8_16x8(dst_y + i*width,
					    pred_y+field*width,
					    width*2, width*2);
	  } else {
	    mlib_VideoInterpAveY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
	  }
	} else {
	  if(apa == 2) {
	    mlib_VideoCopyRefAve_U8_U8_16x8(dst_y + i*width,
					    pred_y+field*width,
					    width*2);
	  } else {
	    mlib_VideoCopyRefAve_U8_U8_16x16(dst_y,  pred_y,  width);
	  }
	}
	if (half_flag_uv[0] && half_flag_uv[1]) {
	  if(apa == 2) {
	    mlib_VideoInterpAveXY_U8_U8_8x4(dst_u + i*width/2,
					    pred_u+field*width/2,
					    width*2/2, width*2/2);
	    mlib_VideoInterpAveXY_U8_U8_8x4(dst_v + i*width/2,
					    pred_v+field*width/2,
					    width*2/2, width*2/2);
	  } else {
	    mlib_VideoInterpAveXY_U8_U8_8x8(dst_u, pred_u, width/2, width/2);
	    mlib_VideoInterpAveXY_U8_U8_8x8(dst_v, pred_v, width/2, width/2);
	  }
	} else if (half_flag_uv[0]) {
	  if(apa == 2) {
	    mlib_VideoInterpAveX_U8_U8_8x4(dst_u + i*width/2,
					   pred_u+field*width/2,
					   width*2/2, width*2/2);
	    mlib_VideoInterpAveX_U8_U8_8x4(dst_v + i*width/2,
					   pred_v+field*width/2,
					   width*2/2, width*2/2);
	  } else {
	    mlib_VideoInterpAveX_U8_U8_8x8(dst_u, pred_u, width/2, width/2);
	    mlib_VideoInterpAveX_U8_U8_8x8(dst_v, pred_v, width/2, width/2);
	  }
	} else if (half_flag_uv[1]) {
	  if(apa == 2) {
	    mlib_VideoInterpAveY_U8_U8_8x4(dst_u + i*width/2,
					   pred_u+field*width/2,
					   width*2/2, width*2/2);
	    mlib_VideoInterpAveY_U8_U8_8x4(dst_v + i*width/2,
					   pred_v+field*width/2,
					   width*2/2, width*2/2);
	  } else {
	    mlib_VideoInterpAveY_U8_U8_8x8(dst_u, pred_u, width/2, width/2);
	    mlib_VideoInterpAveY_U8_U8_8x8(dst_v, pred_v, width/2, width/2);
	  }
	} else {
	  if(apa == 2) {
	    mlib_VideoCopyRefAve_U8_U8_8x4(dst_u + i*width/2,
					   pred_u+field*width/2,
					   width*2/2);
	    mlib_VideoCopyRefAve_U8_U8_8x4(dst_v + i*width/2,
					   pred_v+field*width/2,
					   width*2/2);
	  } else {
	    mlib_VideoCopyRefAve_U8_U8_8x8(dst_u, pred_u, width/2);
	    mlib_VideoCopyRefAve_U8_U8_8x8(dst_v, pred_v, width/2);
	  }
	}
      } else {
	if (half_flag_y[0] && half_flag_y[1]) {
	  if(apa == 2) {
	    mlib_VideoInterpXY_U8_U8_16x8(dst_y + i*width, pred_y+field*width,
					  width*2, width*2);
	  } else {
	    mlib_VideoInterpXY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
	  }
	} else if (half_flag_y[0]) {
	  if(apa == 2) {
	    mlib_VideoInterpX_U8_U8_16x8(dst_y + i*width, pred_y+field*width,
					 width*2, width*2);
	  } else {
	    mlib_VideoInterpX_U8_U8_16x16(dst_y,  pred_y,  width,   width);
	  }
	} else if (half_flag_y[1]) {
	  if(apa == 2) {
	    mlib_VideoInterpY_U8_U8_16x8(dst_y + i*width, pred_y+field*width,
					 width*2, width*2);
	  } else {
	    mlib_VideoInterpY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
	  }
	} else {
	  if(apa == 2) {
	    mlib_VideoCopyRef_U8_U8_16x8(dst_y + i*width, pred_y+field*width,
					 width*2);
	  } else {
	    mlib_VideoCopyRef_U8_U8_16x16(dst_y,  pred_y,  width);
	  }
	}
	if (half_flag_uv[0] && half_flag_uv[1]) {
	  if(apa == 2) {
	    mlib_VideoInterpXY_U8_U8_8x4(dst_u + i*width/2,
					 pred_u+field*width/2,
					 width*2/2, width*2/2);
	    mlib_VideoInterpXY_U8_U8_8x4(dst_v + i*width/2,
					 pred_v+field*width/2, 
					 width*2/2, width*2/2);
	  } else {
	    mlib_VideoInterpXY_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
	    mlib_VideoInterpXY_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
	  }
	} else if (half_flag_uv[0]) {
	  if(apa == 2) {
	    mlib_VideoInterpX_U8_U8_8x4(dst_u + i*width/2,
					pred_u+field*width/2,
					width*2/2, width*2/2);
	    mlib_VideoInterpX_U8_U8_8x4(dst_v + i*width/2,
					pred_v+field*width/2, 
					width*2/2, width*2/2);
	  } else {
	    mlib_VideoInterpX_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
	    mlib_VideoInterpX_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
	  }
	} else if (half_flag_uv[1]) {
	  if(apa == 2) {
	    mlib_VideoInterpY_U8_U8_8x4(dst_u + i*width/2,
					pred_u+field*width/2,
					width*2/2, width*2/2);
	    mlib_VideoInterpY_U8_U8_8x4(dst_v + i*width/2,
					pred_v+field*width/2, 
					width*2/2, width*2/2);
	  } else {
	    mlib_VideoInterpY_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
	    mlib_VideoInterpY_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
	  }
	} else {
	  if(apa == 2) {
	    mlib_VideoCopyRef_U8_U8_8x4(dst_u + i*width/2,
					pred_u+field*width/2,
					width*2/2);
	    mlib_VideoCopyRef_U8_U8_8x4(dst_v + i*width/2,
					pred_v+field*width/2, 
					width*2/2);
	  } else {
	    mlib_VideoCopyRef_U8_U8_8x8  (dst_u, pred_u, width/2);
	    mlib_VideoCopyRef_U8_U8_8x8  (dst_v, pred_v, width/2);
	  }
	}
      }
    }
  }  
  
  if(mb.pattern_code[0])
    mlib_VideoAddBlock_U8_S16(dst_y, mb.f[0], stride);
  
  if(mb.pattern_code[1])
    mlib_VideoAddBlock_U8_S16(dst_y + 8, mb.f[1], stride);
      
  if(mb.pattern_code[2])
    mlib_VideoAddBlock_U8_S16(dst_y + width * d, mb.f[2], stride);
  
  if(mb.pattern_code[3])
    mlib_VideoAddBlock_U8_S16(dst_y + width * d + 8, mb.f[3], stride);
  
  if(mb.pattern_code[4])
    mlib_VideoAddBlock_U8_S16(dst_u, mb.f[4], width/2);
  
  if(mb.pattern_code[5])
    mlib_VideoAddBlock_U8_S16(dst_v, mb.f[5], width/2);
  
  
}


void quant_matrix_extension()
{
  fprintf(stderr, "***ni quant_matrix_extension()\n");
  exit(-1);
}


void picture_display_extension()
{
  uint8_t extension_start_code_identifier;
  uint16_t frame_centre_horizontal_offset;
  uint16_t frame_centre_vertical_offset;
  uint8_t number_of_frame_centre_offsets;
  int i;
  
  if((seq.ext.progressive_sequence == 1) || 
     ((pic.coding_ext.picture_structure == 0x1) ||
      (pic.coding_ext.picture_structure == 0x2))) {
    number_of_frame_centre_offsets = 1;
  } else {
    if(pic.coding_ext.repeat_first_field == 1) {
      number_of_frame_centre_offsets = 3;
    } else {
      number_of_frame_centre_offsets = 2;
    }
  }
  
  DPRINTF(2, "picture_display_extension()\n");
  extension_start_code_identifier=GETBITS(4,"extension_start_code_identifier");

  for(i = 0; i < number_of_frame_centre_offsets; i++) {
    frame_centre_horizontal_offset = GETBITS(16, "frame_centre_horizontal_offset");
    marker_bit();
    frame_centre_vertical_offset = GETBITS(16, "frame_centre_vertical_offset");
    marker_bit();
    
  }
  
  next_start_code();
  
}

void picture_spatial_scalable_extension()
{
  fprintf(stderr, "***ni picture_spatial_scalable_extension()\n");
  exit(-1);
}

void picture_temporal_scalable_extension()
{
  fprintf(stderr, "***ni picture_temporal_scalable_extension()\n");
  exit(-1);
}

void sequence_scalable_extension()
{
  fprintf(stderr, "***ni sequence_scalable_extension()\n");
  exit(-1);
}

void sequence_display_extension()
{

  uint8_t extension_start_code_identifier;
  uint8_t video_format;
  uint8_t colour_description;
  
  uint8_t colour_primaries;
  uint8_t transfer_characteristics;
  uint8_t matrix_coefficients;
  
  uint16_t display_horizontal_size;
  uint16_t display_vertical_size;
  
  
  DPRINTF(2, "sequence_display_extension()\n");
  extension_start_code_identifier=GETBITS(4,"extension_start_code_identifier");
  video_format = GETBITS(3, "video_format");
  colour_description = GETBITS(1, "colour_description");

#ifdef DEBUG
  /* Table 6-6. Meaning of video_format */
  DPRINTF(2, "video_format: ");
  switch(video_format) {
  case 0x0:
    DPRINTF(2, "component\n");
    break;
  case 0x1:
    DPRINTF(2, "PAL\n");
    break;
  case 0x2:
    DPRINTF(2, "NTSC\n");
    break;
  case 0x3:
    DPRINTF(2, "SECAM\n");
    break;
  case 0x4:
    DPRINTF(2, "MAC\n");
    break;
  case 0x5:
    DPRINTF(2, "Unspecified video format\n");
    break;
  default:
    DPRINTF(2, "reserved\n");
    break;
  }
#endif



  if(colour_description) {
    colour_primaries = GETBITS(8, "colour_primaries");
    transfer_characteristics = GETBITS(8, "transfer_characteristics");
    matrix_coefficients = GETBITS(8, "matrix_coefficients");
    
#ifdef DEBUG
    /* Table 6-7. Colour Primaries */
    DPRINTF(2, "colour primaries (Table 6-7): ");
    switch(colour_primaries) {
    case 0x0:
      DPRINTF(2, "(forbidden)\n");
      break;
    case 0x1:
      DPRINTF(2, "ITU-R 709\n");
      break;
    case 0x2:
      DPRINTF(2, "Unspecified Video\n");
      break;
    case 0x3:
      DPRINTF(2, "reserved\n");
      break;
    case 0x4:
      DPRINTF(2, "ITU-R 624-4 System M\n");
      break;
    case 0x5:
      DPRINTF(2, "ITU-R 624-4 System B,G\n");
      break;
    case 0x6:
      DPRINTF(2, "SMPTE 170M\n");
      break;
    case 0x7:
      DPRINTF(2, "SMPTE 240M\n");
      break;
    default:
      DPRINTF(2, "reserved\n");
      break;
    }
#endif
    
#ifdef DEBUG
    /* Table 6-8. Transfer Characteristics */
    DPRINTF(2, "transfer characteristics (Table 6-8): ");
    switch(transfer_characteristics) {
    case 0x0:
      DPRINTF(2, "(forbidden)\n");
      break;
    case 0x1:
      DPRINTF(2, "ITU-R 709\n");
      break;
    case 0x2:
      DPRINTF(2, "Unspecified Video\n");
      break;
    case 0x3:
      DPRINTF(2, "reserved\n");
      break;
    case 0x4:
      DPRINTF(2, "ITU-R 624-4 System M\n");
      break;
    case 0x5:
      DPRINTF(2, "ITU-R 624-4 System B,G\n");
      break;
    case 0x6:
      DPRINTF(2, "SMPTE 170M\n");
      break;
    case 0x7:
      DPRINTF(2, "SMPTE 240M\n");
      break;
    case 0x8:
      DPRINTF(2, "Linear transfer characteristics\n");
      break;
    default:
      DPRINTF(2, "reserved\n");
      break;
    }
#endif
    
#ifdef DEBUG
    /* Table 6-9. Matrix Coefficients */
    DPRINTF(2, "matrix coefficients (Table 6-9): ");
    switch(matrix_coefficients) {
    case 0x0:
      DPRINTF(2, "(forbidden)\n");
      break;
    case 0x1:
      DPRINTF(2, "ITU-R 709\n");
      break;
    case 0x2:
      DPRINTF(2, "Unspecified Video\n");
      break;
    case 0x3:
      DPRINTF(2, "reserved\n");
      break;
    case 0x4:
      DPRINTF(2, "FCC\n");
      break;
    case 0x5:
      DPRINTF(2, "ITU-R 624-4 System B,G\n");
      break;
    case 0x6:
      DPRINTF(2, "SMPTE 170M\n");
      break;
    case 0x7:
      DPRINTF(2, "SMPTE 240M\n");
      break;
    default:
      DPRINTF(2, "reserved\n");
      break;
    }
#endif


  }

  display_horizontal_size = GETBITS(14, "display_horizontal_size");
  marker_bit();
  display_vertical_size = GETBITS(14, "display_vertical_size");

  DPRINTF(2, "display_horizontal_size: %d\n", display_horizontal_size);
  DPRINTF(2, "display_vertical_size: %d\n", display_vertical_size);
  

  next_start_code();

}



typedef struct {
  Window win;
  unsigned char *data;
  XImage *ximage;
  yuv_image_t *image;
  int grid;
  int color_grid;
  macroblock_t *mbs;
} debug_win;

debug_win windows[3];


void compute_frame(yuv_image_t *image, unsigned char *data)
{
  mlib_VideoColorYUV2ABGR420(data,
			     image->y,
			     image->u,
			     image->v,
			     seq.horizontal_size,
			     seq.vertical_size,
			     seq.horizontal_size*4, //TODO
			     seq.horizontal_size,
			     seq.horizontal_size/2);
}
 
void add_grid(unsigned char *data)
{
  int m,n;
  
  for(m = 0; m < 480; m++) {
    if(m%16 == 0) {
      for(n = 0; n < 720*4; n+=4) {
	data[m*720*4+n+1] = 127;
	data[m*720*4+n+2] = 127;
	data[m*720*4+n+3] = 127;
      }
    } else {
      for(n = 0; n < 720*4; n+=16*4) {
	data[m*720*4+n+1] = 127;
	data[m*720*4+n+2] = 127;
	data[m*720*4+n+3] = 127;
      }
    }
  }
}

void add_2_box_sides(unsigned char *data,
		     unsigned char r,
		     unsigned char g,
		     unsigned char b)
{
  int n;
  
  for(n = 0; n < 16*4; n+=4) {
    data[n+1] = b;
    data[n+2] = g;
    data[n+3] = r;
  }
  
  for(n = 0; n < 720*4*16; n+=720*4) {
    data[n+1] = b;
    data[n+2] = g;
    data[n+3] = r;
  }
  
  return;
}

void add_color_grid(debug_win *win)
{
  int m;
  
  for(m = 0; m < 30*45; m++) {
    if(win->mbs[m].skipped) {

      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      150, 150, 150);
      
    } else if(win->mbs[m].modes.macroblock_intra) {

      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      0, 255, 255);
      
    } else if(win->mbs[m].modes.macroblock_motion_forward &&
	      win->mbs[m].modes.macroblock_motion_backward) {
      
      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      255, 0, 0);
      
    } else if(win->mbs[m].modes.macroblock_motion_forward) {
      
      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      255, 255, 0);

    } else if(win->mbs[m].modes.macroblock_motion_backward) {
      
      add_2_box_sides(&(win->data[m/45*720*4*16+(m%45)*16*4]),
		      0, 0, 255);
    }
  }
}

      


debug_win windows[3];

void draw_win(debug_win *dwin);

void frame_done()
{
  XEvent ev;
  int nextframe = 0;

  // TODO move out
  windows[0].win = mywindow;
  windows[0].data = ImageData;
  windows[0].ximage = myximage;
  windows[0].image = current_image;
  windows[0].mbs = cur_mbs;
  windows[1].win = window_ref1;
  windows[1].data = imagedata_ref1;
  windows[1].ximage = ximage_ref1;
  windows[1].image = ref_image1;
  windows[1].mbs = ref1_mbs;
  windows[2].win = window_ref2;
  windows[2].data = imagedata_ref2;
  windows[2].ximage = ximage_ref2;
  windows[2].image = ref_image2;
  windows[2].mbs = ref2_mbs;
  


  switch(pic.header.picture_coding_type) {
  case 0x1:
  case 0x2:
    if(show_window[1]) {
      draw_win(&windows[1]);
    }
    if(show_window[2]) {
      draw_win(&windows[2]);
    }
    //compute_frame(ref_image1, imagedata_ref1);
    //add_grid(imagedata_ref1);
    //Display_Image(window_ref1, ximage_ref1, imagedata_ref1);
    //compute_frame(ref_image2, imagedata_ref2);
    //add_grid(imagedata_ref2);
    //Display_Image(window_ref2, ximage_ref2, imagedata_ref2);
    
    
  case 0x3:
    if(show_window[0]) {
      draw_win(&windows[0]);
    }
    //    compute_frame(current_image, ImageData);
    //Display_Image(mywindow, myximage, ImageData);
    break;
  default:
    break;
  }

  while(!nextframe) {

    if(run) {
      nextframe = 1;
      if(XCheckMaskEvent(mydisplay, 0xFFFFFFFF, &ev) == False) {
	continue;
      }
    } else {
      XNextEvent(mydisplay, &ev);
    }
      
    switch(ev.type) {
    case Expose:
      {
	int n;
	for(n = 0; n < 3; n++) {
	  if(windows[n].win == ev.xexpose.window) {
	    if(show_window[n]) {
	      draw_win(&(windows[n]));
	    }
	    break;
	  }
	}
      }
      break;
    case ButtonPress:
      switch(ev.xbutton.button) {
      case 0x1:
	{
	  char text[64];
	  int n;
	  for(n = 0; n < 3; n++) {
	    if(windows[n].win == ev.xbutton.window) {
	      break;
	    }
	  }


	  snprintf(text, 16, "%4d, %4d", ev.xbutton.x, ev.xbutton.y);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 0*13, text, strlen(text));
	  snprintf(text, 16, "block: %4d", ev.xbutton.x/16+ev.xbutton.y/16*45);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 1*13, text, strlen(text));
	  snprintf(text, 16, "intra: %3s", (windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].modes.macroblock_intra ? "yes" : "no"));
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 2*13, text, strlen(text));

	  snprintf(text, 32, "vector[0][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 3*13, text, strlen(text));

	  snprintf(text, 32, "vector[0][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 4*13, text, strlen(text));


	  snprintf(text, 32, "vector[1][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 5*13, text, strlen(text));

	  snprintf(text, 32, "vector[1][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 6*13, text, strlen(text));


	  snprintf(text, 32, "vector[0][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 7*13, text, strlen(text));

	  snprintf(text, 32, "vector[0][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[0][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 8*13, text, strlen(text));


	  snprintf(text, 32, "vector[1][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 9*13, text, strlen(text));

	  snprintf(text, 32, "vector[1][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].vector[1][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 10*13, text, strlen(text));



	  snprintf(text, 32, "field_select[0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 11*13, text, strlen(text));

	  snprintf(text, 32, "field_select[0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 12*13, text, strlen(text));

	  snprintf(text, 32, "field_select[1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 13*13, text, strlen(text));

	  snprintf(text, 32, "field_select[1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vertical_field_select[1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 14*13, text, strlen(text));

	  snprintf(text, 32, "motion_vector_count: %2d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].motion_vector_count);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 15*13, text, strlen(text));


	  snprintf(text, 32, "delta[0][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 16*13, text, strlen(text));

	  snprintf(text, 32, "delta[0][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 17*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][0][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][0][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 18*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][0][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][0][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 19*13, text, strlen(text));

	  snprintf(text, 32, "delta[0][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 20*13, text, strlen(text));

	  snprintf(text, 32, "delta[0][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[0][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 21*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][1][0]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][1][0]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 22*13, text, strlen(text));

	  snprintf(text, 32, "delta[1][1][1]: %4d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].delta[1][1][1]);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 23*13, text, strlen(text));

	  snprintf(text, 32, "skipped: %2d", windows[n].mbs[ev.xbutton.x/16+ev.xbutton.y/16*45].skipped);
	  XDrawImageString(mydisplay, window_stat, statgc, 10, 24*13, text, strlen(text));


	}
	break;
      case 0x2:
	break;
      case 0x3:
	break;
      }
      break;
    case KeyPress:
      {
	int n;
	char buff[2];
	static int debug_change = 0;
	static int window_change = 0;

	XLookupString(&(ev.xkey), buff, 2, NULL, NULL);
	buff[1] = '\0';
	switch(buff[0]) {
	case 'n':
	  nextframe = 1;
	  break;
	case 'g':
	  for(n = 0; n < 3; n++) {
	    if(ev.xkey.window == windows[n].win) {
	      windows[n].grid = !windows[n].grid;
	      if(show_window[n]) {
		draw_win(&(windows[n]));
	      }
	      break;
	    }
	  }
	  break;
	case 'c':
	  for(n = 0; n < 3; n++) {
	    if(ev.xkey.window == windows[n].win) {
	      windows[n].color_grid = !windows[n].color_grid;
	      if(show_window[n]) {
		draw_win(&(windows[n]));
	      }
	      break;
	    }
	  }
	  break;
	case 'd':
	  debug_change = 2;
	  break;
	case 'w':
	  window_change = 2;
	  break;
	case 's':
	  show_stat = !show_stat;
	  break;
	case 'r':
	  run = !run;
	  nextframe = 1;
	  break;
	case 'q':
	  exit_program();
	  break;
	default:
	  if(debug_change) {
	    debug = atoi(&buff[0]);
	  }
	  if(window_change) {
	    int w;
	    w = atoi(&buff[0]);
	    if((w >= 0) && (w < 3)) {
	      show_window[w] = !show_window[w];
	    }
	  }
	  break;
	    
	}
	if(debug_change > 0) {
	  debug_change--;
	}
	if(window_change > 0) {
	  window_change--;
	}
      }
      break;
    default:
      break;
    }
      

  }

  return;

}

void draw_win(debug_win *dwin)
{
  
  compute_frame(dwin->image, dwin->data);
  if(dwin->grid) {
    if(dwin->color_grid) {
      add_color_grid(dwin);
    } else {
      add_grid(dwin->data);
    }
  }
  Display_Image(dwin->win, dwin->ximage, dwin->data);

  return;
}

void exit_program()
{
  
  XShmDetach (mydisplay, &shm_info);
  XShmDetach (mydisplay, &shm_info_ref1);
  XShmDetach (mydisplay, &shm_info_ref2);
  XDestroyImage (myximage);
  XDestroyImage (ximage_ref1);
  XDestroyImage (ximage_ref2);
  shmdt (shm_info.shmaddr);
  shmdt (shm_info_ref1.shmaddr);
  shmdt (shm_info_ref2.shmaddr);
  shmctl (shm_info.shmid, IPC_RMID, 0);
  shmctl (shm_info_ref1.shmid, IPC_RMID, 0);
  shmctl (shm_info_ref2.shmid, IPC_RMID, 0);

  {
    int n;
    /*
      double percent = 0.0;
    
      for(n = 0; n < 128; n++) {
      fprintf(stderr, "[%d] : %d : %f\n", n, dctstat[n],
      percent+=(double)dctstat[n]/(double)total_calls);
      }

    */
    for(n=0; n<4; n++) {
      fprintf(stderr,"frames: %.0f\n", num_pic[n]);
      fprintf(stderr,"max time: %.4f\n", time_max[n]);
      fprintf(stderr,"min time: %.4f\n", time_min[n]);
      fprintf(stderr,"fps: %.4f\n", num_pic[n]/time_pic[n]);
    }
  }

  
#ifdef STATS 
  
  
  fprintf(stderr, "intra_mtrx_chg_possible: %d\n",
	  stats_intra_inverse_quantiser_matrix_changes_possible);
  fprintf(stderr, "intra_mtrx_loaded: %d\n",
	  stats_intra_inverse_quantiser_matrix_loaded_nr);
  fprintf(stderr, "intra_mtrx_reset: %d\n",
	  stats_intra_inverse_quantiser_matrix_reset_nr);

  fprintf(stderr, "non_intra_mtrx_chg_possible: %d\n",
	  stats_non_intra_inverse_quantiser_matrix_changes_possible);
  fprintf(stderr, "non_intra_mtrx_loaded: %d\n",
	  stats_non_intra_inverse_quantiser_matrix_loaded_nr);
  fprintf(stderr, "non_intra_mtrx_reset: %d\n",
	  stats_non_intra_inverse_quantiser_matrix_reset_nr);
  

  fprintf(stderr, "stats_quantiser_scale_possible: %d\n",
	  stats_quantiser_scale_possible);
  fprintf(stderr, "stats_quantiser_scale_nr: %d\n",
	  stats_quantiser_scale_nr);

  fprintf(stderr, "stats_intra_quantiser_scale_possible: %d\n",
	  stats_intra_quantiser_scale_possible);
  fprintf(stderr, "stats_intra_quantiser_scale_nr: %d\n",
	  stats_intra_quantiser_scale_nr);

  fprintf(stderr, "stats_non_intra_quantiser_scale_possible: %d\n",
	  stats_non_intra_quantiser_scale_possible);
  fprintf(stderr, "stats_non_intra_quantiser_scale_nr: %d\n",
	  stats_non_intra_quantiser_scale_nr);

  fprintf(stderr, "stats_block_non_intra_nr: %d\n",
	  stats_block_non_intra_nr);
  fprintf(stderr, "stats_f_non_intra_compute_first_nr: %d\n",
	  stats_f_non_intra_compute_first_nr);
  fprintf(stderr, "stats_f_non_intra_compute_subseq_nr: %d\n",
	  stats_f_non_intra_compute_subseq_nr);
  fprintf(stderr, "stats_f_non_intra_first_escaped_run_nr: %d\n",
	  stats_f_non_intra_first_escaped_run_nr);
  fprintf(stderr, "stats_f_non_intra_subseq_escaped_run_nr: %d\n",
	  stats_f_non_intra_subseq_escaped_run_nr);
  

  fprintf(stderr, "stats_block_intra_nr: %d\n",
	  stats_block_intra_nr);
  fprintf(stderr, "stats_f_intra_compute_first_nr: %d\n",
	  stats_f_intra_compute_first_nr);
  fprintf(stderr, "stats_f_intra_compute_subseq_nr: %d\n",
	  stats_f_intra_compute_subseq_nr);


  


#endif
  exit(0);
}
