#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
//#include <siginfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/common.h"

#define BUF_SIZE 2048
#define FALSE 0
#define TRUE  1

extern char *optarg;
extern int   optind, opterr, optopt;


// #define DEBUG

#ifdef DEBUG
#define DPRINTF(level, text...) \
if(debug > level) \
{ \
    fprintf(stderr, ## text); \
}
#else
#define DPRINTF(level, text...)
#endif

void usage()
{
  fprintf(stderr, "Usage: play_pes [-v <video file>] <input file>\n");
}

int main(int argc, char **argv)
{
  int c, rv; 
  char *infilename;
  int   debug       = 0;
  int   video       = 0;
  FILE *video_file  = NULL;
  int   infilefd;
  long  infilelen;

  struct stat statbuf;
  struct {
    uint32_t type;
    struct load_file_packet body;
  } lf_pack;

  struct {
    uint32_t type;
    struct off_len_packet body;
  } ol_pack;


  /* Parse command line options */
  while ((c = getopt(argc, argv, "v:d:h?")) != EOF) {
    switch (c) {
    case 'v':
      video_file = fopen(optarg,"w");
      if(!video_file) {
	  perror(optarg);
	  exit(1);
	}
      video=1;
      break;
    case 'd':
      debug = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }
  
  if(!video) {
    usage();
    return 1;
  }

  if(argc - optind != 1){
    usage();
    return 1;
  }

  infilename = (char *)(argv[optind]);

  infilefd = open(infilename, O_RDONLY);
  if(infilefd == -1) {
    perror(infilename);
    exit(1);
  }
  rv = fstat(infilefd, &statbuf);
  if (rv == -1) {
    perror("fstat");
    exit(1);
  }
  infilelen = statbuf.st_size;
  
  //Sending "load-this-file" packet to listener
  
  lf_pack.type        = PACK_TYPE_LOAD_FILE;
  lf_pack.body.length = strlen(infilename);
  strcpy((char *)&(lf_pack.body.filename), infilename);
  
  if(video)
    fwrite(&lf_pack, lf_pack.body.length+8, 1, video_file);

  /* Send the "play the whole goddamn thing"-packet. */

  ol_pack.type        = PACK_TYPE_OFF_LEN;
  ol_pack.body.offset = 0;
  ol_pack.body.length = infilelen;

  if(video)
    fwrite(&ol_pack, 12, 1, video_file);

  fclose(video_file);

  // That's it, folks!

  exit(0);

}

