

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
