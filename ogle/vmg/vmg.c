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

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

#include "common.h"
#include "msgtypes.h"
#include "queue.h"
#include "ip_sem.h"
#include "timemath.h"
//#include "sync.h"

extern int send_msg(msg_t *msg, int mtext_size);
extern int wait_for_msg(cmdtype_t cmdtype);
extern int eval_msg(cmd_t *cmd);
extern int get_q(char *buffer);

extern int file_open(char *infile);
extern int attach_ctrl_shm(int shmid);
extern int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);

extern int msgqid;

char *program_name;

char buffer[2048];

void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", 
	  program_name);
}

int main(int argc, char *argv[])
{
  int c;
  FILE *outfile;
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
    wait_for_msg(CMD_DECODE_STREAM_BUFFER);
  } else {
    fprintf(stderr, "what?\n");
  }
  
  
  while(1) {
    int len;
    len = get_q(&buffer);
    fwrite(&buffer, len, 1, outfile);
  }

  return 0;
}



