

#include "dvdcontrol.h"




int send_msg(msg_t *msg, int mtext_size)
{
  if(msgsnd(msgqid, msg, mtext_size, 0) == -1) {
    perror("dvd: msgsnd1");
    return -1;
  }
  return 0;
}




int send_cmd(msg_t *msg)
{
  int msize;
  cmd_t *cmd;
  
  cmd = (cmd_t *)&msg->mtext;
  
  msg->mtype = MTYPE_DECODE_MPEG_PRIVATE_STREAM_2;  
  
  switch(cmd->cmdtype) {
  case CMD_DVDCTRL_CMD:
    msize = sizeof(cmdtype_t)+sizeof(cmd_dvdctrl_cmd_t);
    break;
  default:
    msize = 0;
  }
  
  send_msg(msg, msize);
  
  return 0;
}

DVDResult_t DVDLeftButtonSelect(void)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_LEFT_BUTTON;
  
  send_cmd(&msg);
  
  return DVD_E_OK;
}

DVDResult_t DVDRightButtonSelect(void)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_RIGHT_BUTTON;
  
  send_cmd(&msg);

  return DVD_E_OK;
}

DVDResult_t DVDUpperButtonSelect(void)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_UP_BUTTON;
  
  send_cmd(&msg);

  return DVD_E_OK;
}

DVDResult_t DVDLowerButtonSelect(void)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_DOWN_BUTTON;
  
  send_cmd(&msg);

  return DVD_E_OK;
}

DVDResult_t DVDButtonActivate(void)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_ACTIVATE_BUTTON;	
  
  send_cmd(&msg);

  return DVD_E_OK;
}

DVDResult_t DVDButtonSelect(int button)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_SELECT_BUTTON_NR;	
  cmd->cmd.dvdctrl_cmd.button_nr = button;

  send_cmd(&msg);

  return DVD_E_OK;
}

DVDResult_t DVDButtonSelectAndActivate(int button)
{
  msg_t msg;
  cmd_t *cmd;
  cmd = (cmd_t *)&msg.mtext;
  
  cmd->cmdtype = CMD_DVDCTRL_CMD;
  cmd->cmd.dvdctrl_cmd.cmd = DVDCTRL_CMD_SELECT_ACTIVATE_BUTTON_NR;	
  cmd->cmd.dvdctrl_cmd.button_nr = button;

  send_cmd(&msg);

  return DVD_E_OK;
}

DVDResult_t DVDMouseSelect(int x, int y)
{
  return DVD_E_NOT_IMPLEMENTED;
}

DVDResult_t DVDMouseActivate(int x, int y)
{
  return DVD_E_NOT_IMPLEMENTED;
}

DVDResult_t DVDMenuCall(DVDMenuID_t menuid)
{
  return DVD_E_NOT_IMPLEMENTED;
}

DVDResult_t DVDResume(void)
{
  return DVD_E_NOT_IMPLEMENTED;
}

DVDResult_t DVDGoUP(void)
{
  return DVD_E_NOT_IMPLEMENTED;
}

DVDResult_t DVDDefaultMenuLanguageSelect(DVDLangID_t lang)
{
  return DVD_E_NOT_IMPLEMENTED;
}


const static char DVD_E_OK_STR = "OK";
const static char DVD_E_NOT_IMPLEMENTED_STR = "Not Implemented";
const static char DVD_E_NO_SUCH_ERROR_STR = "No such error code";

void DVDPerror(const char *str, DVDResult_t errcode)
{
  char *errstr;

  switch(errcode) {
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
