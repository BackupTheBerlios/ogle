#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>

#include "video_stream.h"

#ifdef HAVE_MLIB
#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>
#include <mlib_algebra.h>
#else
#ifdef HAVE_MMX
#include "mmx.h"
#endif
#include "c_mlib.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <glib.h>

#include "../include/common.h"

int dctstat[128];
int total_pos = 0;
int total_calls = 0;

extern int show_stat;
unsigned int debug = 0;
int shm_ready = 0;
static int shmem_flag = 1;
int   ring_shmid;
int   ring_c_shmid;
int   ring_buffers = 4;
char *ring_shmaddr;
void setup_shm(int horiz_size, int vert_size);

double time_pic[4] = { 0.0, 0.0, 0.0, 0.0};
double time_max[4] = { 0.0, 0.0, 0.0, 0.0};
double time_min[4] = { 1.0, 1.0, 1.0, 1.0};
double num_pic[4]  = { 0.0, 0.0, 0.0, 0.0};


//#define DEBUG
//#define STATS

int MPEG2 = 0;


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

uint8_t new_scaled = 0;


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


#ifdef TIMESTAT


void timesub(struct timespec *d,
	     struct timespec *s1, struct timespec *s2)
{
  // d = s1-s2
  //  s1 is greater than s2
  
  d->tv_sec = s1->tv_sec - s2->tv_sec;
  d->tv_nsec = s1->tv_nsec - s2->tv_nsec;
  if(d->tv_nsec < 0) {
    d->tv_nsec +=1000000000;
    d->tv_sec -=1;
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
  }
}  

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



struct timespec picture_decode_start_time;
struct timespec picture_decode_end_time;
struct timespec picture_display_start_time;
struct timespec picture_display_end_time;

struct timespec picture_time;
struct timespec picture_decode_time;
struct timespec picture_display_time;

struct timespec picture_time0;
struct timespec picture_time1;
struct timespec picture_time2;

struct timespec picture_decode_time_tot[3];
struct timespec picture_display_time_tot[3];

struct timespec picture_decode_time_min[3];
struct timespec picture_display_time_min[3];

struct timespec picture_decode_time_max[3];
struct timespec picture_display_time_max[3];

uint32_t picture_decode_nr[3];
uint32_t picture_display_nr[3];

struct timespec *picture_decode_start_times;
struct timespec *picture_decode_end_times;
struct timespec *picture_display_start_times;
struct timespec *picture_display_end_times;
uint8_t *picture_type;


uint32_t picture_decode_num;
uint32_t picture_display_num;

typedef struct {
  struct timespec dec_start;
  struct timespec dec_end;
  struct timespec dec_tot;
  struct timespec dec_min;
  struct timespec disp_start;
  struct timespec disp_end;
  struct timespec disp_tot;
  struct timespec disp_min;
  struct timespec pic_tot;
  struct timespec pic_min;
  struct timespec should_sleep;
  struct timespec did_sleep;
  uint8_t type;
} picture_time_t;

picture_time_t *picture_timestat;

#define TIMESTAT_NOF 24*60
void timestat_init()
{
  int n;

  for(n = 0; n < 3; n++) {
    
    picture_decode_time_tot[n].tv_sec = 0;
    picture_decode_time_tot[n].tv_nsec = 0;
    
    picture_display_time_tot[n].tv_sec = 0;
    picture_display_time_tot[n].tv_nsec = 0;

    picture_decode_time_min[n].tv_sec = 99;
    picture_decode_time_min[n].tv_nsec = 0;
    
    picture_display_time_min[n].tv_sec = 99;
    picture_display_time_min[n].tv_nsec = 0;
    
    picture_decode_time_max[n].tv_sec = 0;
    picture_decode_time_max[n].tv_nsec = 0;

    picture_display_time_max[n].tv_sec = 0;
    picture_display_time_max[n].tv_nsec = 0;

    picture_decode_nr[n] = 0;
    picture_display_nr[n] = 0;
  }

  picture_timestat = calloc(TIMESTAT_NOF, sizeof(picture_time_t));

  picture_time.tv_sec = 0;
  picture_time.tv_nsec = 0;

  for(n = 0; n < TIMESTAT_NOF; n++) {
    picture_timestat[n].dec_start = picture_time;
    picture_timestat[n].dec_end = picture_time;
    picture_timestat[n].disp_start = picture_time;
    picture_timestat[n].disp_end = picture_time;
    picture_timestat[n].should_sleep = picture_time;
    picture_timestat[n].did_sleep = picture_time;
    picture_timestat[n].type = 0xff;
  }

  picture_decode_start_times = calloc(TIMESTAT_NOF, sizeof(struct timespec));
  picture_decode_end_times = calloc(TIMESTAT_NOF, sizeof(struct timespec));
  picture_display_start_times = calloc(TIMESTAT_NOF, sizeof(struct timespec));
  picture_display_end_times = calloc(TIMESTAT_NOF, sizeof(struct timespec));
  picture_type = calloc(TIMESTAT_NOF, sizeof(uint8_t));

  picture_decode_num = 0;
  picture_display_num = 0;
  
}

void timestat_print()
{
  int n;
  FILE *stat_file;
  FILE *gnuplot_file;
  picture_time_t tmppic;
  int statnr = 1;
  fprintf(stderr, "\ntimestat\n");


  for(n = 0; n < picture_decode_num; n++) {

    timesub(&(picture_timestat[n].dec_tot),
	    &(picture_timestat[n].dec_end),
	    &(picture_timestat[n].dec_start));
    
    picture_timestat[n].dec_min = picture_timestat[n].dec_tot;


    timesub(&(picture_timestat[n].disp_tot),
	    &(picture_timestat[n].disp_end),
	    &(picture_timestat[n].disp_start));

    picture_timestat[n].disp_min = picture_timestat[n].disp_tot;
    

    timesub(&(picture_timestat[n].pic_tot),
	    &(picture_timestat[n].disp_end),
	    &(picture_timestat[n].dec_start));

    picture_timestat[n].pic_min = picture_timestat[n].pic_tot;
    
  }
  
  if((stat_file = fopen("video_stats", "r")) == NULL) {
    perror("no previous stats");
  } else {
    n = 0;
    fread(&statnr, sizeof(statnr), 1, stat_file);
    statnr++;
    while(fread(&tmppic, sizeof(tmppic), 1, stat_file) && 
	  (n < picture_decode_num)) {
      if(timecompare(&(picture_timestat[n].dec_min), &(tmppic.dec_min)) > 0) {
	picture_timestat[n].dec_min = tmppic.dec_min;
      }
      timeadd(&(picture_timestat[n].dec_tot),
	      &(picture_timestat[n].dec_tot),
	      &(tmppic.dec_tot));
      
      if(timecompare(&(picture_timestat[n].disp_min), &(tmppic.disp_min)) > 0) {
	picture_timestat[n].disp_min = tmppic.disp_min;
      }
      timeadd(&(picture_timestat[n].disp_tot),
	      &(picture_timestat[n].disp_tot),
	      &(tmppic.disp_tot));

      if(timecompare(&(picture_timestat[n].pic_min), &(tmppic.pic_min)) > 0) {
	picture_timestat[n].pic_min = tmppic.pic_min;
      }
      timeadd(&(picture_timestat[n].pic_tot),
	      &(picture_timestat[n].pic_tot),
	      &(tmppic.pic_tot));
      n++;

    }
    picture_decode_num = n;
    fclose(stat_file);
  }
  
  if((stat_file = fopen("video_stats", "w")) == NULL) {
    perror("fopen");
  } else {
    fwrite(&statnr, sizeof(statnr), 1, stat_file);
    if(fwrite(picture_timestat, sizeof(picture_time_t),
	      picture_decode_num, stat_file) != picture_decode_num) {
      perror("fwrite");
    }
    fprintf(stderr, "Wrote stats to file: video_stats\n");
    fclose(stat_file);
  }

  if((gnuplot_file = fopen("stats_dec.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {
      timesub(&picture_time, &(picture_timestat[n].dec_end), &(picture_timestat[n].dec_start));
      timesub(&picture_time0, &(picture_timestat[n].dec_start), &(picture_timestat[0].dec_start));
      fprintf(gnuplot_file, "%d.%09d %d.%09d\n",
	      picture_time0.tv_sec,
	      picture_time0.tv_nsec,
	      picture_time.tv_sec,
	      picture_time.tv_nsec);
    }
   
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_disp.dat\n");
    fclose(gnuplot_file);
  } 

  
  if((gnuplot_file = fopen("stats_disp.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {
      timesub(&picture_time, &(picture_timestat[n].disp_end), &(picture_timestat[n].disp_start));
      timesub(&picture_time0, &(picture_timestat[n].disp_start), &(picture_timestat[0].dec_start));
      fprintf(gnuplot_file, "%d.%09d %d.%09d\n",
	      picture_time0.tv_sec,
	      picture_time0.tv_nsec,
	      picture_time.tv_sec,
	      picture_time.tv_nsec);
    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_disp.dat\n");
    fclose(gnuplot_file);
  } 


  if((gnuplot_file = fopen("stats_pic.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {
      timesub(&picture_time, &(picture_timestat[n].disp_end), &(picture_timestat[n].dec_start));
      timesub(&picture_time0, &(picture_timestat[n].dec_start), &(picture_timestat[0].dec_start));
      fprintf(gnuplot_file, "%d.%09d %d.%09d\n",
	      picture_time0.tv_sec,
	      picture_time0.tv_nsec,
	      picture_time.tv_sec,
	      picture_time.tv_nsec);
    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_pic.dat\n");
    fclose(gnuplot_file);
  } 





  /* ---------------- */


  if((gnuplot_file = fopen("avgstats_dec.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %.09f\n",
	      n,
	      ((double)picture_timestat[n].dec_tot.tv_sec+
	      (double)picture_timestat[n].dec_tot.tv_nsec/
	      1000000000.0)/(double)statnr);

    }
   
    fprintf(stderr, "Wrote gnuplot-stats to file: avgstats_disp.dat\n");
    fclose(gnuplot_file);
  } 

  
  if((gnuplot_file = fopen("avgstats_disp.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %.09f\n",
	      n,
	      ((double)picture_timestat[n].disp_tot.tv_sec+
	      (double)picture_timestat[n].disp_tot.tv_nsec/
	       1000000000.0)/(double)statnr);
    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: avgstats_disp.dat\n");
    fclose(gnuplot_file);
  } 


  if((gnuplot_file = fopen("avgstats_pic.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %.09f\n",
	      n,
	      ((double)picture_timestat[n].pic_tot.tv_sec+
	       (double)picture_timestat[n].pic_tot.tv_nsec/
	       1000000000.0)/(double)statnr);
    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_pic.dat\n");
    fclose(gnuplot_file);
  } 

  fprintf(stderr, "statnr: %d\n", statnr);

  

  /* ------------------------------ */



  if((gnuplot_file = fopen("minstats_dec.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %d.%09d\n",
	      n,
	      picture_timestat[n].dec_min.tv_sec,
	      picture_timestat[n].dec_min.tv_nsec);
      
    }
   
    fprintf(stderr, "Wrote gnuplot-stats to file: avgstats_disp.dat\n");
    fclose(gnuplot_file);
  } 

  
  if((gnuplot_file = fopen("minstats_disp.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %d.%09d\n",
	      n,
	      picture_timestat[n].disp_min.tv_sec,
	      picture_timestat[n].disp_min.tv_nsec);

    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: avgstats_disp.dat\n");
    fclose(gnuplot_file);
  } 


  if((gnuplot_file = fopen("minstats_pic.dat", "w")) == NULL) {
    perror("fopen");
  } else {
    for(n = 0; n < picture_decode_num-1; n++) {

      fprintf(gnuplot_file, "%d %d.%09d\n",
	      n,
	      picture_timestat[n].pic_min.tv_sec,
	      picture_timestat[n].pic_min.tv_nsec);

    }
    
    fprintf(stderr, "Wrote gnuplot-stats to file: stats_pic.dat\n");
    fclose(gnuplot_file);
  } 



  /* ------------------------------- */
  
    
    /*
    for(n = 0; n < picture_decode_num; n++) {
      fprintf(gnuplot_file, "%.9f %d\n",
	      1.0/24.0*n,
	      n);
    }
    */

  
  for(n = 0; n < 3; n++) {
    
    fprintf(stderr, "avg_decode_time: %.4f s,  %.2f fps \n",
	    ((double)picture_decode_time_tot[n].tv_sec +
	     (double)picture_decode_time_tot[n].tv_nsec/1000000000.0)/
	    (double)picture_decode_nr[n],
	    (double)picture_decode_nr[n]/
	    ((double)picture_decode_time_tot[n].tv_sec +
	     (double)picture_decode_time_tot[n].tv_nsec/1000000000.0));

    fprintf(stderr, "avg_display_time: %.4f s\n",
	    ((double)picture_display_time_tot[n].tv_sec +
	     (double)picture_display_time_tot[n].tv_nsec/1000000000.0)/
	    (double)picture_display_nr[n]);



    fprintf(stderr, "display_time_max: %d.%09d\n",
	    picture_display_time_max[0].tv_sec,
	    picture_display_time_max[0].tv_nsec);
    fprintf(stderr, "display_time_max: %d.%09d\n",
	    picture_display_time_max[1].tv_sec,
	    picture_display_time_max[1].tv_nsec);
    fprintf(stderr, "display_time_max: %d.%09d\n",
	    picture_display_time_max[2].tv_sec,
	    picture_display_time_max[2].tv_nsec);

    fprintf(stderr, "display_time_min: %d.%09d\n",
	    picture_display_time_min[0].tv_sec,
	    picture_display_time_min[0].tv_nsec);
    fprintf(stderr, "display_time_min: %d.%09d\n",
	    picture_display_time_min[1].tv_sec,
	    picture_display_time_min[1].tv_nsec);
    fprintf(stderr, "display_time_min: %d.%09d\n",
	    picture_display_time_min[2].tv_sec,
	    picture_display_time_min[2].tv_nsec);

    /*
    picture_decode_time_tot[n].tv_sec = 0;
    picture_decode_time_tot[n].tv_nsec = 0;
    
    picture_display_time_tot[n].tv_sec = 0;
    picture_display_time_tot[n].tv_nsec = 0;

    picture_decode_time_min[n].tv_sec = 99;
    picture_decode_time_min[n].tv_nsec = 0;
    
    picture_display_time_min[n].tv_sec = 99;
    picture_display_time_min[n].tv_nsec = 99;
    
    picture_decode_time_max[n].tv_sec = 0;
    picture_decode_time_max[n].tv_nsec = 0;

    picture_display_time_max[n].tv_sec = 0;
    picture_display_time_max[n].tv_nsec = 0;

    picture_decode_nr[n] = 0;
    picture_display_nr[n] = 0;
    */
  }
  
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


  //int16_t f[6][8*8] __attribute__ ((aligned (8)));

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


macroblock_t mb;
picture_t pic;
sequence_t seq;


yuv_image_t r1_img;
yuv_image_t r2_img;
yuv_image_t b_img;

yuv_image_t *ref_image1;
yuv_image_t *ref_image2;
yuv_image_t *dst_image;
yuv_image_t *b_image;
yuv_image_t *current_image;
yuv_image_t *ring;


/*macroblock*/
uint8_t abgr[16*16*4];
uint8_t y_blocks[64*4];
uint8_t u_block[64];
uint8_t v_block[64];


macroblock_t *cur_mbs;
macroblock_t *ref1_mbs;
macroblock_t *ref2_mbs;




//prototypes
int bytealigned(void);
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

void display_init();
void display_exit();
void frame_done(yuv_image_t *current_image, macroblock_t *cur_mbs,
		yuv_image_t *ref_image1, macroblock_t *ref1_mbs,
		yuv_image_t *ref_image2, macroblock_t *ref2_mbs, 
		uint8_t picture_coding_type);

void user_control(macroblock_t *cur_mbs,
		  macroblock_t *ref1_mbs,
		  macroblock_t *ref2_mbs);

void motion_comp();
void motion_comp_add_coeff(int i);

void extension_data(unsigned int i);

void exit_program(int exitcode) __attribute__ ((noreturn));

int  writer_alloc(void);
void writer_free(int);
int  reader_alloc(void);
void reader_free(int);


// Not implemented
void quant_matrix_extension();
void picture_display_extension();
void picture_spatial_scalable_extension();
void picture_temporal_scalable_extension();
void sequence_scalable_extension();


void fprintbits(FILE *fp, unsigned int bits, uint32_t value)
{
  int n;
  for(n = 0; n < bits; n++) {
    fprintf(fp, "%u", (value>>(bits-n-1)) & 0x1);
  }
}
  








//#define GETBITS32
#define GETBITS64
#define GETBITSMMAP


FILE *infile;

#ifdef GETBITSMMAP // Mmap i/o
void setup_mmap(char *);
void get_next_packet();
uint32_t *buf;
uint32_t buf_size;
struct off_len_packet packet;
uint8_t *mmap_base;

#else // Normal i/o
#define READ_SIZE 1024*8
#define ALLOC_SIZE 2048
#define BUF_SIZE_MAX 1024*8
uint32_t buf[BUF_SIZE_MAX] __attribute__ ((aligned (8)));

#endif

#ifdef GETBITS32
void back_word(void);
void next_word(void);
unsigned int backed = 0;
unsigned int buf_pos = 0;
unsigned int bits_left = 32;
uint32_t cur_word = 0;
#endif

#ifdef GETBITS64
void read_buf(void);
int offs = 0;
unsigned int bits_left = 64;
uint64_t cur_word = 0;
#endif



#ifdef DEBUG
#define GETBITS(a,b) getbits(a,b)
#else
#define GETBITS(a,b) getbits(a)
#endif

/* 5.2.1 Definition of bytealigned() function */
int bytealigned(void)
{
  return !(bits_left%8);
}



/* ---------------------------------------------------------------------- */
#ifdef GETBITS64
#ifdef GETBITSMMAP // (64bit) Discontinuous buffers of variable size


#ifdef DEBUG
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
  result = (cur_word << (64-bits_left)) >> (64-nr);
  //  result = result >> (32-nr);
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


/* 5.2.2 Definition of nextbits() function */
static inline
uint32_t nextbits(unsigned int nr)
{
  //  uint32_t result = (cur_word << (64-bits_left)) >> 32;
  return (cur_word << (64-bits_left)) >> (64-nr);
  //return *((uint32_t*)&cur_word) >> (32-nr); //+
}

#endif
#endif


/* ---------------------------------------------------------------------- */
#ifdef GETBITS64
#ifndef GETBITSMMAP // (64bit) Discontinuous buffers of *static* size


#ifdef DEBUG
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


/* 5.2.2 Definition of nextbits() function */
static inline
uint32_t nextbits(unsigned int nr)
{
  uint32_t result = (cur_word << (64-bits_left)) >> 32;
  return result >> (32-nr);
}

#endif
#endif


/* ---------------------------------------------------------------------- */
#ifdef GETBITS32  // (32bit) 'Normal' getbits, word based.


#ifdef DEBUG
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




int get_vlc(const vlc_table_t *table, char *func) {
  int pos=0;
  int numberofbits=1;
  int vlc;
  
  while(table[pos].numberofbits != VLC_FAIL) {
    vlc=nextbits(numberofbits);
    while(table[pos].numberofbits == numberofbits) {
      if(table[pos].vlc == vlc) {
	DPRINTF(3, "get_vlc(%s): len: %d, vlc: %d, val: %d\n",
		func, numberofbits, table[pos].vlc, table[pos].value);
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
  exit_program(-1);
  return VLC_FAIL;
}


void sighandler(void)
{
  exit_program(0);
}


void init_program()
{
  struct sigaction sig;

  cur_mbs = malloc(36*45*sizeof(macroblock_t));
   ref1_mbs = malloc(36*45*sizeof(macroblock_t));
   ref2_mbs = malloc(36*45*sizeof(macroblock_t));
  
  
  ref_image1 = &r1_img;
  ref_image2 = &r2_img;
  b_image = &b_img;

  display_init();


  // Setup signal handler.

  sig.sa_handler = sighandler;
  sigaction(SIGINT, &sig, NULL);
  


#ifdef STATS
  statistics_init();
#endif


  // init values for MPEG-1
  pic.coding_ext.picture_structure = 0x3;
  pic.coding_ext.frame_pred_frame_dct = 1;
  pic.coding_ext.intra_vlc_format = 0;

  seq.ext.chroma_format = 0x1;

  return;
}

int main(int argc, char **argv)
{
  int c;

  /* Parse command line options */
  while ((c = getopt(argc, argv, "d:r:hs")) != EOF) {
    switch (c) {
    case 'd':
      debug = atoi(optarg);
      break;
    case 'r':
      ring_buffers = atoi(optarg);
      break;
    case 's':
      shmem_flag = 0;
      break;
    case 'h':
    case '?':
      printf ("Usage: %s [-d <level] [-s <buffers>] [-s]\n\n"
	      "  -d <level>   set debug level (default 0)\n"
	      "  -r <buffers> set ringbuffer size (default 4)\n"
	      "  -s           disable shared memory\n", 
	      argv[0]);
      return 1;
    }
  }
  if(optind < argc) {
    infile = fopen(argv[optind], "r");
  } else {
    infile = stdin;
  }
  
  init_program();

#ifdef TIMESTAT
  timestat_init();
#endif

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
    uint32_t new_word1 = GUINT32_FROM_BE(buf[offs++]);
    uint32_t new_word2;
    if(offs >= READ_SIZE/4)
      read_buf();
    new_word2 = GUINT32_FROM_BE(buf[offs++]);
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
  static int first = 1;
  
  if(first) {
    first = 0;
    user_control(cur_mbs, ref1_mbs, ref2_mbs);
  }

  next_start_code();
  if(nextbits(32) == MPEG2_VS_SEQUENCE_HEADER_CODE) {
    DPRINTF(1, "Found Sequence Header\n");

    sequence_header();

    if(nextbits(32) == MPEG2_VS_EXTENSION_START_CODE) {
      MPEG2 = 1;
      sequence_extension();

      if(!shm_ready) {
	setup_shm(seq.horizontal_size, seq.vertical_size);
	//display_init(...);
	shm_ready = 1;
      }
      
      do {
	extension_and_user_data(0);
	do {
#ifdef TIMESTAT
  clock_gettime(CLOCK_REALTIME, &picture_decode_start_time);
#endif
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
      
      //MPEG-1 2000-04-06 start
      
      if(!shm_ready) {
	setup_shm(seq.horizontal_size, seq.vertical_size);
	//display_init(...);
	shm_ready = 1;
      }

      extension_and_user_data(1);
      do {
	group_of_pictures_header();
	extension_and_user_data(1);
	do {
	  picture_header();
	  extension_and_user_data(1);
	  picture_data();
	} while(nextbits(32) == MPEG2_VS_PICTURE_START_CODE);
      } while(nextbits(32) == MPEG2_VS_GROUP_START_CODE);
      
      //MPEG-1 2000-04-06 end
      
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
  
#ifdef DEBUG

void marker_bit(void)
{
  if(!GETBITS(1, "markerbit")) {
    fprintf(stderr, "*** incorrect marker_bit in stream\n");
    exit_program(-1);
  }
}

#else // DEBUG

void marker_bit(void)
{
  dropbits(1);
}

#endif //DEBUG


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
  
  seq.mb_width = (seq.horizontal_size+15)/16;

}

#define BUFFER_FREE 0
#define BUFFER_FULL 1

int writer_alloc(void)
{
  int i;
  for(i=0 ; i<ring_buffers ; i++) {
    if (ring[i].lock == BUFFER_FREE)
      break;
  }
  return i;
}

void writer_free(int buffer)
{
  ring[buffer].lock = BUFFER_FULL;
}

int  reader_alloc(void)
{
  int i;
  for(i=0 ; i<ring_buffers ; i++) {
    if (ring[i].lock == BUFFER_FULL)
      break;
  }
  return i;
}

void reader_free(int buffer)
{
  ring[buffer].lock = BUFFER_FREE;
}

#define INC_8b_ALIGNMENT(a) ((a) + (a)%8)

void setup_shm(int horiz_size, int vert_size)
{ 
  int num_pels     = horiz_size * vert_size;
  int key          = 740828;
  int pagesize     = sysconf(_SC_PAGESIZE);
  int yuv_size     = INC_8b_ALIGNMENT(num_pels) + INC_8b_ALIGNMENT(num_pels/4) * 2; 
  int segment_size = ring_buffers * (yuv_size + yuv_size % pagesize);
  int ring_c_size  = ring_buffers * sizeof(yuv_image_t);
  int i;
  char *baseaddr;

  // Get the ringbuffer shared memory.
  
  if ((ring_shmid = shmget(key, segment_size, IPC_CREAT | 0666)) < 0) {
    perror("shmget ringbuffer");
    exit_program(1);
  }

  // Get the control part.

  if ((ring_c_shmid = shmget(key+1, ring_c_size, IPC_CREAT | 0666)) < 0) {
    perror("shmget ringbuffer control");
    exit_program(1);
  }
  
  /*
   * Now we attach the segments to our data space.
   */
  ring_shmaddr = shmat(ring_shmid, NULL, 0); // Add SHM_SHARE_MMU?
  if(ring_shmaddr == (char *) -1) {
    perror("shmat ringbuffer");
    exit_program(1);
  }
  
  ring = (yuv_image_t *)shmat(ring_c_shmid, NULL, 0); // Add SHM_SHARE_MMU?
  if(ring == (yuv_image_t *) -1) {
    perror("shmat ringbuffer control");
    exit_program(1);
  }

  // Setup the ring buffer.
  //  ring = (yuv_image_t[])malloc(ring_buffers*sizeof(yuv_image_t));
  
  baseaddr = ring_shmaddr;
  for(i=0 ; i<ring_buffers ; i++) {
    ring[i].y = baseaddr;
    ring[i].u = num_pels   + (char *)INC_8b_ALIGNMENT((long)ring[i].y);
    ring[i].v = num_pels/4 + (char *)INC_8b_ALIGNMENT((long)ring[i].u);
    ring[i].lock = BUFFER_FREE;
    baseaddr += yuv_size + yuv_size % pagesize;
  }

  // Fork off the CC/sync stage here.
  
  {}  

  if(ref_image1->y == NULL) {
    DPRINTF(1, "Allocating buffers\n");
#if 0
    ref_image1->y = memalign(8, num_pels);
    ref_image1->u = memalign(8, num_pels/4);
    ref_image1->v = memalign(8, num_pels/4);
    
    ref_image2->y = memalign(8, num_pels);
    ref_image2->u = memalign(8, num_pels/4);
    ref_image2->v = memalign(8, num_pels/4);
    
    b_image->y = memalign(8, num_pels);
    b_image->u = memalign(8, num_pels/4);
    b_image->v = memalign(8, num_pels/4);
#else    
    ref_image1 = &ring[0];
    ref_image2 = &ring[1];
    b_image    = &ring[2];
#endif

  } 
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


#ifdef DEBUG

  DPRINTF(1, "bit rate: %d bits/s\n",
	  400 *
	  ((seq.ext.bit_rate_extension << 12) | seq.header.bit_rate_value));


  DPRINTF(1, "frame rate: ");
  switch(seq.header.frame_rate_code) {
  case 0x0:
    DPRINTF(1, "forbidden\n");
    break;
  case 0x1:
    DPRINTF(1, "24000/1001 (23.976)\n");
    break;
  case 0x2:
    DPRINTF(1, "24\n");
    break;
  case 0x3:
    DPRINTF(1, "25\n");
    break;
  case 0x4:
    DPRINTF(1, "30000/1001 (29.97)\n");
    break;
  case 0x5:
    DPRINTF(1, "30\n");
    break;
  case 0x6:
    DPRINTF(1, "50\n");
    break;
  case 0x7:
    DPRINTF(1, "60000/1001 (59.94)\n");
    break;
  case 0x8:
    DPRINTF(1, "60\n");
    break;
  default:
    DPRINTF(1, "%f (computed)\n",
	    (double)(seq.header.frame_rate_code)*
	    ((double)(seq.ext.frame_rate_extension_n)+1.0)/
	    ((double)(seq.ext.frame_rate_extension_d)+1.0));
    break;
  }
#endif
#if 0
  //this is moved out in case of MPEG-1
  if(!shm_ready) {
    setup_shm(seq.horizontal_size, seq.vertical_size);
    //display_init(...);
    shm_ready = 1;
  }
#endif
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

  seq.mb_row = 0;
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
  
#ifdef TIMESTAT
  clock_gettime(CLOCK_REALTIME, &picture_decode_end_time);

  picture_timestat[picture_decode_num].dec_start = picture_decode_start_time;
  picture_timestat[picture_decode_num].dec_end = picture_decode_end_time;
  picture_timestat[picture_decode_num].type = pic.header.picture_coding_type;

  timesub(&picture_decode_time,
	  &picture_decode_end_time, &picture_decode_start_time);

  /*
  switch(pic.header.picture_coding_type) {
  case 0x1:
    fprintf(stderr, "I ");
    break;
  case 0x2:
    fprintf(stderr, "P ");
    break;
  case 0x3:
    fprintf(stderr, "B ");
    break;
  }
  */
  
  switch(pic.header.picture_coding_type) {
  case 0x1:
    timeadd(&picture_decode_time_tot[0],
	    &picture_decode_time_tot[0], &picture_decode_time);

    if(timecompare(&picture_decode_time, &picture_decode_time_max[0]) > 0) {
      picture_decode_time_max[0] = picture_decode_time;
    }
    if(timecompare(&picture_decode_time_min[0], &picture_decode_time) > 0) {
      picture_decode_time_min[0] = picture_decode_time;
    }
    picture_decode_nr[0]++;
    break;
  case 0x2:
    timeadd(&picture_decode_time_tot[1],
	    &picture_decode_time_tot[1], &picture_decode_time);
    if(timecompare(&picture_decode_time, &picture_decode_time_max[1]) > 0) {
      picture_decode_time_max[1] = picture_decode_time;
    }
    if(timecompare(&picture_decode_time_min[1], &picture_decode_time) > 0) {
      picture_decode_time_min[1] = picture_decode_time;
    }
    picture_decode_nr[1]++;
    break;
  case 0x3:
    timeadd(&picture_decode_time_tot[2],
	    &picture_decode_time_tot[2], &picture_decode_time);
    if(timecompare(&picture_decode_time, &picture_decode_time_max[2]) > 0) {
      picture_decode_time_max[2] = picture_decode_time;
    }
    if(timecompare(&picture_decode_time_min[2], &picture_decode_time) > 0) {
      picture_decode_time_min[2] = picture_decode_time;
    }
    picture_decode_nr[2]++;
    break;
  }
  if(picture_decode_num < TIMESTAT_NOF) {
    picture_decode_num++;
  }
#endif



#ifdef TIMESTAT
  clock_gettime(CLOCK_REALTIME, &picture_display_start_time);
#endif



  frame_done(current_image, cur_mbs, 
	     ref_image1, ref1_mbs, 
	     ref_image2, ref2_mbs,
	     pic.header.picture_coding_type);
  
#ifdef TIMESTAT
  clock_gettime(CLOCK_REALTIME, &picture_display_end_time);
#endif


#ifdef TIMESTAT
  /*
  fprintf(stderr, "decodetime: %d.%09d  %.3f\n",
	  picture_decode_time.tv_sec,
	  picture_decode_time.tv_nsec,
	  (double)picture_decode_time.tv_sec+
	  (double)picture_decode_time.tv_nsec/1000000000.0);
  */
  picture_timestat[picture_decode_num-1].disp_start =picture_display_start_time;
  picture_timestat[picture_decode_num-1].disp_end = picture_display_end_time;
  

  timesub(&picture_display_time,
	  &picture_display_end_time, &picture_display_start_time);
  /*
  fprintf(stderr, "displaytime: %d.%09d\n",
	  picture_display_time.tv_sec,
	  picture_display_time.tv_nsec);
  */
  switch(pic.header.picture_coding_type) {
  case 0x1:
    timeadd(&picture_display_time_tot[0],
	    &picture_display_time_tot[0], &picture_display_time);
    if(timecompare(&picture_display_time, &picture_display_time_max[0]) > 0) {
      picture_display_time_max[0] = picture_display_time;
    }
    if(timecompare(&picture_display_time_min[0], &picture_display_time) > 0) {
      picture_display_time_min[0].tv_sec = picture_display_time.tv_sec;
      picture_display_time_min[0].tv_nsec = picture_display_time.tv_nsec;
    }
    picture_display_nr[0]++;
    break;
  case 0x2:
    timeadd(&picture_display_time_tot[1],
	    &picture_display_time_tot[1], &picture_display_time);
    if(timecompare(&picture_display_time, &picture_display_time_max[1]) > 0) {
      picture_display_time_max[1] = picture_display_time;
    }
    if(timecompare(&picture_display_time_min[1], &picture_display_time) > 0) {
      picture_display_time_min[1] = picture_display_time;
    }
    picture_display_nr[1]++;
    break;
  case 0x3:
    timeadd(&picture_display_time_tot[2],
	    &picture_display_time_tot[2], &picture_display_time);
    if(timecompare(&picture_display_time, &picture_display_time_max[2]) > 0) {
      picture_display_time_max[2] = picture_display_time;
    }
    if(timecompare(&picture_display_time_min[2], &picture_display_time) > 0) {
      picture_display_time_min[2] = picture_display_time;
    }
    picture_display_nr[2]++;
    break;
  }
  if(picture_display_num < TIMESTAT_NOF) {
    picture_display_num++;
  }

  
  picture_time0 = picture_time1;
  picture_time1 = picture_time;
  timesub(&picture_time,
	  &picture_display_end_time, &picture_decode_start_time);
  picture_time2 = picture_time;
  
  /*
  fprintf(stderr, "picturetime: %d.%09d\n",
	  picture_time.tv_sec,
	  picture_time.tv_nsec);

  fprintf(stderr, "picturetime avg 3frames: %.4f\n",
	  (((double)picture_time0.tv_sec+
	    (double)picture_time0.tv_nsec/1000000000.0) +
	   ((double)picture_time1.tv_sec+
	    (double)picture_time1.tv_nsec/1000000000.0) +
	   ((double)picture_time2.tv_sec+
	    (double)picture_time2.tv_nsec/1000000000.0)) /
	  3.0);
  */  
#endif


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
#ifdef HAVE_MMX
  emms();
#endif
  
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
    if(MPEG2) {
      seq.mb_row = slice_data.slice_vertical_position - 1;
    }
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
#ifdef STATS
  new_scaled = 1;
#endif
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

#if 0
  // MPEG-1 start   (this does not occur in an MPEG-2 stream)
  //                 (not a valid macroblock_escape code)
  
  while(nextbits(11) == 0x00F) {
    mb.macroblock_escape = GETBITS(11, "macroblock_stuffing");
  }
  
  // MPEG-1 end
#endif

  while(nextbits(11) == 0x008) {
    mb.macroblock_escape = GETBITS(11, "macroblock_escape");
    inc_add+=33;
  }

  mb.macroblock_address_increment = get_vlc(table_b1, "macroblock_address_increment");

  mb.macroblock_address_increment+= inc_add;
  
  seq.macroblock_address =  mb.macroblock_address_increment + seq.previous_macroblock_address;
  
  seq.previous_macroblock_address = seq.macroblock_address;




  seq.mb_column = seq.macroblock_address % seq.mb_width;
  if(!MPEG2) {
    seq.mb_row = seq.macroblock_address / seq.mb_width;
  }
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
    
    if(show_stat) {
      for(seq.macroblock_address = seq.macroblock_address-mb.macroblock_address_increment+1; 
	  seq.macroblock_address < old_address; 
	  seq.macroblock_address++) {
	switch(pic.header.picture_coding_type) {
	case 0x1:
	case 0x2:
	  memcpy(&ref2_mbs[seq.macroblock_address], &mb, sizeof(mb));
	  break;
	case 0x3:
	  memcpy(&cur_mbs[seq.macroblock_address], &mb, sizeof(mb));
	  break;
	}
      }
    }
    
    /* Skipped blocks never have any DCT-coefficients */
    {
      int i;
      for(i = 0; i < 12; i++)
	mb.pattern_code[i] = 0;
      // mb.cbp = 0;
    }
    
    switch(pic.header.picture_coding_type) {
    
    case 0x2:
      DPRINTF(3,"skipped in P-frame\n");
      for(x = (seq.mb_column-mb.macroblock_address_increment+1)*16, y = seq.mb_row*16;
	  y < (seq.mb_row+1)*16; y++) {
	memcpy(&dst_image->y[y*seq.horizontal_size+x], 
	       &ref_image1->y[y*seq.horizontal_size+x], 
	       (mb.macroblock_address_increment-1)*16);
      }
      
      for(x = (seq.mb_column-mb.macroblock_address_increment+1)*8, y = seq.mb_row*8;
	  y < (seq.mb_row+1)*8; y++) {
	memcpy(&dst_image->u[y*seq.horizontal_size/2+x], 
	       &ref_image1->u[y*seq.horizontal_size/2+x], 
	       (mb.macroblock_address_increment-1)*8);
      }
      
      for(x = (seq.mb_column-mb.macroblock_address_increment+1)*8, y = seq.mb_row*8;
	  y < (seq.mb_row+1)*8; y++) {
	memcpy(&dst_image->v[y*seq.horizontal_size/2+x], 
	       &ref_image1->v[y*seq.horizontal_size/2+x], 
	       (mb.macroblock_address_increment-1)*8);
      }
      
      break;
    
    case 0x3:
      DPRINTF(3,"skipped in B-frame\n");
      if(pic.coding_ext.picture_structure == 0x03 /*frame*/) {
	/* 7.6.6.4  B frame picture */
	mb.prediction_type = PRED_TYPE_FRAME_BASED;
	mb.motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FRAME;
	mb.dmv = 0;
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
#ifdef STATS
    new_scaled = 1;
#endif
  }
  
#ifdef STATS
  stats_quantiser_scale_possible++;
  if(new_scaled) {
    stats_quantiser_scale_nr++;
  }
  if(mb.modes.macroblock_intra) {
    stats_intra_quantiser_scale_possible++;
    if(new_scaled) {
      stats_intra_quantiser_scale_nr++;
      new_scaled = 0;
    }
  } else {
    stats_non_intra_quantiser_scale_possible++;
    if(new_scaled) {
      stats_non_intra_quantiser_scale_nr++;
      new_scaled = 0;
    }
  }    
#endif
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
   

  /* Table 6-20 block_count as a function of chroma_format */
  if(seq.ext.chroma_format == 0x01) {
    block_count = 6;
  } else if(seq.ext.chroma_format == 0x02) {
    block_count = 8;
  } else if(seq.ext.chroma_format == 0x03) {
    block_count = 12;
  }
  
  
  
  /* Intra blocks always have all sub block and are writen directly 
     to the output buffers by block() */
  if(mb.modes.macroblock_intra) {
    int i;
    for(i = 0; i < block_count; i++) {  
      //  mb.pattern_code[i] = 1;
      block_intra(i);
    }
  } 
  else {
    int i;
    
    motion_comp();    

    for(i = 0; i < 12; i++) {  // !!!!
      mb.pattern_code[i] = 0;
    } 
    
    if(mb.modes.macroblock_pattern) {

      coded_block_pattern();
      
      for(i = 0; i < 6; i++) {
	if(mb.cbp & (1<<(5-i))) {
	  DPRINTF(4, "cbpindex: %d set\n", i);
	  mb.pattern_code[i] = 1;
	  block_non_intra(i);
	  motion_comp_add_coeff(i);
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
    

  if(show_stat) {
    switch(pic.header.picture_coding_type) {
    case 0x1:
    case 0x2:
      memcpy(&ref2_mbs[seq.macroblock_address], &mb, sizeof(mb));
      break;
    case 0x3:
      memcpy(&cur_mbs[seq.macroblock_address], &mb, sizeof(mb));
      break;
    } 
  }
  
#if 0
  /* Intra blocks are already handled directly in block() */
  if(mb.modes.macroblock_intra == 0) {
    motion_comp();
  }
#endif
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
    exit_program(1);
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
    exit_program(1);
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
    exit_program(1);
  }
  
  dropbits(tab->len);
  
  if (tab->run==64) { // end_of_block 
    run = VLC_END_OF_BLOCK;
    val = VLC_END_OF_BLOCK;
  } 
  else if (tab->run==65) { /* escape */
#if 0
    if( MPEG2 ) {
#endif
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
#if 0
    } else {
      uint32_t tmp = GETBITS(6+8, "(get_dct escape - run & level )");
      run = tmp >> 8;
      val = tmp & 0x7f;
      if (val == 0) {
	val =  GETBITS(8, "(get_dct escape - ext. level )");
	if(val < 128) {
	  fprintf(stderr, " invalid extended dct escape MPEG-1\n");
	}
      }
      if (tmp & 0x80) {
	val = -val;
      }
    }
#endif
  } else {
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
    exit_program(1);
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
    exit_program(1);
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
    exit_program(1);
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
      
      int inverse_quantisation_sum;
      
      DPRINTF(3, "pattern_code(%d) set\n", i);
      
      // Reset all coefficients to 0.
      {
	int m;
	for(m=0; m<16; m++)
	  memset( ((uint64_t *)mb.QFS) + m, 0, sizeof(uint64_t) );
      }
      
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
    mlib_VideoIDCT8x8_U8_S16(dst, (int16_t *)mb.QFS, stride);
  }
}

void block_non_intra(unsigned int b)
{
  uint8_t i;
  int f;

#ifdef STATS
  stats_block_non_intra_nr++;
#endif
  
  {
    int n;
    
    runlevel_t runlevel;
    
    int inverse_quantisation_sum = 0;
    
    DPRINTF(3, "pattern_code(%d) set\n", b);
    
    // Reset all coefficients to 0.
    {
      int m;
      for(m=0; m<16; m+=4)
	memset( ((uint64_t *)mb.QFS) + m, 0, 4*sizeof(uint64_t) );
    }
    
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

#if 0    
    if(f > 2047) {
      fprintf(stderr ," **clipped 1\n");
      f = 2047;
    } else if(f < -2048) {
      fprintf(stderr ," **clipped 2\n");
      f = -2048;
    }
#endif

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
	exit_program(1);
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

	if( sgn )
	  f = -f;
	
#if 0
	if(f > 2047)
	  f = 2047;
	else if(f < -2048)
	  f = -2048;
#endif
	mb.QFS[i] = f;
	inverse_quantisation_sum += f;
	
	n++;      
      }
    }
    inverse_quantisation_final(inverse_quantisation_sum);
    
    DPRINTF(4, "nr of coeffs: %d\n", n);
  }
  //  mlib_VideoIDCT8x8_S16_S16(mb.f[b], (int16_t *)mb.QFS);
  mlib_VideoIDCT8x8_S16_S16((int16_t *)mb.QFS, (int16_t *)mb.QFS);
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

  /*
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
  */
  
}

void motion_comp_add_coeff(int i)
{

  int width,x,y;
  int stride;
  int d;
  uint8_t *dst_y,*dst_u,*dst_v;

  width = seq.horizontal_size;


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


  switch(i) {
  case 0:
    mlib_VideoAddBlock_U8_S16(dst_y, mb.QFS, stride);
    break;
  case 1:
    mlib_VideoAddBlock_U8_S16(dst_y + 8, mb.QFS, stride);
    break;
  case 2:
    mlib_VideoAddBlock_U8_S16(dst_y + width * d, mb.QFS, stride);
    break;
  case 3:
    mlib_VideoAddBlock_U8_S16(dst_y + width * d + 8, mb.QFS, stride);
    break;
  case 4:
    mlib_VideoAddBlock_U8_S16(dst_u, mb.QFS, width/2);
    break;
  case 5:
    mlib_VideoAddBlock_U8_S16(dst_v, mb.QFS, width/2);
    break;
  }
}

/* 6.2.3.2 Quant matrix extension */
void quant_matrix_extension()
{
  GETBITS(4, "extension_start_code_identifier");
  seq.header.load_intra_quantiser_matrix = 
    GETBITS(1, "load_intra_quantiser_matrix");
  if(seq.header.load_intra_quantiser_matrix) {
    int n;
    intra_inverse_quantiser_matrix_changed = 1;
#ifdef STATS
    stats_intra_inverse_quantiser_matrix_loaded = 1;
#endif
    for(n = 0; n < 64; n++) {
      seq.header.intra_quantiser_matrix[n] = 
        GETBITS(8, "intra_quantiser_matrix[n]");
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
  }
  seq.header.load_non_intra_quantiser_matrix = 
    GETBITS(1, "load_non_intra_quantiser_matrix");
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
  }
  GETBITS(1, "load_chroma_intra_quantiser_matrix (always 0 in 4:2:0)");
  GETBITS(1, "load_chroma_non_intra_quantiser_matrix (always 0 in 4:2:0)");
  next_start_code();
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


void exit_program(int exitcode)
{
  display_exit();

  shmdt ( ring_shmaddr);
  shmdt ( (char *)ring);
  shmctl (ring_shmid, IPC_RMID, 0);
  shmctl (ring_c_shmid, IPC_RMID, 0);

#ifdef HAVE_MMX
  emms();
#endif
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

#ifdef TIMESTAT
  timestat_print();
#endif

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
  
  fprintf(stderr, "\nCompiled with:\n");
#ifdef DEBUG
  fprintf(stderr, "\t#define DEBUG\n");
#endif

#ifdef STATS
  fprintf(stderr, "\t#define STATS\n");
#endif

#ifdef DCFLAGS
  fprintf(stderr, "\tCFLAGS = %s\n", DCFLAGS);
#endif

#ifdef DLDFLAGS
  fprintf(stderr, "\tLDFLAGS = %s\n", DLDFLAGS);
#endif

  exit(exitcode);
}
