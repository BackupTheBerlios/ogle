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

#include "common.h"
#include "msgtypes.h"
#include "queue.h"
#include "msgevents.h"

int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);
int get_q();
int attach_ctrl_shm(int shmid);


char *program_name;

int ctrl_data_shmid;
ctrl_data_t *ctrl_data;
ctrl_time_t *ctrl_time;

int stream_shmid;
char *stream_shmaddr;
int msgqid = -1;

static MsgEventQ_t *msgq;

MsgEventClient_t demux_client;
MsgEventClient_t spu_client;
MsgEventClient_t nav_client;


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
  
  {
    MsgEvent_t ev;

    fprintf(stderr, "ui: opening msg q\n");
    
    // get a handle
    if((msgq = MsgOpen(msgqid)) == NULL) {
      fprintf(stderr, "ui: couldn't get message q\n");
      exit(-1);
    }
    
    fprintf(stderr, "ui: msg open q\n");
    
    // register with the resource manager and tell what we can do
    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = UI_DVD_CLI | UI_MPEG_CLI;
    
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev) == -1) {
      fprintf(stderr, "ui: couldn't register\n");
    }

    fprintf(stderr, "ui: registered q\n");
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = DEMUX_MPEG2_PS | DEMUX_MPEG1;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev) == -1) {
      fprintf(stderr, "ui: didn't get cap\n");
    }

    fprintf(stderr, "ui: requested capability q\n");


    while(!demux_client) {
      MsgNextEvent(msgq, &ev);
      switch(ev.type) {
      case MsgEventQGntCapability:
	if((ev.gntcapability.capability & (DEMUX_MPEG2_PS | DEMUX_MPEG1))
	   == (DEMUX_MPEG2_PS | DEMUX_MPEG1)) {
	  demux_client = ev.gntcapability.capclient;
	} else {
	  fprintf(stderr, "ui: capabilities not requested\n");
	}
	break;
      default:
	fprintf(stderr, "ui: event not wanted %d, from %ld\n",
		ev.type, ev.any.client);
	break;
      }
    }

    if(demux_client) {
      fprintf(stderr, "ui: got capability\n");
    }
    

  }
  input();
  return 0;
}





int request_spu()
{
  MsgEvent_t ev;
  ev.type = MsgEventQReqCapability;
  ev.reqcapability.capability = DECODE_DVD_SPU;
  if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev) == -1) {
    fprintf(stderr, "ui: didn't get cap\n");
  }
  
  while(!spu_client) {
    MsgNextEvent(msgq, &ev);
    switch(ev.type) {
    case MsgEventQGntCapability:
      if((ev.gntcapability.capability & (DECODE_DVD_SPU))
	 == (DECODE_DVD_SPU)) {
	spu_client = ev.gntcapability.capclient;
      } else {
	fprintf(stderr, "ui: capabilities not requested\n");
      }
      break;
    default:
      fprintf(stderr, "ui: event not wanted %d, from %ld\n",
	      ev.type, ev.any.client);
      break;
    }
  }
  return 0;
}


int request_nav()
{
  MsgEvent_t ev;
  ev.type = MsgEventQReqCapability;
  ev.reqcapability.capability = DECODE_DVD_NAV;
  if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev) == -1) {
    fprintf(stderr, "ui: didn't get cap\n");
  }
  
  while(!nav_client) {
    MsgNextEvent(msgq, &ev);
    switch(ev.type) {
    case MsgEventQGntCapability:
      if((ev.gntcapability.capability & (DECODE_DVD_NAV))
	 == (DECODE_DVD_NAV)) {
	nav_client = ev.gntcapability.capclient;
      } else {
	fprintf(stderr, "ui: capabilities not requested\n");
      }
      break;
    default:
      fprintf(stderr, "ui: event not wanted %d, from %ld\n",
	      ev.type, ev.any.client);
      break;
    }
  }
  return 0;
}




#define CMDSTR_LEN 256
int input() {
  char cmdstr[CMDSTR_LEN];
  char *cmdtokens[16];
  char *tok;
  int m,n;
  int time;
  MsgEventClient_t rcpt;
  MsgEvent_t sendev;
  

  fprintf(stderr, "****input()\n");
  while(!feof(infile)) {
    rcpt = demux_client;
    sendev.type = MsgEventQNone;
    
    fgets(cmdstr, CMDSTR_LEN, infile);
    cmdstr[strlen(cmdstr)-1] = '\0';
    
    fprintf(stderr, "****input() got line\n");
    
    
    fprintf(stderr, "string: .%s.\n", cmdstr);
    tok = strtok(cmdstr, " ");
    fprintf(stderr, "cmd: .%s.\n", tok);
    if(strcmp(tok, "play") == 0) {
      sendev.type = MsgEventQPlayCtrl;
      sendev.playctrl.cmd = PlayCtrlCmdPlay;
      fprintf(stderr, "****input() play\n");
      
    } else if(strcmp(tok, "play_from") == 0) {
      sendev.type = MsgEventQPlayCtrl;
      sendev.playctrl.cmd = PlayCtrlCmdPlayFrom;
      tok = strtok(NULL, " ");
      sendev.playctrl.from = strtol(tok, NULL, 0);

    } else if(strcmp(tok, "play_to") == 0) {
      sendev.type = MsgEventQPlayCtrl;
      sendev.playctrl.cmd = PlayCtrlCmdPlayTo;
      tok = strtok(NULL, " ");
      sendev.playctrl.to = strtol(tok, NULL, 0);
      
    } else if(strcmp(tok, "play_from_to") == 0) {
      sendev.type = MsgEventQPlayCtrl;
      sendev.playctrl.cmd = PlayCtrlCmdPlayFromTo;
      tok = strtok(NULL, " ");
      sendev.playctrl.from = strtol(tok, NULL, 0);
      tok = strtok(NULL, " ");
      sendev.playctrl.to = strtol(tok, NULL, 0);
      
    } else if(strcmp(tok, "play_from_s") == 0) {
      sendev.type = MsgEventQPlayCtrl;
      sendev.playctrl.cmd = PlayCtrlCmdPlayFrom;
      tok = strtok(NULL, " ");
      sendev.playctrl.from = strtol(tok, NULL, 0)*2048;

    } else if(strcmp(tok, "play_to_s") == 0) {
      sendev.type = MsgEventQPlayCtrl;
      sendev.playctrl.cmd = PlayCtrlCmdPlayTo;
      tok = strtok(NULL, " ");
      sendev.playctrl.to = (strtol(tok, NULL, 0)+1)*2048;
      
    } else if(strcmp(tok, "play_from_to_s") == 0) {
      sendev.type = MsgEventQPlayCtrl;
      sendev.playctrl.cmd = PlayCtrlCmdPlayFromTo;
      tok = strtok(NULL, " ");
      sendev.playctrl.from = strtol(tok, NULL, 0)*2048;
      tok = strtok(NULL, " ");
      sendev.playctrl.to = (strtol(tok, NULL, 0)+1)*2048;
      
    } else if(strcmp(tok, "stop") == 0) {
      sendev.type = MsgEventQPlayCtrl;
      sendev.playctrl.cmd = PlayCtrlCmdStop;
      fprintf(stderr, "****input() stop\n");


      /******** debug commands *********/

    } else if(strcmp(tok, "palette") == 0) {
      int n;
      sendev.type = MsgEventQSPUPalette;
      if(!spu_client) {
	request_spu();
      }
      rcpt = spu_client;

      for(n = 0; n < 16; n++) {
	tok = strtok(NULL, " ");
	sendev.spupalette.colors[n] = strtol(tok, NULL, 0);
      }
      
    } else if(strcmp(tok, "highlight") == 0) {
      int n;
      sendev.type = MsgEventQSPUHighlight;
      if(!spu_client) {
	request_spu();
      }
      rcpt = spu_client;
      
      tok = strtok(NULL, " ");
      sendev.spuhighlight.x_start = strtol(tok, NULL, 0);
      tok = strtok(NULL, " ");
      sendev.spuhighlight.y_start = strtol(tok, NULL, 0);
      tok = strtok(NULL, " ");
      sendev.spuhighlight.x_end = strtol(tok, NULL, 0);
      tok = strtok(NULL, " ");
      sendev.spuhighlight.y_end = strtol(tok, NULL, 0);
      for(n = 0; n < 4; n++) {
	tok = strtok(NULL, " ");
	sendev.spuhighlight.color[n] =
	  (unsigned char)strtol(tok, NULL, 0);
      }
      for(n = 0; n < 4; n++) {
	tok = strtok(NULL, " ");
	sendev.spuhighlight.contrast[n] =
	  (unsigned char)strtol(tok, NULL, 0);
      }
      
    } else if(strcmp(tok, "file") == 0) {
      sendev.type = MsgEventQChangeFile;
      
      tok = strtok(NULL, "\n");

      strncpy(sendev.changefile.filename, tok, PATH_MAX);
      sendev.changefile.filename[PATH_MAX] = '\0';
      
    } else if(strcmp(tok, "btn") == 0) {
      sendev.type = MsgEventQDVDCtrl;
      if(!nav_client) {
	request_nav();
      }
      rcpt = nav_client;
      tok = strtok(NULL, " ");
      if(strcmp(tok, "up") == 0) {
	sendev.dvdctrl.cmd.type = DVDCtrlUpperButtonSelect;
      } else if(strcmp(tok, "down") == 0) {
	sendev.dvdctrl.cmd.type = DVDCtrlLowerButtonSelect;
      } else if(strcmp(tok, "left") == 0) {
	sendev.dvdctrl.cmd.type = DVDCtrlLeftButtonSelect;
      } else if(strcmp(tok, "right") == 0) {
	sendev.dvdctrl.cmd.type = DVDCtrlRightButtonSelect;
      } else if(strcmp(tok, "activate") == 0) {
	sendev.dvdctrl.cmd.type = DVDCtrlButtonActivate;
      }
    } else if(strcmp(tok, "btnnr") == 0) {
      sendev.type = MsgEventQDVDCtrl;
      if(!nav_client) {
	request_nav();
      }
      rcpt = nav_client;
      tok = strtok(NULL, " ");
      if(strcmp(tok, "activate") == 0) {
	sendev.dvdctrl.cmd.type = DVDCtrlButtonSelectAndActivate;
      } else if(strcmp(tok, "select") == 0) {
	sendev.dvdctrl.cmd.type = DVDCtrlButtonSelect;
      }
      tok = strtok(NULL, " ");
      sendev.dvdctrl.cmd.button.nr = strtol(tok, NULL, 0);	

    } else if(strcmp(tok, "speed") == 0) {
      rcpt = CLIENT_RESOURCE_MANAGER;
      sendev.type = MsgEventQSpeed;
      tok = strtok(NULL, " ");
      sscanf(tok, "%lf", &sendev.speed.speed);
    }
    

    fprintf(stderr, "****input() tok end\n");

    if(sendev.type != MsgEventQNone) {
      if(MsgSendEvent(msgq, rcpt, &sendev) == -1) {
	fprintf(stderr, "ui: couldn't send user cmd\n");
      }
    }

    fprintf(stderr, "****input() sent cmd\n");
    
    
  }
  
  fprintf(stderr, "****input() exit\n");
  
  fclose(infile);
  // send command quit

  exit(0);
}
