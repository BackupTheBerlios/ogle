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
#include <string.h>

#include "../include/common.h"
#include "../include/msgtypes.h"
#include "../include/queue.h"


int wait_for_msg(cmdtype_t cmdtype);
int eval_msg(cmd_t *cmd);
int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);
int get_q();
int attach_ctrl_shm(int shmid);

void print_time_base_offset(uint64_t PTS, int scr_nr);
int set_time_base(uint64_t PTS, int scr_nr, struct timespec offset);
struct timespec get_time_base_offset(uint64_t PTS, int scr_nr);


char *program_name;

int ctrl_data_shmid;
ctrl_data_t *ctrl_data;
ctrl_time_t *ctrl_time;

int stream_shmid;
char *stream_shmaddr;
int msgqid = -1;

void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", 
	  program_name);
}

int filefd;
char *mmap_base;
FILE *infile;
struct stat statbuf;

int main(int argc, char *argv[])
{
  int c; 
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


  

  if((infile = fopen("/tmp/cmd", "r")) == NULL) {
    fprintf(stderr, "******* file err\n");
  }

  input();
  return 0;
}


int send_msg(msg_t *msg, int mtext_size)
{
  if(msgsnd(msgqid, msg, mtext_size, 0) == -1) {
    perror("ctrl: msgsnd1");
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
	      MTYPE_CTRL_INPUT, 0) == -1) {
      perror("msgrcv");
      return -1;
    } else {
      fprintf(stderr, "ui: got msg\n");
      eval_msg(cmd);
    }
    if(cmdtype == CMD_ALL) {
      break;
    }
  }
  return 0;
}




int eval_msg(cmd_t *cmd)
{
  
  switch(cmd->cmdtype) {
  default:
    fprintf(stderr, "ui: unrecognized command cmdtype: %x\n",
	    cmd->cmdtype);
    return -1;
    break;
  }
  
  return 0;
}



#define CMDSTR_LEN 256
int input() {
  msg_t msg;
  cmd_t *sendcmd;
  ctrlcmd_t cmd;
  char cmdstr[CMDSTR_LEN];
  char *cmdtokens[16];
  char *tok;
  int m,n;
  int time;
  
  sendcmd = (cmd_t *)&msg.mtext;
  msg.mtype = MTYPE_CTL;
  sendcmd->cmdtype = CMD_CTRL_CMD;

  fprintf(stderr, "****input()\n");
  while(!feof(infile)) {
    fgets(cmdstr, CMDSTR_LEN, infile);
    fprintf(stderr, "****input() got line\n");
    
    
    cmd = CTRLCMD_NONE;
    fprintf(stderr, "string: .%s.\n", cmdstr);
    tok = strtok(cmdstr, " ");
    fprintf(stderr, "cmd: .%s.\n", tok);
    if(strcmp(tok, "play") == 0) {
      cmd = CTRLCMD_PLAY;
      fprintf(stderr, "****input() play\n");
      
    } else if(strcmp(tok, "play_from") == 0) {
      cmd = CTRLCMD_PLAY_FROM;
      tok = strtok(NULL, " ");
      sendcmd->cmd.ctrl_cmd.off_from = strtol(tok, NULL, 0);

    } else if(strcmp(tok, "play_to") == 0) {
      cmd = CTRLCMD_PLAY_TO;
      tok = strtok(NULL, " ");
      sendcmd->cmd.ctrl_cmd.off_to = strtol(tok, NULL, 0);
      
    } else if(strcmp(tok, "play_from_to") == 0) {
      cmd = CTRLCMD_PLAY_FROM_TO;
      tok = strtok(NULL, " ");
      sendcmd->cmd.ctrl_cmd.off_from = strtol(tok, NULL, 0);
      tok = strtok(NULL, " ");
      sendcmd->cmd.ctrl_cmd.off_to = strtol(tok, NULL, 0);
      
    } else if(strcmp(tok, "play_from_s") == 0) {
      cmd = CTRLCMD_PLAY_FROM;
      tok = strtok(NULL, " ");
      sendcmd->cmd.ctrl_cmd.off_from = strtol(tok, NULL, 0)*2048;

    } else if(strcmp(tok, "play_to_s") == 0) {
      cmd = CTRLCMD_PLAY_TO;
      tok = strtok(NULL, " ");
      sendcmd->cmd.ctrl_cmd.off_to = (strtol(tok, NULL, 0)+1)*2048;
      
    } else if(strcmp(tok, "play_from_to_s") == 0) {
      cmd = CTRLCMD_PLAY_FROM_TO;
      tok = strtok(NULL, " ");
      sendcmd->cmd.ctrl_cmd.off_from = strtol(tok, NULL, 0)*2048;
      tok = strtok(NULL, " ");
      sendcmd->cmd.ctrl_cmd.off_to = (strtol(tok, NULL, 0)+1)*2048;
      
    } else if(strcmp(tok, "stop") == 0) {
      cmd = CTRLCMD_STOP;
      fprintf(stderr, "****input() stop\n");
      
    }
    

    fprintf(stderr, "****input() tok end\n");

    if(cmd != CTRLCMD_NONE) {
      // send command
      
      sendcmd->cmd.ctrl_cmd.ctrlcmd = cmd;
      
      switch(cmd) {
      case CTRLCMD_PLAY:
	break;
      case CTRLCMD_PLAY_FROM:
	break;
      case CTRLCMD_STOP:
	break;
      default:
	break;
      }
      
      fprintf(stderr, "****input() sent cmd\n");
      
      send_msg(&msg, sizeof(cmdtype_t)+sizeof(cmd_ctrl_cmd_t));
      
    }
    
  }

  fprintf(stderr, "****input() exit\n");
    
  fclose(infile);
  // send command quit

  exit(0);
}
