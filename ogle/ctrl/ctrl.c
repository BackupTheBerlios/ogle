#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "../include/msgtypes.h"
#include "../include/queue.h"
#include "../mpeg2_program/programstream.h"

int create_msgq();
int init_demux(char *msqqid_str);
int init_decode(char *msgqid_str);
int chk_for_msg(void);
int eval_msg(cmd_t *cmd);
int send_msg(msg_t *msg, int mtext_size);
int get_buffer(int size, shm_bufinfo_t *bufinfo);
int create_q(int nr_of_elems, int buf_shmid);
int wait_for_msg(cmdtype_t cmdtype);
int create_ctrl_data();

void int_handler();
void remove_q_shm();
void add_q_shmid(int shmid);
void destroy_msgq();

int msgqid;

int ctrl_data_shmid;

msg_t msg;
char *program_name;
char msgqid_str[9];

void usage(void)
{
  fprintf(stderr, "Usage: %s [-h] <input file>\n", program_name);
}


char *input_file;

char *framerate = NULL;
char *output_bufs = NULL;
char *file_offset = NULL;
char *videodecode_debug = NULL;

int ac3_audio_stream = 0;
int mpeg_audio_stream = 0;
int mpeg_video_stream = 0;
int subpicture_stream = 0;

int main(int argc, char *argv[])
{
  struct sigaction sig;
  pid_t demux_pid = -1;
  pid_t decode_pid = -1;
  pid_t display_pid = -1;
  int c;

  sig.sa_handler = int_handler;

  if(sigaction(SIGINT, &sig, NULL) == -1) {
    perror("sigaction");
  }
  
  
  program_name = argv[0];
  
  while((c = getopt(argc, argv, "a:v:s:m:f:r:o:d:h?")) != EOF) {
    switch(c) {
    case 'a':
      ac3_audio_stream = atoi(optarg);
      break;
    case 'm':
      mpeg_audio_stream = atoi(optarg);
      break;
    case 'v':
      mpeg_video_stream = atoi(optarg);
      break;
    case 's':
      subpicture_stream = atoi(optarg);
      break;
    case 'f':
      framerate = optarg;
      break;
    case 'r':
      output_bufs = optarg;
      break;
    case 'o':
      file_offset = optarg;
      break;
    case 'd':
      videodecode_debug = optarg;
      break;      
    case 'h':
    case '?':
      usage();
      return 1;
      break;
    }
  }
  
  if(argc - optind != 1){
    usage();
    return 1;
  }
  
  input_file = argv[optind];
  

  ctrl_data_shmid = create_ctrl_data();
  
  /* create msgq */

  create_msgq();
  

  sprintf(msgqid_str, "%d", msgqid);

  init_ctrl(msgqid_str);

  demux_pid = init_demux(msgqid_str);
  
  {
    cmd_t *cmd;
    
    msg.mtype = MTYPE_DEMUX;
    cmd = (cmd_t *)&msg.mtext;
    
    cmd->cmdtype = CMD_FILE_OPEN;
    strcpy(cmd->cmd.file_open.file, input_file);
    
    if(msgsnd(msgqid, &msg,
	      sizeof(cmdtype_t)+strlen(cmd->cmd.file_open.file)+1,
	      0) == -1) {
      perror("msgsnd");
    }
  }

  
  while(1){
    wait_for_msg(CMD_ALL);
    //  sleep(1);
  }
  
  
  return 0;
}

int chk_for_msg(void)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)(msg.mtext);
  
  if(msgrcv(msgqid, &msg, sizeof(msg.mtext), MTYPE_CTL, IPC_NOWAIT) == -1) {
    if(errno == ENOMSG) {
      fprintf(stderr ,"ctrl: no msg in q\n");
      return 0;
    }
    perror("msgrcv");
    return -1;
  } else {
    fprintf(stderr, "ctrl: got msg\n");
    eval_msg(cmd);
  }
  
  return 1;
}


int wait_for_msg(cmdtype_t cmdtype)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)(msg.mtext);
  cmd->cmdtype = CMD_NONE;
  
  while(cmd->cmdtype != cmdtype) {
    if(msgrcv(msgqid, &msg, sizeof(msg.mtext), MTYPE_CTL, 0) == -1) {
      perror("msgrcv");
      return -1;
    } else {
      fprintf(stderr, "ctrl: got msg\n");
      eval_msg(cmd);
    }
    if(cmdtype == CMD_ALL) {
      break;
    }
  }
  return 0;
}


int init_ctrl(char *msgqid_str)
{
  pid_t pid;
  int n;
  char *eargv[16];
  char *ctrl_name;
  char ctrl_path[] = "ui/ui";

  //TODO clean up filename handling
  
  if((ctrl_name = strrchr(ctrl_path, '/')+1) == NULL) {
    ctrl_name = ctrl_path;
  }
  if(ctrl_name > &ctrl_path[strlen(ctrl_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }

  /* fork/exec ctrl */

  switch(pid = fork()) {
  case 0:
    /* child process */
    
    n = 0;
    eargv[n++] = ctrl_name;
    eargv[n++] = "-m";
    eargv[n++] = msgqid_str;

    eargv[n++] = NULL;
    
    if(execv(ctrl_path, eargv) == -1) {
      perror("execv ctrl");
    }

    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    break;
  default:
    /* parent process */
    break;
  }
  return pid;
}

int init_demux(char *msgqid_str)
{
  pid_t pid;
  int n;
  char *eargv[16];
  char *demux_name;
  char demux_path[] = "mpeg2_program/programstream";

  //TODO clean up filename handling
  
  if((demux_name = strrchr(demux_path, '/')+1) == NULL) {
    demux_name = demux_path;
  }
  if(demux_name > &demux_path[strlen(demux_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }

  /* fork/exec demuxer */

  switch(pid = fork()) {
  case 0:
    /* child process */
    
    n = 0;
    eargv[n++] = demux_name;
    eargv[n++] = "-m";
    eargv[n++] = msgqid_str;

    if(file_offset != NULL) {
      eargv[n++] = "-o";
      eargv[n++] = file_offset;
    }
    
    eargv[n++] = NULL;
    
    if(execv(demux_path, eargv) == -1) {
      perror("execv demux");
    }

    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    break;
  default:
    /* parent process */
    break;
  }
  return pid;
}


int init_mpeg_video_decoder(char *msgqid_str)
{
  pid_t pid;
  char *eargv[16];
  char *decode_name;
  char decode_path[] = "mpeg2_video/video_stream";
  int n;
  //TODO clean up filename handling
  
  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }

  /* fork/exec decoder */

  switch(pid = fork()) {
  case 0:
    /* child process */
    n = 0;
    eargv[n++] = decode_name;
    eargv[n++] = "-m";
    eargv[n++] = msgqid_str;

    if(output_bufs != NULL) {
      eargv[n++] = "-r";
      eargv[n++] = output_bufs;
    }
    if(framerate != NULL) {
      eargv[n++] = "-f";
      eargv[n++] = framerate;
    }

    if(videodecode_debug != NULL) {
      eargv[n++] = "-d";
      eargv[n++] = videodecode_debug;
    }
    eargv[n++] = NULL;
    
    if(execv(decode_path, eargv) == -1) {
      perror("execv decode");
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    break;
  default:
    /* parent process */
    break;
  }
  return pid;

}

int init_dolby_ac3_decoder(char *msgqid_str)
{
  pid_t pid;
  char *decode_name;
  char decode_path[] = "ac3/ac3dec_wrap";

  //TODO clean up filename handling
  fprintf(stderr, "init_dolby_ac3_decoder\n");
  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }
  
  switch(pid = fork()) {
  case 0:
    /* child process */
    
    if(execl(decode_path, decode_name,
	     "-m", msgqid_str, NULL) == -1) {
      perror("execl");
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    
    break;
  default:
    /* parent process */
    break;
  }
 
  return pid;
  
}

int init_spu_decoder(char *msgqid_str)
{
  pid_t pid;
  char *decode_name;
  char decode_path[] = "subpicture/spu_wrap";

  //TODO clean up filename handling
  fprintf(stderr, "init_spu_decoder\n");
  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }
  
  switch(pid = fork()) {
  case 0:
    /* child process */
    
    if(execl(decode_path, decode_name,
	     "-m", msgqid_str, NULL) == -1) {
      perror("execl");
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    
    break;
  default:
    /* parent process */
    break;
  }
 
  return pid;
  
}

int init_mpeg_private_stream_2_decoder(char *msgqid_str)
{
  pid_t pid;
  char *decode_name;
  char decode_path[] = "vmg/vmg";

  //TODO clean up filename handling
  fprintf(stderr, "init_mpeg_private_stream_2_decoder\n");
  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }
  
  switch(pid = fork()) {
  case 0:
    /* child process */
    
    if(execl(decode_path, decode_name,
	     "-m", msgqid_str, NULL) == -1) {
      perror("execl");
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    
    break;
  default:
    /* parent process */
    break;
  }
 
  return pid;
  
}

int init_mpeg_audio_decoder(char *msgqid)
{
  pid_t pid;
  char *decode_name;
  char decode_path[] = "mpeg_audio/mpg123_wrap";

  //TODO clean up filename handling
  fprintf(stderr, "init_mpeg_audio_decoder\n");
  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    fprintf(stderr, "illegal file name?\n");
    return -1;
  }
  
  switch(pid = fork()) {
  case 0:
    /* child process */
    
    if(execl(decode_path, decode_name,
	     "-m", msgqid_str, NULL) == -1) {
      perror("execl");
    }
    exit(-1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    
    break;
  default:
    /* parent process */
    break;
  }
 
  return pid;
  
}






int register_stream(uint8_t stream_id, uint8_t subtype)
{
  

  if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    if(subtype == (0x80 | (ac3_audio_stream & 0x1f))) {
      return 1;
    }
    
    if(subtype == (0x20 | (subpicture_stream & 0x1f))) {
      return 1;
    }
  }
  
  if(stream_id == (0xc0 | (mpeg_audio_stream & 0x1f))) { 
    return 1;
  }
  
  if(stream_id == (0xe0 | (mpeg_video_stream & 0x0f))) {
    return 1;
  }
  
  if(stream_id == 0xbf) { // nav packs
    return 1;
  }
  
  return 0;  
  
}


int create_msgq()
{
  
  if((msgqid = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
    perror("msgget msgqid");
    exit(-1);
  }

  return 0;
}  


void destroy_msgq()
{
  if(msgctl(msgqid, IPC_RMID, NULL) == -1) {
    perror("msgctl ipc_rmid");
  }
  msgqid = -1;

}



int eval_msg(cmd_t *cmd)
{
  msg_t sendmsg;
  cmd_t *sendcmd;
  
  sendcmd = (cmd_t *)&sendmsg.mtext;
  
  switch(cmd->cmdtype) {
  case CMD_DEMUX_REQ_BUFFER:
    {
      shm_bufinfo_t bufinfo;

      fprintf(stderr, "ctrl: got request for buffer size %d\n",
	      cmd->cmd.req_buffer.size);
      get_buffer(cmd->cmd.req_buffer.size, &bufinfo);
      
      sendmsg.mtype = MTYPE_DEMUX;
      sendcmd->cmdtype = CMD_DEMUX_GNT_BUFFER;
      sendcmd->cmd.gnt_buffer.shmid = bufinfo.shmid;
      sendcmd->cmd.gnt_buffer.size = bufinfo.size;
      send_msg(&sendmsg, sizeof(cmdtype_t)+sizeof(cmd_gnt_buffer_t));
      
    }
    break;
  case CMD_DEMUX_NEW_STREAM:
    {
      int shmid;
      fprintf(stderr, "ctrl: new stream %x, %x\n",
	      cmd->cmd.new_stream.stream_id,
	      cmd->cmd.new_stream.subtype);
      //      if(!((cmd->cmd.new_stream.stream_id == MPEG2_PRIVATE_STREAM_1) &&
      //	   !(cmd->cmd.new_stream.subtype == 0x80))) {
      
      if(register_stream(cmd->cmd.new_stream.stream_id,
			 cmd->cmd.new_stream.subtype)) {
	
	fprintf(stderr, "NR: %d\n",cmd->cmd.new_stream.nr_of_elems);
	shmid = create_q(cmd->cmd.new_stream.nr_of_elems,
			 cmd->cmd.new_stream.data_buf_shmid);
	
      } else {
	shmid = -1;
      }
      
      /*
      if((cmd->cmd.new_stream.stream_id == MPEG2_PRIVATE_STREAM_1) &&
	 !(cmd->cmd.new_stream.subtype == 0x80)) {
	shmid = -1;
      } else {
	
	fprintf(stderr, "NR: %d\n",cmd->cmd.new_stream.nr_of_elems);
	shmid = create_q(cmd->cmd.new_stream.nr_of_elems,
			 cmd->cmd.new_stream.data_buf_shmid);
      }
      */



      sendmsg.mtype = MTYPE_DEMUX;
      sendcmd->cmdtype = CMD_DEMUX_STREAM_BUFFER;

      sendcmd->cmd.stream_buffer.q_shmid = shmid;
      fprintf(stderr, "shmid: %d\n", shmid);
      sendcmd->cmd.stream_buffer.stream_id =
	cmd->cmd.new_stream.stream_id; 
      sendcmd->cmd.stream_buffer.subtype =
	cmd->cmd.new_stream.subtype; 
      fprintf(stderr, "send_msg\n");
      send_msg(&sendmsg, sizeof(cmdtype_t)+sizeof(cmd_stream_buffer_t));

      if(shmid >= 0) {
	// lets start the decoder
	if((cmd->cmd.new_stream.stream_id) == MPEG2_PRIVATE_STREAM_1) {
	  if((cmd->cmd.new_stream.subtype >= 0x80) &&
	     (cmd->cmd.new_stream.subtype < 0x90)) {
	    
	    init_dolby_ac3_decoder(msgqid_str);
	    

	    // send ctrl shm
	    
	    sendmsg.mtype = MTYPE_AUDIO_DECODE_AC3;
	    sendcmd->cmdtype = CMD_CTRL_DATA;
	    sendcmd->cmd.ctrl_data.shmid = ctrl_data_shmid;

	    send_msg(&sendmsg, sizeof(cmdtype_t) +
		     sizeof(cmd_ctrl_data_t));
	    

	    // temporary fix to mmap infile

	    sendmsg.mtype = MTYPE_AUDIO_DECODE_AC3;
	    sendcmd->cmdtype = CMD_FILE_OPEN;

	    strcpy(sendcmd->cmd.file_open.file, input_file);
	    
	    
	    send_msg(&sendmsg, sizeof(cmdtype_t) +
		     strlen(sendcmd->cmd.file_open.file)+1);
	    

	    // ac3 stream
	    sendmsg.mtype = MTYPE_AUDIO_DECODE_AC3;
	    sendcmd->cmdtype = CMD_DECODE_STREAM_BUFFER;
	    sendcmd->cmd.stream_buffer.q_shmid = shmid;
	    sendcmd->cmd.stream_buffer.stream_id =
	      cmd->cmd.new_stream.stream_id; 
	    sendcmd->cmd.stream_buffer.subtype =
	      cmd->cmd.new_stream.subtype; 
	    
	    send_msg(&sendmsg, sizeof(cmdtype_t)+sizeof(cmd_stream_buffer_t));
	    
	  } else if((cmd->cmd.new_stream.subtype >= 0x20) &&
		    (cmd->cmd.new_stream.subtype < 0x40)) {
	    
	    init_spu_decoder(msgqid_str);
	    

	    // send ctrl shm
	    
	    sendmsg.mtype = MTYPE_SPU_DECODE;
	    sendcmd->cmdtype = CMD_CTRL_DATA;
	    sendcmd->cmd.ctrl_data.shmid = ctrl_data_shmid;

	    send_msg(&sendmsg, sizeof(cmdtype_t) +
		     sizeof(cmd_ctrl_data_t));
	    

	    // temporary fix to mmap infile

	    sendmsg.mtype = MTYPE_SPU_DECODE;
	    sendcmd->cmdtype = CMD_FILE_OPEN;

	    strcpy(sendcmd->cmd.file_open.file, input_file);
	    
	    
	    send_msg(&sendmsg, sizeof(cmdtype_t) +
		     strlen(sendcmd->cmd.file_open.file)+1);
	    

	    // ac3 stream
	    sendmsg.mtype = MTYPE_SPU_DECODE;
	    sendcmd->cmdtype = CMD_DECODE_STREAM_BUFFER;
	    sendcmd->cmd.stream_buffer.q_shmid = shmid;
	    sendcmd->cmd.stream_buffer.stream_id =
	      cmd->cmd.new_stream.stream_id; 
	    sendcmd->cmd.stream_buffer.subtype =
	      cmd->cmd.new_stream.subtype; 
	    
	    send_msg(&sendmsg, sizeof(cmdtype_t)+sizeof(cmd_stream_buffer_t));

	  }
	  
	} else if((cmd->cmd.new_stream.stream_id >= 0xc0) &&
		  (cmd->cmd.new_stream.stream_id < 0xe0)) {
	  init_mpeg_audio_decoder(msgqid_str);
	  // mpeg audio stream


	  // send ctrl shm
	  
	  sendmsg.mtype = MTYPE_AUDIO_DECODE_MPEG;
	  sendcmd->cmdtype = CMD_CTRL_DATA;
	  sendcmd->cmd.ctrl_data.shmid = ctrl_data_shmid;

	  send_msg(&sendmsg, sizeof(cmdtype_t) + sizeof(cmd_ctrl_data_t));
	  
	  
	  // temporary fix to mmap infile
	  
	  sendmsg.mtype = MTYPE_AUDIO_DECODE_MPEG;
	  sendcmd->cmdtype = CMD_FILE_OPEN;
	  
	  strcpy(sendcmd->cmd.file_open.file, input_file);
	  
	  
	  send_msg(&sendmsg, sizeof(cmdtype_t) +
		   strlen(sendcmd->cmd.file_open.file)+1);
	    

	  // mpeg audio stream
	  sendmsg.mtype = MTYPE_AUDIO_DECODE_MPEG;
	  sendcmd->cmdtype = CMD_DECODE_STREAM_BUFFER;
	  sendcmd->cmd.stream_buffer.q_shmid = shmid;
	  sendcmd->cmd.stream_buffer.stream_id =
            cmd->cmd.new_stream.stream_id; 
          sendcmd->cmd.stream_buffer.subtype =
            cmd->cmd.new_stream.subtype; 
	  
	  send_msg(&sendmsg, sizeof(cmdtype_t)+sizeof(cmd_stream_buffer_t));
	  
	  
	} else if((cmd->cmd.new_stream.stream_id >= 0xe0) &&
		  (cmd->cmd.new_stream.stream_id < 0xf0)) {
	  init_mpeg_video_decoder(msgqid_str);


	  // send ctrl shm
	  
	  sendmsg.mtype = MTYPE_VIDEO_DECODE_MPEG;
	  sendcmd->cmdtype = CMD_CTRL_DATA;
	  sendcmd->cmd.ctrl_data.shmid = ctrl_data_shmid;
	  
	  send_msg(&sendmsg, sizeof(cmdtype_t) +
		   sizeof(cmd_ctrl_data_t));
	  
	  // mpeg video stream
	  
	  sendmsg.mtype = MTYPE_VIDEO_DECODE_MPEG;
	  sendcmd->cmdtype = CMD_FILE_OPEN;
	  strcpy(sendcmd->cmd.file_open.file, input_file);

	  send_msg(&sendmsg,
		   sizeof(cmdtype_t)+strlen(sendcmd->cmd.file_open.file)+1);
	  
	  
	  sendmsg.mtype = MTYPE_VIDEO_DECODE_MPEG;
	  sendcmd->cmdtype = CMD_DECODE_STREAM_BUFFER;
	  sendcmd->cmd.stream_buffer.q_shmid = shmid;
	  sendcmd->cmd.stream_buffer.stream_id =
	    cmd->cmd.new_stream.stream_id; 
	  sendcmd->cmd.stream_buffer.subtype =
	    cmd->cmd.new_stream.subtype; 
	  
	  send_msg(&sendmsg, sizeof(cmdtype_t)+sizeof(cmd_stream_buffer_t));
	  
	} else if(cmd->cmd.new_stream.stream_id == 0xbf) {
	  
	  init_mpeg_private_stream_2_decoder(msgqid_str);


	  // send ctrl shm
	  
	  sendmsg.mtype = MTYPE_DECODE_MPEG_PRIVATE_STREAM_2;
	  sendcmd->cmdtype = CMD_CTRL_DATA;
	  sendcmd->cmd.ctrl_data.shmid = ctrl_data_shmid;
	  
	  send_msg(&sendmsg, sizeof(cmdtype_t) +
		   sizeof(cmd_ctrl_data_t));
	  
	  // mpeg private stream 2 
	  
	  sendmsg.mtype = MTYPE_DECODE_MPEG_PRIVATE_STREAM_2;
	  sendcmd->cmdtype = CMD_FILE_OPEN;
	  strcpy(sendcmd->cmd.file_open.file, input_file);

	  send_msg(&sendmsg,
		   sizeof(cmdtype_t)+strlen(sendcmd->cmd.file_open.file)+1);
	  
	  
	  sendmsg.mtype = MTYPE_DECODE_MPEG_PRIVATE_STREAM_2;
	  sendcmd->cmdtype = CMD_DECODE_STREAM_BUFFER;
	  sendcmd->cmd.stream_buffer.q_shmid = shmid;
	  sendcmd->cmd.stream_buffer.stream_id =
	    cmd->cmd.new_stream.stream_id; 
	  sendcmd->cmd.stream_buffer.subtype =
	    cmd->cmd.new_stream.subtype; 
	  
	  send_msg(&sendmsg, sizeof(cmdtype_t)+sizeof(cmd_stream_buffer_t));
	  
	}
	
      }
    }
    break;
  case CMD_CTRL_CMD:
    sendmsg.mtype = MTYPE_VIDEO_DECODE_MPEG;
    sendcmd->cmdtype = cmd->cmdtype;
    sendcmd->cmd.ctrl_cmd.ctrlcmd = cmd->cmd.ctrl_cmd.ctrlcmd;
    
    send_msg(&sendmsg, sizeof(cmdtype_t)+sizeof(cmd_ctrl_cmd_t));
    
    break;
  default:
    fprintf(stderr, "ctrl: unrecognized command cmdtype: %x\n",
	    cmd->cmdtype);
    return -1;
    break;
  }
  
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

int get_buffer(int size, shm_bufinfo_t *bufinfo)
{
  /* get shm buffer */

  bufinfo->shmid = 3;
  bufinfo->size = size;
  
  return 0;
}

  
int create_q(int nr_of_elems, int buf_shmid)
{
  
  int shmid;
  char *shmaddr;
  q_head_t *q_head;
  
  fprintf(stderr, "create_q\n");
  fprintf(stderr, "shmget\n");
  if((shmid = shmget(IPC_PRIVATE,
		     sizeof(q_head_t) + nr_of_elems*sizeof(q_elem_t),
		     IPC_CREAT | 0600)) == -1) {
    perror("create_q(), shmget()");
    return -1;
  }
  
  
  fprintf(stderr, "shmat\n");
  if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
    perror("create_q(), shmat()");
    
    if(shmctl(shmid, IPC_RMID, NULL) == -1) {
      perror("ipc_rmid");
    }
    return -1;
  }

  add_q_shmid(shmid);
  
  q_head = (q_head_t *)shmaddr;
  
  fprintf(stderr, "sem_init\n");
  if(sem_init(&q_head->bufs_full, 1, 0) == -1) {
    perror("create_q(), sem_init(bufs_full)");
    return -1;
  }

  fprintf(stderr, "sem_init\n");
  if(sem_init(&q_head->bufs_empty, 1, nr_of_elems) == -1) {
    perror("create_q(), sem_init(bufs_empty)");
    return -1;
  }
  
  q_head->buf_shmid = buf_shmid;
  
  q_head->nr_of_qelems = nr_of_elems;
  
  q_head->write_nr = 0;
  
  q_head->read_nr = 0;

  if(shmdt(shmaddr) == -1) {
    perror("create_q(), shmdt()");
  }
  return shmid;
}




int create_ctrl_data()
{
  int shmid;
  char *shmaddr;
  ctrl_data_t *ctrl;
  ctrl_time_t *ctrl_time;
  int n;
  int nr_of_offsets = 16;
  
  if((shmid = shmget(IPC_PRIVATE,
		     sizeof(ctrl_data_t)+
		     nr_of_offsets*sizeof(ctrl_time_t),
		     IPC_CREAT | 0600)) == -1) {
    perror("create_ctrl_data(), shmget()");
    return -1;
  }
  
  if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
    perror("create_ctrl_data(), shmat()");

    if(shmctl(shmid, IPC_RMID, NULL) == -1) {
      perror("ipc_rmid");
    }

    return -1;
  }

  add_q_shmid(shmid);
  
  ctrl = (ctrl_data_t *)shmaddr;
  ctrl->mode = MODE_STOP;
  ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  
  for(n = 0; n < nr_of_offsets; n++) {
    ctrl_time[n].offset_valid = OFFSET_NOT_VALID;
  }

  return shmid;

}


int *shmids = NULL;
int nr_shmids = 0;

void add_q_shmid(int shmid)
{
  nr_shmids++;
  
  if((shmids = (int *)realloc(shmids, sizeof(int)*nr_shmids)) == NULL) {
    perror("realloc");
    nr_shmids--;
    return;
  }

  shmids[nr_shmids-1] = shmid;
  
}


void remove_q_shm()
{
  int n;

  for(n = 0; n < nr_shmids; n++) {
    fprintf(stderr, "removing shmid: %d\n", shmids[n]);
    if(shmctl(shmids[n], IPC_RMID, NULL) == -1) {
      perror("ipc_rmid");
    }
  }
  nr_shmids = 0;
  free(shmids);
  shmids = NULL;
  
}

void int_handler()
{
  /* send quit msg to demuxer and decoders
   * (and wait for ack? (timeout))
   * 
   * remove all shared memory segments
   *
   * remove all msqqueues
   *
   * exit
   */
  
  
  remove_q_shm();
  destroy_msgq();
  exit(0);

}
