#include <unistd.h>
#include <stdio.h>
#include <sys/msg.h>

#include "msgtypes.h"
#include "dvdcontrol.h"



static int send_msg(int msgqid, mq_msg_t *msg, int mtext_size)
{
  if(msgsnd(msgqid, msg, mtext_size, 0) == -1) {
    perror("dvd: msgsnd1");
    return -1;
  }
  return 0;
}


static int send_cmd(int msgqid, mq_msg_t *msg)
{
  int msize;
  mq_cmd_t *cmd;
  
  cmd = (mq_cmd_t *)&msg->mtext;
  
  msg->mtype = MTYPE_DECODE_MPEG_PRIVATE_STREAM_2;  
  
  switch(cmd->cmdtype) {
  case CMD_DVDCTRL_CMD:
    msize = sizeof(mq_cmdtype_t)+sizeof(mq_cmd_dvdctrl_cmd_t);
    break;
  default:
    msize = 0;
  }
  
  send_msg(msgqid, msg, msize);
  
  return 0;
}

DVDResult_t DVDLeftButtonSelect(int msgqid)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_LEFT_BUTTON;
  
  send_cmd(msgqid, &msg);
  
  return DVD_E_OK;
}

DVDResult_t DVDRightButtonSelect(int msgqid)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_RIGHT_BUTTON;
  
  send_cmd(msgqid, &msg);

  return DVD_E_OK;
}

DVDResult_t DVDUpperButtonSelect(int msgqid)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_UP_BUTTON;
  
  send_cmd(msgqid, &msg);

  return DVD_E_OK;
}

DVDResult_t DVDLowerButtonSelect(int msgqid)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_DOWN_BUTTON;
  
  send_cmd(msgqid, &msg);

  return DVD_E_OK;
}

DVDResult_t DVDButtonActivate(int msgqid)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_ACTIVATE_BUTTON;	
  
  send_cmd(msgqid, &msg);

  return DVD_E_OK;
}

DVDResult_t DVDButtonSelect(int msgqid, int Button)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_SELECT_BUTTON_NR;	
  cmd->cmd.dvdctrl_cmd.button_nr = Button;

  send_cmd(msgqid, &msg);

  return DVD_E_OK;
}

DVDResult_t DVDButtonSelectAndActivate(int msgqid, int Button)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_SELECT_ACTIVATE_BUTTON_NR;	
  cmd->cmd.dvdctrl_cmd.button_nr = Button;

  send_cmd(msgqid, &msg);

  return DVD_E_OK;
}

DVDResult_t DVDMouseSelect(int msgqid, int x, int y)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_CHECK_MOUSE_SELECT;
  cmd->cmd.dvdctrl_cmd.mouse_x = (unsigned int) x;
  cmd->cmd.dvdctrl_cmd.mouse_y = (unsigned int) y;
  
  send_cmd(msgqid, &msg);

  return DVD_E_OK;
}

DVDResult_t DVDMouseActivate(int msgqid, int x, int y)
{
  mq_msg_t msg;
  mq_cmd_t *cmd;
  cmd = (mq_cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_CHECK_MOUSE_ACTIVATE;
  cmd->cmd.dvdctrl_cmd.mouse_x = (unsigned int) x;
  cmd->cmd.dvdctrl_cmd.mouse_y = (unsigned int) y;

  send_cmd(msgqid, &msg);

  return DVD_E_OK;
}

DVDResult_t DVDMenuCall(int msgqid, DVDMenuID_t MenuId)
{
  return DVD_E_NOT_IMPLEMENTED;
}

DVDResult_t DVDResume(int msgqid)
{
  return DVD_E_NOT_IMPLEMENTED;
}

DVDResult_t DVDGoUP(int msgqid)
{
  return DVD_E_NOT_IMPLEMENTED;
}

DVDResult_t DVDDefaultMenuLanguageSelect(int msgqid, DVDLangID_t Lang)
{
  return DVD_E_NOT_IMPLEMENTED;
}


const static char DVD_E_OK_STR[] = "OK";
const static char DVD_E_NOT_IMPLEMENTED_STR[] = "Not Implemented";
const static char DVD_E_NO_SUCH_ERROR_STR[] = "No such error code";

void DVDPerror(const char *str, DVDResult_t ErrCode)
{
  const char *errstr;

  switch(ErrCode) {
  case DVD_E_OK:
    errstr = DVD_E_OK_STR;
    break;
  case DVD_E_NOT_IMPLEMENTED:
    errstr = DVD_E_NOT_IMPLEMENTED_STR;
    break;
  default:
    errstr = DVD_E_NO_SUCH_ERROR_STR;
    break;
  }

  fprintf(stderr, "%s%s %s\n",
	  (str == NULL ? "" : str),
	  (str == NULL ? "" : ":"),
	  errstr);

}
