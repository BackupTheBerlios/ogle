#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>

#include "video_stream.h"

#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>


#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
static int shmem_flag = 1;
static XVisualInfo vinfo;
static XShmSegmentInfo shm_info;

unsigned char *ImageData;
XImage *myximage;
Display *mydisplay;
Window mywindow;

GC mygc;

int bpp, mode;
XWindowAttributes attribs;

#define READ_SIZE 2048
#define ALLOC_SIZE 2048
#define BUF_SIZE_MAX 2048*8

#define DEBUG

unsigned int debug = 0;
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
int get_vlc(vlc_table_t *table, char *func);
void block(unsigned int i);
void reset_PMV();
void motion_vectors(unsigned int s);
void motion_vector(int r, int s);

void reset_to_default_quantiser_matrix();
int sign(int16_t num);


void display_init();
void Display_Image(XImage *myximage, unsigned char *ImageData);
void motion_comp();






// Not implemented
void extension_data(unsigned int i);




uint32_t buf[BUF_SIZE_MAX];
unsigned int buf_len = 0;
unsigned int buf_start = 0;
unsigned int buf_fill = 0;
unsigned int bytes_read = 0;
unsigned int bit_start = 0;





FILE *infile;


unsigned int backed = 0;
unsigned int buf_pos = 0;
unsigned int bits_left = 32;
uint32_t cur_word = 0;

uint8_t bytealign = 1;





/* image data */
typedef struct {
  mlib_u8 *y; //[480][720];  //y-component image
  mlib_u8 *u; //[480/2][720/2]; //u-component
  mlib_u8 *v; //[480/2][720/2]; //v-component
} yuv_image_t;


yuv_image_t r1_img;
yuv_image_t r2_img;
yuv_image_t dst_img;

yuv_image_t *ref_image1;
yuv_image_t *ref_image2;
yuv_image_t *dst_image;
yuv_image_t *b_image;






/*macroblock*/
mlib_u8 abgr[16*16*4];
mlib_u8 y_blocks[64*4];
mlib_u8 u_block[64];
mlib_u8 v_block[64];





void fprintbits(FILE *fp, unsigned int bits, uint32_t value)
{
  int n;
  for(n = 0; n < bits; n++) {
    fprintf(fp, "%u", (value>>(bits-n-1)) & 0x1);
  }
}
  

int bytealigned(void)
{
  return bytealign;
}


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
	exit(0);
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

#ifdef DEBUG
uint32_t getbits(unsigned int nr, char *func)
#else
uint32_t getbits(unsigned int nr)
#endif
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
    bytealign = !(bits_left%8);
#ifdef DEBUG
    if(debug > 2) {
      fprintf(stderr, "%s getbits(%u): %x, ", func, nr, result);
      fprintbits(stderr, nr, result);
      fprintf(stderr, "\n");
    }
#endif
    return result;
  } else {
    rem = nr-bits_left;
    result = ((cur_word << (32-bits_left)) >> (32-bits_left)) << rem;

    bits_left = 32;
    next_word();
    result |= ((cur_word << (32-bits_left)) >> (32-rem));
    bits_left -=rem;
    if(bits_left == 0) {
      next_word();
      bits_left = 32;
    }
    bytealign = !(bits_left%8);
#ifdef DEBUG
    if(debug > 2) {
      fprintf(stderr, "%s getbits(%u): %x ", func, nr, result);
      fprintbits(stderr, nr, result);
      fprintf(stderr, "\n");
    }
#endif
    return result;
  }
}

uint32_t nextbits(unsigned int nr)
{
  uint32_t result;
  uint32_t rem;
  unsigned int t_bits_left = bits_left;
  //fprintf(stderr, "left %d\n", t_bits_left);
  if(nr <= t_bits_left) {
    result = (cur_word << (32-t_bits_left)) >> (32-nr);
    // fprintf(stderr, "c %08x\n",cur_word);
    //fprintf(stderr, "x %08x\n",result);
  } else {
    rem = nr-t_bits_left;
    result = ((cur_word << (32-t_bits_left)) >> (32-t_bits_left)) << rem;
    //fprintf(stderr, "y %08x\n", result);
    t_bits_left = 32;
    next_word();
    result |= ((cur_word << (32-t_bits_left)) >> (32-rem));
    back_word();
    //fprintf(stderr, "z %08x\n", result);
  }
  return result;
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

  ref_image1 = &r1_img;
  ref_image2 = &r2_img;
  dst_image = &dst_img;

  display_init();

  next_word();
  
  while(!feof(infile)) {
    fprintf(stderr, "Looking for new Video Sequence\n");
    video_sequence();
  }
  return 0;
}

#ifdef DEBUG

#define GETBITS(a,b) getbits(a,b)

#else

#define GETBITS(a,b) getbits(a)

#endif


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
  fprintf(stderr, "Resyncing\n");
  GETBITS(8, "resync");

}

void video_sequence(void) {
  next_start_code();
  if(nextbits(32) == MPEG2_VS_SEQUENCE_HEADER_CODE) {
    fprintf(stderr, "Found Sequence Header\n");

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

	  //fprintf(stderr, "end of pic nextbits: %08x\n", nextbits(32));
       	} while((nextbits(32) == MPEG2_VS_PICTURE_START_CODE) ||
		(nextbits(32) == MPEG2_VS_GROUP_START_CODE));
	if(nextbits(32) != MPEG2_VS_SEQUENCE_END_CODE) {

	  if(nextbits(32) != MPEG2_VS_SEQUENCE_HEADER_CODE) {
	    fprintf(stderr, "*** not a sequence header\n");

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
      fprintf(stderr, "Found Sequence End\n");

    } else {
      fprintf(stderr, "*** Didn't find Sequence End\n");

    }
  } else {
    resync();
  }

}
  

void marker_bit(void)
{
  if(!GETBITS(1, "markerbit")) {
    fprintf(stderr, "*** incorrect marker_bit\n");
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
  uint8_t intra_inverse_quantiser_matrix[8][8];
  uint8_t non_intra_inverse_quantiser_matrix[8][8];
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

void sequence_header(void)
{
  uint32_t sequence_header_code;
  
  fprintf(stderr, "sequence_header\n");
  
  sequence_header_code = GETBITS(32, "sequence header code");
  
  /* When a sequence_header_code is decoded all matrices shall be reset
     to their default values */
  
  reset_to_default_quantiser_matrix();
  
  seq.header.horizontal_size_value = GETBITS(12, "horizontal_size_value");
  seq.header.vertical_size_value = GETBITS(12, "vertical_size_value");
  seq.header.aspect_ratio_information = GETBITS(4, "aspect_ratio_information");
  fprintf(stderr, "vertical: %u\n", seq.header.horizontal_size_value);
  fprintf(stderr, "horisontal: %u\n", seq.header.vertical_size_value);
  seq.header.frame_rate_code = GETBITS(4, "frame_rate_code");
  seq.header.bit_rate_value = GETBITS(18, "bit_rate_value");  
  marker_bit();
  seq.header.vbv_buffer_size_value = GETBITS(10, "vbv_buffer_size_value");
  seq.header.constrained_parameters_flag = GETBITS(1, "constrained_parameters_flag");
  seq.header.load_intra_quantiser_matrix = GETBITS(1, "load_intra_quantiser_matrix");
  if(seq.header.load_intra_quantiser_matrix) {
    int n;
    for(n = 0; n < 64; n++) {
      seq.header.intra_quantiser_matrix[n] = GETBITS(8, "intra_quantiser_matrix[n]");
    }
    
    /* inverse scan for matrix download */
    {
      int v, u;
      for (v=0; v<8; v++) {
	for (u=0; u<8; u++) {
	  seq.header.intra_inverse_quantiser_matrix[v][u] =
	    seq.header.intra_quantiser_matrix[scan[0][v][u]];
	}
      }
    }
    
  }
  seq.header.load_non_intra_quantiser_matrix = GETBITS(1, "load_non_intra_quantiser_matrix");
  if(seq.header.load_non_intra_quantiser_matrix) {
    int n;
    for(n = 0; n < 64; n++) {
      seq.header.non_intra_quantiser_matrix[n] = GETBITS(8, "non_intra_quantiser_matrix[n]");
    }
   
    /* inverse scan for matrix download */
    {
      int v, u;
      for (v=0; v<8; v++) {
	for (u=0; u<8; u++) {
	  seq.header.non_intra_inverse_quantiser_matrix[v][u] =
	    seq.header.non_intra_quantiser_matrix[scan[0][v][u]];
	}
      }
    }
    
  }


  seq.horizontal_size = seq.header.horizontal_size_value;
  seq.vertical_size = seq.header.vertical_size_value;
  

}


void sequence_extension(void) {

  uint32_t extension_start_code;
  
  
  extension_start_code = GETBITS(32, "extension_start_code");
  //fprintf(stderr, "sequence_extension: %08x\n", extension_start_code);

  seq.ext.extension_start_code_identifier = GETBITS(4, "extension_start_code_identifier");
  seq.ext.profile_and_level_indication = GETBITS(8, "profile_and_level_indication");

#ifdef DEBUG
  if(debug > 0) {
    fprintf(stderr, "profile_and_level_indication: ");
    if(seq.ext.profile_and_level_indication & 0x80) {
      fprintf(stderr, "(reserved)\n");
    } else {
      fprintf(stderr, "Profile: ");
      switch((seq.ext.profile_and_level_indication & 0x70)>>4) {
      case 0x5:
	fprintf(stderr, "Simple");
	break;
      case 0x4:
	fprintf(stderr, "Main");
	break;
      case 0x3:
	fprintf(stderr, "SNR Scalable");
      break;
      case 0x2:
	fprintf(stderr, "Spatially Scalable");
	break;
      case 0x1:
	fprintf(stderr, "High");
	break;
      default:
	fprintf(stderr, "(reserved)");
	break;
      }

      fprintf(stderr, ", Level: ");
      switch(seq.ext.profile_and_level_indication & 0x0f) {
      case 0xA:
	fprintf(stderr, "Low");
	break;
      case 0x8:
	fprintf(stderr, "Main");
	break;
      case 0x6:
	fprintf(stderr, "High 1440");
      break;
      case 0x4:
	fprintf(stderr, "High");
	break;
      default:
	fprintf(stderr, "(reserved)");
	break;
      }
      fprintf(stderr, "\n");
    }
  }
#endif
      
  seq.ext.progressive_sequence = GETBITS(1, "progressive_sequence");
#ifdef DEBUG
  if(debug > 0) {
    fprintf(stderr, "progressive seq: %01x\n", seq.ext.progressive_sequence);
  }
#endif
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
      fprintf(stderr, "allocateing buffers\n");
      ref_image1->y = memalign(8, num_pels);
      ref_image1->u = memalign(8, num_pels/4);
      ref_image1->v = memalign(8, num_pels/4);
      
      ref_image2->y = memalign(8, num_pels);
      ref_image2->u = memalign(8, num_pels/4);
      ref_image2->v = memalign(8, num_pels/4);
      
      dst_image->y = memalign(8, num_pels);
      dst_image->u = memalign(8, num_pels/4);
      dst_image->v = memalign(8, num_pels/4);

      if (shmem_flag) {
        /* Create shared memory image */
        myximage = XShmCreateImage(mydisplay, vinfo.visual, bpp,
                                   ZPixmap, NULL, &shm_info,
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
        if (shm_info.shmid < 0) {
          XDestroyImage(myximage);
          fprintf(stderr, "Shared memory: Couldn't get segment\n");
          goto shmemerror;
        }
        
        /* Attach shared memory segment */
        shm_info.shmaddr = (char *) shmat(shm_info.shmid, 0, 0);
        if (shm_info.shmaddr == ((char *) -1)) {
          XDestroyImage(myximage);
          fprintf(stderr, "Shared memory: Couldn't attach segment\n");
          goto shmemerror;
        }

        myximage->data = shm_info.shmaddr;
        shm_info.readOnly = False;
        XShmAttach(mydisplay, &shm_info);
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
    }
  }
}

void extension_and_user_data(unsigned int i) {
  
#ifdef DEBUG

  if(debug > 1) {
    fprintf(stderr, "extension_and_user_data(%u)\n", i);
  }
#endif

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


void picture_coding_extension(void)
{
  uint32_t extension_start_code;

#ifdef DEBUG

  if(debug > 1) {
    fprintf(stderr, "picture_coding_extension\n");
  }
#endif

  extension_start_code = GETBITS(32, "extension_start_code");
  pic.coding_ext.extension_start_code_identifier = GETBITS(4, "extension_start_code_identifier");
  pic.coding_ext.f_code[0][0] = GETBITS(4, "f_code[0][0]");
  pic.coding_ext.f_code[0][1] = GETBITS(4, "f_code[0][1]");
  pic.coding_ext.f_code[1][0] = GETBITS(4, "f_code[1][0]");
  pic.coding_ext.f_code[1][1] = GETBITS(4, "f_code[1][1]");
  pic.coding_ext.intra_dc_precision = GETBITS(2, "intra_dc_precision");
  pic.coding_ext.picture_structure = GETBITS(2, "pciture_structure");

  if(debug > 0) {
    fprintf(stderr, "picture_structure: ");
    switch(pic.coding_ext.picture_structure) {
    case 0x0:
      fprintf(stderr, "reserved");
      break;
    case 0x1:
      fprintf(stderr, "Top Field");
      break;
    case 0x2:
      fprintf(stderr, "Bottom Field");
      break;
    case 0x3:
      fprintf(stderr, "Frame Picture");
      break;
    }
    fprintf(stderr, "\n");
  }

  pic.coding_ext.top_field_first = GETBITS(1, "top_field_first");
  pic.coding_ext.frame_pred_frame_dct = GETBITS(1, "frame_pred_frame_dct");
  pic.coding_ext.concealment_motion_vectors = GETBITS(1, "concealment_motion_vectors");
  pic.coding_ext.q_scale_type = GETBITS(1, "q_scale_type");
  pic.coding_ext.intra_vlc_format = GETBITS(1, "intra_vlc_format");
  pic.coding_ext.alternate_scan = GETBITS(1, "alternate_scan");
  pic.coding_ext.repeat_first_field = GETBITS(1, "repeat_first_field");
  pic.coding_ext.chroma_420_type = GETBITS(1, "chroma_420_type");
  pic.coding_ext.progressive_frame = GETBITS(1, "progressive_frame");
  if(debug > 0) {
  fprintf(stderr, "progressive_frame: %01x\n", pic.coding_ext.progressive_frame);
  }
  pic.coding_ext.composite_display_flag = GETBITS(1, "composite_display_flag");
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

void user_data(void)
{
  uint32_t user_data_start_code;
  uint8_t user_data;
  
#ifdef DEBUG

  if(debug > 1) {
    fprintf(stderr, "user_data\n");
  }
#endif



  user_data_start_code = GETBITS(32, "user_date_start_code");
  while(nextbits(24) != 0x000001) {
    user_data = GETBITS(8, "user_data");
  }
  next_start_code();
  
}

void group_of_pictures_header(void)
{
  uint32_t group_start_code;
  uint32_t time_code;
  uint8_t closed_gop;
  uint8_t broken_link;
  
#ifdef DEBUG

  if(debug > 1) {
    fprintf(stderr, "group_of_pictures_header\n");
  }

#endif

  group_start_code = GETBITS(32, "group_start_code");
  time_code = GETBITS(25, "time_code");

#ifdef DEBUG
  if(debug > 0) {
    fprintf(stderr, "time_code: %02d:%02d.%02d'%02d\n",
	    (time_code & 0x00f80000)>>19,
	    (time_code & 0x0007e000)>>13,
	    (time_code & 0x00000fc0)>>6,
	    (time_code & 0x0000003f));
  }
#endif
  closed_gop = GETBITS(1, "closed_gop");
  broken_link = GETBITS(1, "broken_link");
  next_start_code();
  
}


void picture_header(void)
{
  uint32_t picture_start_code;

#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "picture_header\n");
  }
#endif  

  picture_start_code = GETBITS(32, "picture_start_code");
  pic.header.temporal_reference = GETBITS(10, "temporal_reference");
  pic.header.picture_coding_type = GETBITS(3, "picture_coding_type");
  if(debug > 0) {
    fprintf(stderr, "picture coding type: %01x, ", pic.header.picture_coding_type);
  }
  switch(pic.header.picture_coding_type) {
  case 0x00:
    fprintf(stderr, "forbidden\n");
    break;
  case 0x01:
    fprintf(stderr, "intra-coded (I)\n");
    break;
  case 0x02:
    fprintf(stderr, "predictive-coded (P)\n");
    break;
  case 0x03:
    fprintf(stderr, "bidirectionally-predictive-coded (B)\n");
    break;
  case 0x04:
    fprintf(stderr, "shall not be used (dc intra-coded (D) in ISO/IEC11172-2)\n");
    break;
  default:
    fprintf(stderr, "reserved\n");
    break;
  }
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

void picture_data(void)
{

    yuv_image_t *tmp_img;
    yuv_image_t *display_image;
  
#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "picture_data\n");
  }
#endif
  
  
  
  
  switch(pic.header.picture_coding_type) {
  case 0x1:
  case 0x2:
    //I,P picture
    b_image = dst_image;
    tmp_img = ref_image1;
    ref_image1 = ref_image2;
    dst_image = tmp_img;
    ref_image2 = tmp_img;
    display_image = ref_image1;
    break;
  case 0x3:
    // B-picture
    dst_image = b_image;
    display_image = b_image;
    break;
  }
  /*
  switch(pic.header.picture_coding_type) {
  case 0x1:
  case 0x2:
    tmp_img = ref_image1;
    ref_image1 = ref_image2;
    ref_image2 = tmp_img;
    dst_image = tmp_img;
    display_image = tmp_img;
    break;
  case 0x3:
    // B-picture
    dst_image = &dst_img;
    //display_image = b_image;
    break;
  }
  */
  if(debug > 0) {
    fprintf(stderr," switching buffers\n");
  }
  //fprintf(stderr,"
//  dst_image = ref_image1;
  //display_image = dst_image;
  
  memset(dst_image->y, 0, seq.horizontal_size*seq.vertical_size);
  memset(dst_image->u, 0, seq.horizontal_size*seq.vertical_size/4);
  memset(dst_image->v, 0, seq.horizontal_size*seq.vertical_size/4);

  do {
    slice();
  } while((nextbits(32) >= MPEG2_VS_SLICE_START_CODE_LOWEST) &&
	  (nextbits(32) <= MPEG2_VS_SLICE_START_CODE_HIGHEST));
  
  //  memset(dst_image->y, 0, seq.horizontal_size*seq.vertical_size);
  //memset(dst_image->u, 0, seq.horizontal_size*seq.vertical_size/4);
  //memset(dst_image->v, 0, seq.horizontal_size*seq.vertical_size/4);

  /* display screen */
  
  switch(pic.header.picture_coding_type) {
  case 0x1:
  case 0x2:
  case 0x3:
    mlib_VideoColorYUV2ABGR420(ImageData,
			       display_image->y,
			       display_image->u,
			       display_image->v,
			       seq.horizontal_size,
			       seq.vertical_size,
			       seq.horizontal_size*4, //TODO
			     seq.horizontal_size,
			       seq.horizontal_size/2);
    
    
    
    Display_Image(myximage, ImageData);
    break;
  default:
    break;
  }
  next_start_code();
}

void extension_data(unsigned int i)
{
  fprintf(stderr, "**ni extension_data\n");
  exit(-1);
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

void slice(void)
{
  uint32_t slice_start_code;
  
#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "slice\n");
  }
#endif
  
  reset_dc_dct_pred();
  reset_PMV();

#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "start of slice\n");
  }
#endif

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

  if(0) {//sequence_scalable_extension_present) {
    if(0) { //scalable_mode == DATA_PARTITIONING) {
      slice_data.priority_breakpoint = GETBITS(7, "priority_breakpoint");
    }
  }
  slice_data.quantiser_scale_code = GETBITS(5, "quantiser_scale_code");
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



  int16_t f[6][8*8];
  
} macroblock_t;

macroblock_t mb;

void macroblock(void)
{
  unsigned int i;
  unsigned int block_count;
  uint16_t inc_add = 0;
  
#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "macroblock()\n");
  }
#endif

  while(nextbits(11) == 0x0008) {
    mb.macroblock_escape = GETBITS(11, "macroblock_escape");
    inc_add+=33;
  }

  mb.macroblock_address_increment = get_vlc(table_b1, "macroblock_address_increment");

  mb.macroblock_address_increment+= inc_add;
  
  seq.macroblock_address =  mb.macroblock_address_increment + seq.previous_macroblock_address;
  
  seq.previous_macroblock_address = seq.macroblock_address;

  seq.mb_column = seq.macroblock_address % seq.mb_width;
  
#ifdef DEBUG
  if(debug > 0) {
    fprintf(stderr, " Macroblock: %d, row: %d, col: %d\n",
	    seq.macroblock_address,
	    seq.mb_row,
	    seq.mb_column);
  }
  
  if(debug > 0) {
    if(mb.macroblock_address_increment > 1) {
      fprintf(stderr, "Skipped %d macroblocks\n",
	      mb.macroblock_address_increment);
    }
  }
#endif
  

    if(pic.header.picture_coding_type == 0x2) {
    /* In a P-picture when a macroblock is skipped */
    if(mb.macroblock_address_increment > 1) {
      reset_PMV();
    }
    }
    
  if(mb.macroblock_address_increment > 1) {
   
    int x,y;
    int old_col = seq.mb_column;
    switch(pic.header.picture_coding_type) {
    case 0x2:


      if(debug > 0) {
	fprintf(stderr,"skipped in P-frame\n");
      }
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
      if(debug > 0) {
	fprintf(stderr,"skipped in B-frame\n");
      }
      for(seq.mb_column = seq.mb_column-mb.macroblock_address_increment+1;
	  seq.mb_column < old_col; seq.mb_column++) {
	motion_comp();
      }
      seq.mb_column = old_col;
      break;
    default:
      fprintf(stderr, "*** skipped blocks in I-frame\n");
      break;
    }
  }


  




  macroblock_modes();
  
    if(mb.modes.macroblock_intra == 0) {
    reset_dc_dct_pred();

#ifdef DEBUG
    if(debug > 1) {
      fprintf(stderr, "non_intra macroblock\n");
    }
#endif

  }

  if(mb.macroblock_address_increment > 1) {
    reset_dc_dct_pred();
#ifdef DEBUG
    if(debug > 1) {
      fprintf(stderr, "skipped block\n");
    }
#endif
  }
    
   


  if(mb.modes.macroblock_quant) {
    slice_data.quantiser_scale_code = GETBITS(5, "quantiser_scale_code");
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
	  if(debug > 0) {
	    fprintf(stderr, "* 1\n");
	  }
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
	  if(debug > 0) {
	    fprintf(stderr, "* 2\n");
	  }
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
	  if(debug > 0) {
	    fprintf(stderr, "* 3\n");
	  }
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
	  if(debug > 0) {
	    fprintf(stderr, "* 4\n");
	  }

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
      if(debug > 0) {
	fprintf(stderr, "* 5\n");
      }
      reset_PMV();
    }
  }
  

  if(pic.header.picture_coding_type == 0x02) {

  /* In a P-picture when a non-intra macroblock is decoded
     in which macroblock_motion_forward is zero */

    if(mb.modes.macroblock_intra == 0) {
      if(mb.modes.macroblock_motion_forward == 0) {
	reset_PMV();
	if(debug > 0) {
	  fprintf(stderr, "* 6\n");
	}

      }
    }

  /* In a P-picture when a macroblock is skipped */
    /*
    if(mb.macroblock_address_increment > 1) {
      reset_PMV();
    }
    */
  }
    
  



  if(mb.modes.macroblock_pattern) {
    coded_block_pattern();
  } else {
   mb.cbp = 0;
  }

  {
    int i;
    for(i = 0; i < 12; i++) {
      if(mb.modes.macroblock_intra != 0) {
	mb.pattern_code[i] = 1;
      } else {
	mb.pattern_code[i] = 0;
      }
    }
    
    if(mb.modes.macroblock_intra == 0) {
      for(i = 0; i < 6; i++) {
	if(mb.cbp & (1<<(5-i))) {
#ifdef DEBUG
	  if(debug > 1) {
	    fprintf(stderr, "cbpindex: %d set\n", i);
	  }
#endif
	  mb.pattern_code[i] = 1;
	}
      }
      if(seq.ext.chroma_format == 0x02) {
	for(i = 6; i < 8; i++) {
	  if(mb.coded_block_pattern_1 & (1<<(7-i))) {
	    mb.pattern_code[i] = 1;
	  }
	}
	
      }
      if(seq.ext.chroma_format == 0x03) {
	for(i = 6; i < 12; i++) {
	  if(mb.coded_block_pattern_2 & (1<<(11-i))) {
	    mb.pattern_code[i] = 1;
	  }
	}
      }
    }
    
  }

  if(seq.ext.chroma_format == 0x01) {
    block_count = 6;
  } else if(seq.ext.chroma_format == 0x02) {
    block_count = 8;
  } else if(seq.ext.chroma_format == 0x03) {
    block_count = 12;
  }

  for(i = 0; i < block_count; i++) {  
    block(i);
  }
  

  
  /*********** koll av specialfall ***/
  
  if(pic.header.picture_coding_type == 0x2) {
    /* P-picture */
    if((!mb.modes.macroblock_motion_forward) && (!mb.modes.macroblock_intra)) {
      if(debug > 0) {
	fprintf(stderr, "macroblock\n");
	fprintf(stderr, "prediction mode shall be Frame-based\n");
	fprintf(stderr, "The frame motion vector shall be zero (0;0)\n");
	fprintf(stderr, "The motion vector predictors shall be reset to zero\n");
	fprintf(stderr, "motion_type: %02x\n", mb.modes.frame_motion_type);
      }
      mb.modes.macroblock_motion_forward = 1;
      mb.vector[0][0][0] = 0;
      mb.vector[0][0][1] = 0;
       
      

    }

    /* never happens */
    if(mb.macroblock_address_increment > 1) {
      if(debug > 0) {
	fprintf(stderr, "prediction mode shall be Frame-based\n");
	fprintf(stderr, "motion vector predictors shall be reset to zero\n");
	fprintf(stderr, "motion vector shall be zero\n");
      }
      //**** TODO
      //mb.vector[0][0][0] = 0;
      //mb.vector[0][0][1] = 0;

    }
    
    
  }
  
  motion_comp();

  

}


void macroblock_modes(void)
{
  
#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "macroblock_modes\n");
  }
#endif

  if(pic.header.picture_coding_type == 0x01) {
    /* I-picture */
#ifdef DEBUG
    if(debug > 1) {
      fprintf(stderr, "table_b2\n");
    }
#endif
    mb.modes.macroblock_type = get_vlc(table_b2, "macroblock_type (b2)");

  } else if(pic.header.picture_coding_type == 0x02) {
    /* P-picture */
#ifdef DEBUG
    if(debug > 1) {
      fprintf(stderr, "table_b3\n");
    }
#endif
    mb.modes.macroblock_type = get_vlc(table_b3, "macroblock_type (b3)");

  } else if(pic.header.picture_coding_type == 0x03) {
    /* B-picture */
#ifdef DEBUG
    if(debug > 1) {
      fprintf(stderr, "table_b4\n");
    }
#endif

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
  


  if((mb.modes.spatial_temporal_weight_code_flag == 1) &&
     ( 1 /*spatial_temporal_weight_code_table_index != 0*/)) {
    mb.modes.spatial_temporal_weight_code = GETBITS(2, "spatial_temporal_weight_code");
  }

  if(mb.modes.macroblock_motion_forward ||
     mb.modes.macroblock_motion_backward) {
    if(pic.coding_ext.picture_structure == 0x03 /*frame*/) {
      if(pic.coding_ext.frame_pred_frame_dct == 0) {
	mb.modes.frame_motion_type = GETBITS(2, "frame_motion_type");
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
  }

}


void coded_block_pattern(void)
{
  //  uint16_t coded_block_pattern_420;
  uint8_t cbp = 0;
  
#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "coded_block_pattern\n");
  }
#endif

  cbp = get_vlc(table_b9, "cbp (b9)");
  

  if((cbp == 0) && (seq.ext.chroma_format == 0x1)) {
    fprintf(stderr, "** shall not be used with 4:2:0 chrominance\n");
    exit(-1);
  }
  mb.cbp = cbp;
#ifdef DEBUG
  if(debug > 2) {
    fprintf(stderr, "cpb = %u\n", mb.cbp);
  }
#endif

  if(seq.ext.chroma_format == 0x02) {
    mb.coded_block_pattern_1 = GETBITS(2, "coded_block_pattern_1");
  }
 
  if(seq.ext.chroma_format == 0x03) {
    mb.coded_block_pattern_2 = GETBITS(6, "coded_block_pattern_2");
  }
  
}

int get_vlc(vlc_table_t *table, char *func) {
  int pos=0;
  int numberofbits=1;
  int vlc;
  while(table[pos].numberofbits != VLC_FAIL) {
    vlc=nextbits(numberofbits);
    while(table[pos].numberofbits == numberofbits) {
      if(table[pos].vlc == vlc) {
#ifdef DEBUG
	if(debug > 2) {
	  fprintf(stderr, "%s ", func);
	}
#endif
	GETBITS(numberofbits, "vlc");
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


void block(unsigned int i)
{
  uint16_t dct_dc_size_luminance;
  uint16_t dct_dc_differential;
  uint16_t dct_dc_size_chrominance;
  int n = 0;
  int16_t QFS[64];
  int16_t QF[8][8];
  int16_t F_bis[8][8];
  int16_t F_prim[8][8];
  int16_t F[8][8];


  int16_t dct_diff;
  int16_t half_range;
  uint8_t cc;
  runlevel_t runlevel;
  int eob_not_read;
  int m;

  
  
  if(i < 4) {
    cc = 0;
  } else {
    cc = (i%2)+1;
  }
  if(mb.pattern_code[i]) {
#ifdef DEBUG
    if(debug > 1) {
      fprintf(stderr, "pattern_code(%d) set\n", i);
    }
#endif
    if(mb.modes.macroblock_intra) {
      //      fprintf(stderr, "macroblock_intra\n");
      if(i < 4) {
	//fprintf(stderr, "luma\n");
      
	dct_dc_size_luminance = get_vlc(table_b12, "dct_dc_size_luminance (b12)");
#ifdef DEBUG
	if(debug > 1) {
	  fprintf(stderr, "luma_size: %d\n", dct_dc_size_luminance);
	}
#endif
	if(dct_dc_size_luminance != 0) {
	  //fprintf(stderr, "dct_dc_diff get\n");
	  dct_dc_differential = GETBITS(dct_dc_size_luminance, "dct_dc_differential (luma)");
	  //fprintf(stderr, "dc_diff: %d\n", dct_dc_differential); 
#ifdef DEBUG
	  if(debug > 1) {
	    fprintf(stderr, "luma_val: %d, ", dct_dc_differential);
	  }
#endif
	}
	
	if(dct_dc_size_luminance == 0) {
	  //fprintf(stderr, "no dct_dc_diff\n");
	  dct_diff = 0;
	} else {
	  half_range = 1<<(dct_dc_size_luminance-1);
	  //fprintf(stderr, "halfrange: %d\n", half_range);
	  if(dct_dc_differential >= half_range) {
	    dct_diff = (int16_t) dct_dc_differential;
	  } else {
	    dct_diff = (int16_t)((dct_dc_differential+1)-(2*half_range));
	  }
#ifdef DEBUG
	  if(debug > 1) {
	    fprintf(stderr, "%d\n", dct_diff);
	  }
#endif
	}
	QFS[n] = mb.dc_dct_pred[cc]+dct_diff;
	mb.dc_dct_pred[cc] = QFS[n];
	
      } else {
	//fprintf(stderr, "chroma\n");

	dct_dc_size_chrominance = get_vlc(table_b13, "dct_dc_size_chrominance (b13)");
#ifdef DEBUG
	if(debug > 1) {
	  fprintf(stderr, "chroma_size: %d\n", dct_dc_size_chrominance);
	}
#endif
	if(dct_dc_size_chrominance != 0) {
	  dct_dc_differential = GETBITS(dct_dc_size_chrominance, "dct_dc_differential (chroma)");
#ifdef DEBUG
	  if(debug > 1) {
	    fprintf(stderr, "chroma_val: %d, ", dct_dc_differential);
	  }
#endif
	}
	
	if(dct_dc_size_chrominance == 0) {
	  dct_diff = 0;
	} else {
	  half_range = 1<<(dct_dc_size_chrominance-1);
	  if(dct_dc_differential >= half_range) {
	    dct_diff = (int16_t) dct_dc_differential; 
	  } else {
	    dct_diff = (int16_t)((dct_dc_differential+1)-(2*half_range));
	  }
#ifdef DEBUG
	  if(debug > 1) {
	    fprintf(stderr, "%d\n", dct_diff);
	  }
#endif
	}
	QFS[n] = mb.dc_dct_pred[cc]+dct_diff;
	mb.dc_dct_pred[cc] = QFS[n];
      }
      
      if((QFS[0] < 0) || (QFS[0] > ((1<<(8+pic.coding_ext.intra_dc_precision))-1))) {
	fprintf(stderr, "*** QFS[0] (%d) Out of Range\n", QFS[0]);
	exit(-1);
      }
      n++;
    } else {
      //fprintf(stderr, "First dct_dc\n");
      //First DCT coefficient
      get_dct(&runlevel, DCT_DC_FIRST, mb.modes.macroblock_intra, pic.coding_ext.intra_vlc_format, "dct_dc_first");
#ifdef DEBUG
      if(debug > 1) {
	if(runlevel.run != VLC_END_OF_BLOCK) {
	  if(debug > 1) {
	    fprintf(stderr, "coeff run: %d, level: %d\n",
		    runlevel.run, runlevel.level);
	  }
	}
      }
#endif
      for(m = 0; m < runlevel.run; m++) {
	QFS[n] = 0;
	n++;
      }
      QFS[n] =  runlevel.level;
      n++;
    }


    eob_not_read = 1;
    while(eob_not_read) {
      //fprintf(stderr, "Subsequent dct_dc\n");
      //Subsequent DCT coefficients
      get_dct(&runlevel, DCT_DC_SUBSEQUENT, mb.modes.macroblock_intra, pic.coding_ext.intra_vlc_format, "dct_dc_subsequent");
#ifdef DEBUG
      if(debug > 1) {
	if(runlevel.run != VLC_END_OF_BLOCK) {
	  if(debug > 1) {
	    fprintf(stderr, "coeff run: %d, level: %d\n",
		    runlevel.run, runlevel.level);
	  }
	}
      }
#endif
      if(runlevel.run == VLC_END_OF_BLOCK) {
	eob_not_read = 0;
	while(n<64) {
	  QFS[n] = 0;
	  n++;
	}
      } else {
	for(m = 0; m < runlevel.run; m++) {
	  QFS[n] = 0;
	  n++;
	}
	QFS[n] =  runlevel.level;
	n++;      
      }
      if(n > 64) {
	fprintf(stderr, "********* more than 64\n");
	exit(-1);
      }
    }
#ifdef DEBUG
    if(debug > 1) {
      fprintf(stderr, "nr of coeffs: %d\n", n);
    }
#endif
   
    /* inverse scan */
    {
      int v,u;
      
      for (v=0; v<8; v++) {
	for (u=0; u<8; u++) {
	  QF[v][u] = QFS[scan[pic.coding_ext.alternate_scan][v][u]];
	}
      }
    }


    /* inverse quantisation  (currently only supports 4:2:0)*/
    {
      int v, u;
      int sum;
      int quantiser_scale;
      int intra_dc_mult;
      
      quantiser_scale =
	q_scale[slice_data.quantiser_scale_code][pic.coding_ext.q_scale_type];
      
      for(v = 0; v < 8; v++) {
	for(u=0; u<8; u++) {
	  if((u==0) && (v==0) && (mb.modes.macroblock_intra)) {
	    /* intra dc coefficient */
	    
	    switch(pic.coding_ext.intra_dc_precision) {
	    case 0x0:
	      intra_dc_mult = 8;
	      break;
	    case 0x1:
	      intra_dc_mult = 4;
	      break;
	    case 0x2:
	      intra_dc_mult = 2;
	      break;
	    case 0x3:
	      intra_dc_mult = 1;
	      break;
	    }
	    
	    F_bis[0][0] = intra_dc_mult * QF[v][u];
	  } else {
	    /* other coefficients */
	    
	    if(mb.modes.macroblock_intra) {
	      F_bis[v][u] = 
		(QF[v][u]*
		 seq.header.intra_inverse_quantiser_matrix[v][u]*
		 quantiser_scale*2)/32;
	    } else {
	      F_bis[v][u] =
		(((QF[v][u]*2)+sign(QF[v][u])) *
		 seq.header.non_intra_inverse_quantiser_matrix[v][u] *
		 quantiser_scale)/32;
	    }
	  }
	}
      }
      
      sum = 0;
      for(v=0; v<8; v++) {
	for(u=0; u<8; u++) {
	  if(F_bis[v][u] > 2047) {
	    F_prim[v][u] = 2047;
	  } else {
	    if(F_bis[v][u] < -2048) {
	      F_prim[v][u] = -2048;
	    } else {
	      F_prim[v][u] = F_bis[v][u];
	    }

	  }
	  sum = sum+F_prim[v][u];
	  F[v][u] = F_prim[v][u];
	}
      }

      if((sum & 1) == 0) {
	if((F[7][7]&1) != 0) {
	  F[7][7] = F_prim[7][7]-1;
	} else {
	  F[7][7] = F_prim[7][7]+1;
	}
      }
      
    }
    

    
    
    mlib_VideoIDCT_IEEE_S16_S16(mb.f[i], (mlib_s16 *)F);      
    
  } else {
    // pattern[i] == 0
    
    memset(mb.f[i], 0, 64*sizeof(int16_t));
    
  }
}


void motion_vectors(unsigned int s)
{
  
  int8_t motion_vector_count;
  int8_t motion_vertical_field_select[2][2];

#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "motion_vectors(%u)\n", s);
  }
#endif

  if(pic.coding_ext.picture_structure == 0x03 /*frame*/) {
    if(pic.coding_ext.frame_pred_frame_dct == 0) {
      
      switch(mb.modes.frame_motion_type) {
      case 0x0:
	fprintf(stderr, "*** reserved prediction type\n");
	exit(-1);
	break;
      case 0x1:
	mb.prediction_type = PRED_TYPE_FIELD_BASED;
	motion_vector_count = 2;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 0;
	break;
      case 0x2:
	/* spatial_temporal_weight_class always 0 for now */
	mb.prediction_type = PRED_TYPE_FRAME_BASED;
	motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FRAME;
	mb.dmv = 0;
	break;
      case 0x3:
	mb.prediction_type = PRED_TYPE_DUAL_PRIME;
	motion_vector_count = 1;
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
	motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FRAME;
	mb.dmv = 0;
    }
      
    if(pic.coding_ext.frame_pred_frame_dct == 1) {
      mb.prediction_type = PRED_TYPE_FRAME_BASED;
      motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FRAME;
      mb.dmv = 0;
    }
    
  } else {
    switch(mb.modes.field_motion_type) {
    case 0x0:
	fprintf(stderr, "*** reserved field prediction type\n");
	exit(-1);
	break;
      case 0x1:
	mb.prediction_type = PRED_TYPE_FIELD_BASED;
	motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 0;
	break;
      case 0x2:
	mb.prediction_type = PRED_TYPE_16x8_MC;
	motion_vector_count = 2;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 0;
	break;
      case 0x3:
	mb.prediction_type = PRED_TYPE_DUAL_PRIME;
	motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 1;
	break;
      default:
	fprintf(stderr, "*** impossible prediction type\n");
	exit(-1);
	break;
      }
  }


  if(motion_vector_count == 1) {
    if((mb.mv_format == MV_FORMAT_FIELD) && (mb.dmv != 1)) {
      motion_vertical_field_select[0][s] = GETBITS(1, "motion_vertical_field_select[0][s]");
    }
    motion_vector(0, s);
  } else {
    motion_vertical_field_select[0][s] = GETBITS(1, "motion_vertical_field_select[0][s]");
    motion_vector(0, s);
    motion_vertical_field_select[1][s] = GETBITS(1, "motion_vertical_field_select[1][s]");
    motion_vector(1, s);
  }
}
 

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
  
#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "motion_vector(%d, %d)\n", r, s);
  }
#endif

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
      prediction = pic.PMV[r][s][t] / 2;
    }
    
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

void get_dct(runlevel_t *runlevel, int first_subseq, uint8_t intra_block,
	     uint8_t intra_vlc_format, char *func) 
{
  int pos=0;
  int numberofbits=1;
  int vlc;
  int sign=0;
  vlc_rl_table *table;
  
  
  if((intra_vlc_format == 1) && (intra_block == 1)) {
    table = table_b15;
  } else {    
    table = table_b14;
    
    if(first_subseq == DCT_DC_FIRST) {
      table[0].numberofbits = 1;
      table[0].vlc = 0x01;
      table[0].run = 0;
      table[0].level = 1;  
      table[1].numberofbits = 1;
      table[1].vlc = 0x01;
      table[1].run = 0;
      table[1].level = 1;  
    } else {
      table[0].numberofbits = 2;
      table[0].vlc = 0x02;
      table[0].run = VLC_END_OF_BLOCK;
      table[0].level = VLC_END_OF_BLOCK;  
      table[1].numberofbits = 2;
      table[1].vlc = 0x03;
      table[1].run = 0;
      table[1].level = 1;  
    } 
  }
  
  while(table[pos].numberofbits != VLC_FAIL) {
    vlc=nextbits(numberofbits);
    while(table[pos].numberofbits == numberofbits) {
      if(table[pos].vlc == vlc) {
#ifdef DEBUG
	if(debug > 2) {
	  fprintf(stderr, "%s ", func);
	}
#endif
	GETBITS(numberofbits, "(get_dct)");
	if(table[pos].run==VLC_ESCAPE) {
#ifdef DEBUG
	  if(debug>1) {
	    fprintf(stderr, "VLC_ESCAPE\n");
	  }
#endif
	  runlevel->run   = GETBITS(6, "(get_dct run)");
	  runlevel->level = GETBITS(12, "(get_dct level)");
	  if(runlevel->level > 2047) {
	    runlevel->level -= 4096;
	  }
	  if(runlevel->level == 0) {
	    fprintf(stderr, "*** get_dct(): level 0 forbidden\n");
	    exit(-1);
	  }
	} else {
	  if(table[pos].run != VLC_END_OF_BLOCK) {
	    sign = GETBITS(1, "(get_dct sign)");
	  }
	  runlevel->run   = table[pos].run;
	  runlevel->level = (sign?-1:1) * table[pos].level;
	}
	return;
      }
      pos++;
    }
    numberofbits++;
  }
  fprintf(stderr, "*** get_dct(): no matching bitstream found.\n");
  exit(1);
  return;
}

void reset_dc_dct_pred(void)
{
#ifdef DEBUG
  if(debug > 1) {
    fprintf(stderr, "Resetting dc_dct_pred\n");
  }
#endif

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

void reset_PMV() {
#ifdef DEBUG
  if(debug > 0) {
    fprintf(stderr, "Resetting PMV\n");
  }
#endif
  
  pic.PMV[0][0][0] = 0;
  pic.PMV[0][0][1] = 0;
  
  pic.PMV[0][1][0] = 0;
  pic.PMV[0][1][1] = 0;

  pic.PMV[1][0][0] = 0;
  pic.PMV[1][0][1] = 0;

  pic.PMV[1][1][0] = 0;
  pic.PMV[1][1][1] = 0;

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

   XSelectInput(mydisplay, mywindow, StructureNotifyMask);

   /* Tell other applications about this window */

   XSetStandardProperties(mydisplay, mywindow, hello, hello, None, NULL, 0, &hint);

   /* Map window. */

   XMapWindow(mydisplay, mywindow);

   /* Wait for map. */
   do {
      XNextEvent(mydisplay, &xev);
   }
   while (xev.type != MapNotify || xev.xmap.event != mywindow);

   XSelectInput(mydisplay, mywindow, NoEventMask);

   XFlush(mydisplay);
   XSync(mydisplay, False);
   
   mygc = XCreateGC(mydisplay, mywindow, 0L, &xgcv);
   
   
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










void Display_Image(XImage *myximage, unsigned char *ImageData)
{

  if (shmem_flag) {
    XShmPutImage(mydisplay, mywindow, mygc, myximage, 
                 0, 0, 0, 0, myximage->width, myximage->height, 1);
  } else {
    XPutImage(mydisplay, mywindow, mygc, myximage, 0, 0,
              0, 0, myximage->width, myximage->height);
  }
  XFlush(mydisplay);
}



void motion_comp()
{
  int width,x,y;
  int pitch;
  int d;
  uint8_t *dst_y,*dst_u,*dst_v;
  int half_flag_y[2];
  int half_flag_uv[2];
  int int_vec_y[2];
  int int_vec_uv[2];

  uint8_t *pred_y;
  uint8_t *pred_u;
  uint8_t *pred_v;



  width = seq.horizontal_size;

  
  //handle interlaced blocks
  if (mb.modes.dct_type) { // skicka med dct_type som argument
    d = 1;
    pitch = width *2;
  } else {
    d = 8;
    pitch = width;
  }

  x = seq.mb_column;
  y = seq.mb_row;
    
  dst_y = &dst_image->y[x * 16 + y * width * 16];
  dst_u = &dst_image->u[x * 8 + y * width/2 * 8];
  dst_v = &dst_image->v[x * 8 + y * width/2 * 8];
    
    
  if(mb.modes.macroblock_motion_forward) {
    if(debug > 0) {
      fprintf(stderr, "forward_motion_comp\n");
    }
    half_flag_y[0] = (mb.vector[0][0][0] & 1);
    half_flag_y[1] = (mb.vector[0][0][1] & 1);
    half_flag_uv[0] = ((mb.vector[0][0][0]/2) & 1);
    half_flag_uv[1] = ((mb.vector[0][0][1]/2) & 1);
    int_vec_y[0] = (mb.vector[0][0][0] >> 1) + (signed int)x * 16;
    int_vec_y[1] = (mb.vector[0][0][1] >> 1) + (signed int)y * 16;
    int_vec_uv[0] = ((mb.vector[0][0][0]/2) >> 1)  + x * 8;
    int_vec_uv[1] = ((mb.vector[0][0][1]/2) >> 1)  + y * 8;
    //int_vec_uv[0] = int_vec_y[0] / 2 ;
    //  int_vec_uv[1] = int_vec_y[1] / 2 ;
    
    



    if(debug > 0) {
      fprintf(stderr, "start: 0, end: %d\n",
	      seq.horizontal_size * seq.vertical_size);
      
      fprintf(stderr, "p_vec x: %d, y: %d\n",
	      (mb.vector[0][0][0] >> 1),
	      (mb.vector[0][0][1] >> 1));
    }
    pred_y  =
      &ref_image1->y[int_vec_y[0] + int_vec_y[1] * width];

    if(debug >0) {
      fprintf(stderr, "ypos: %d\n",
	      int_vec_y[0] + int_vec_y[1] * width);
      
      fprintf(stderr, "start: 0, end: %d\n",
	      seq.horizontal_size * seq.vertical_size/4);
    }
    pred_u =
      &ref_image1->u[int_vec_uv[0] + int_vec_uv[1] * width/2];

    if(debug > 0) {
      fprintf(stderr, "uvpos: %d\n",
	      int_vec_uv[0] + int_vec_uv[1] * width/2);
    }
    pred_v =
      &ref_image1->v[int_vec_uv[0] + int_vec_uv[1] * width/2];

    if(debug > 0) {
      fprintf(stderr, "x: %d, y: %d\n", x, y);
    }
	
    if (half_flag_y[0] && half_flag_y[1]) {
      mlib_VideoInterpXY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
    } else if (half_flag_y[0]) {
      mlib_VideoInterpX_U8_U8_16x16(dst_y,  pred_y,  width,   width);
    } else if (half_flag_y[1]) {
      mlib_VideoInterpY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
    } else {
      mlib_VideoCopyRef_U8_U8_16x16(dst_y,  pred_y,  width);
    }

    if (half_flag_uv[0] && half_flag_uv[1]) {
      mlib_VideoInterpXY_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
      mlib_VideoInterpXY_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
    } else if (half_flag_uv[0]) {
      mlib_VideoInterpX_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
      mlib_VideoInterpX_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
    } else if (half_flag_uv[1]) {
      mlib_VideoInterpY_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
      mlib_VideoInterpY_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
    } else {
      mlib_VideoCopyRef_U8_U8_8x8  (dst_u, pred_u, width/2);
      mlib_VideoCopyRef_U8_U8_8x8  (dst_v, pred_v, width/2);
    }
  }
      
  if(mb.modes.macroblock_motion_backward) {
    if(debug > 0) {
      fprintf(stderr, "backward_motion_comp\n");
    }
    half_flag_y[0]   = (mb.vector[0][1][0] & 1);
    half_flag_y[1]   = (mb.vector[0][1][1] & 1);
    half_flag_uv[0] = ((mb.vector[0][1][0]/2) & 1);
    half_flag_uv[1] = ((mb.vector[0][1][1]/2) & 1);
    int_vec_y[0] = (mb.vector[0][1][0] >> 1) + x * 16;
    int_vec_y[1] = (mb.vector[0][1][1] >> 1) + y * 16;
    int_vec_uv[0] = ((mb.vector[0][1][0]/2) >> 1)  + x * 8;
    int_vec_uv[1] = ((mb.vector[0][1][1]/2) >> 1)  + y * 8;
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
	mlib_VideoInterpAveXY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
      } else if (half_flag_y[0]) {
	mlib_VideoInterpAveX_U8_U8_16x16(dst_y,  pred_y,  width,   width);
      } else if (half_flag_y[1]) {
	mlib_VideoInterpAveY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
      } else {
	mlib_VideoCopyRefAve_U8_U8_16x16(dst_y,  pred_y,  width);
      }
      if (half_flag_uv[0] && half_flag_uv[1]) {
	mlib_VideoInterpAveXY_U8_U8_8x8(dst_u, pred_u, width/2, width/2);
	mlib_VideoInterpAveXY_U8_U8_8x8(dst_v, pred_v, width/2, width/2);
      } else if (half_flag_uv[0]) {
	mlib_VideoInterpAveX_U8_U8_8x8(dst_u, pred_u, width/2, width/2);
	mlib_VideoInterpAveX_U8_U8_8x8(dst_v, pred_v, width/2, width/2);
      } else if (half_flag_uv[1]) {
	mlib_VideoInterpAveY_U8_U8_8x8(dst_u, pred_u, width/2, width/2);
	mlib_VideoInterpAveY_U8_U8_8x8(dst_v, pred_v, width/2, width/2);
      } else {
	mlib_VideoCopyRefAve_U8_U8_8x8(dst_u, pred_u, width/2);
	mlib_VideoCopyRefAve_U8_U8_8x8(dst_v, pred_v, width/2);
      }
    } else {
      if (half_flag_y[0] && half_flag_y[1]) {
	mlib_VideoInterpXY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
      } else if (half_flag_y[0]) {
	mlib_VideoInterpX_U8_U8_16x16(dst_y,  pred_y,  width,   width);
      } else if (half_flag_y[1]) {
	mlib_VideoInterpY_U8_U8_16x16(dst_y,  pred_y,  width,   width);
      } else {
	mlib_VideoCopyRef_U8_U8_16x16(dst_y,  pred_y,  width);
      }
      if (half_flag_uv[0] && half_flag_uv[1]) {
	mlib_VideoInterpXY_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
	mlib_VideoInterpXY_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
      } else if (half_flag_uv[0]) {
	mlib_VideoInterpX_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
	mlib_VideoInterpX_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
      } else if (half_flag_uv[1]) {
	mlib_VideoInterpY_U8_U8_8x8  (dst_u, pred_u, width/2, width/2);
	mlib_VideoInterpY_U8_U8_8x8  (dst_v, pred_v, width/2, width/2);
      } else {
	mlib_VideoCopyRef_U8_U8_8x8  (dst_u, pred_u, width/2);
	mlib_VideoCopyRef_U8_U8_8x8  (dst_v, pred_v, width/2);
      }
    }
  }
  
      
  if(mb.pattern_code[0])
    mlib_VideoAddBlock_U8_S16(dst_y, mb.f[0], pitch);
      
  if(mb.pattern_code[1])
    mlib_VideoAddBlock_U8_S16(dst_y + 8, mb.f[1], pitch);
      
  if(mb.pattern_code[2])
    mlib_VideoAddBlock_U8_S16(dst_y + width * d, mb.f[2], pitch);

  if(mb.pattern_code[3])
    mlib_VideoAddBlock_U8_S16(dst_y + width * d + 8, mb.f[3], pitch);
      
  if(mb.pattern_code[4])
    mlib_VideoAddBlock_U8_S16(dst_u, mb.f[4], width/2);
      
  if(mb.pattern_code[5])
    mlib_VideoAddBlock_U8_S16(dst_v, mb.f[5], width/2);
      
      
}


