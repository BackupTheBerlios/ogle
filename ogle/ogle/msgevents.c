#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <errno.h>
#include <ogle/msgevents.h>



//#define DEBUG
#ifdef DEBUG

static char *MsgEventType_str[] = {
  "MsgEventQNone",
  "Reserved",
  "MsgEventQInitReq",
  "MsgEventQInitGnt",  
  "MsgEventQRegister",
  "MsgEventQNotify",
  "MsgEventQReqCapability",
  "MsgEventQGntCapability",
  "MsgEventQPlayCtrl",
  "MsgEventQChangeFile",
  "MsgEventQReqStreamBuf",
  "MsgEventQGntStreamBuf",
  "MsgEventQDecodeStreamBuf",
  "MsgEventQReqBuf",
  "MsgEventQGntBuf",
  "MsgEventQCtrlData",
  "MsgEventQReqPicBuf",
  "MsgEventQGntPicBuf",
  "MsgEventQAttachQ",
  "MsgEventQSPUPalette",
  "MsgEventQSPUHighlight",
  "MsgEventQSpeed",
  "MsgEventQDVDCtrl",
  "MsgEventQFlow",
  "MsgEventQFlushData",
  "MsgEventQDemuxStream",
  "MsgEventQDemuxStreamChange",
  NULL
};

void PrintMsgEventType(MsgEventType_t type)
{
  fprintf(stderr, MsgEventType_str[type]);
}
#endif

MsgEventQ_t *MsgOpen(int msqid)
{
  MsgEventQ_t *ret = NULL;
  msg_t msg;
  MsgQInitReqEvent_t initreq;
  MsgQInitGntEvent_t initgnt;
  
  /* TODO: if we are using one message queue per process, create
   *  one here.
   */
  
  msg.mtype = CLIENT_RESOURCE_MANAGER; // the recipient of this message
  initreq.type = MsgEventQInitReq;   // we want a handle
  memcpy(msg.event_data, &initreq, sizeof(MsgQInitReqEvent_t));
  
  if(msgsnd(msqid, (void *)&msg, sizeof(MsgQInitReqEvent_t), 0) == -1) {
    perror("MsgOpen, snd");
    return NULL;
    
  } else {
    if(msgrcv(msqid, (void *)&msg, sizeof(MsgEvent_t),
	      CLIENT_UNINITIALIZED, 0) == -1) {
      perror("MsgOpen, rcv");
      return NULL;
    } else {
      ret = (MsgEventQ_t *)malloc(sizeof(MsgEventQ_t));
      
      ret->msqid = msqid;       // which msq to wait for messages on
      memcpy(&initgnt, msg.event_data, sizeof(MsgQInitGntEvent_t));
      ret->mtype = initgnt.newclientid; // mtype to wait for
    
    }
  }

  return ret;
  
}

/**
 * Close the message connection.
 * TODO: tell resource manager that there isn't anyone listening
 * for messages here any more. 
 */
void MsgClose(MsgEventQ_t *q)
{

  fprintf(stderr, "msg close FIX\n");
  
  
  // just in case someone tries to access this pointer after close
  q->msqid = 0;
  q->mtype = 0;
  
  free(q);
  
}

int MsgNextEvent(MsgEventQ_t *q, MsgEvent_t *event_return)
{
  msg_t msg;
  
  if(msgrcv(q->msqid, (void *)&msg, sizeof(MsgEvent_t),
	    q->mtype, 0) == -1) {
    perror("MsgNextEvent");
      return -1;
  } else {
    
    memcpy(event_return, msg.event_data, sizeof(MsgEvent_t));
    return 0;
  }
  
}

int MsgCheckEvent(MsgEventQ_t *q, MsgEvent_t *event_return)
{
  msg_t msg;
  
  if(msgrcv(q->msqid, (void *)&msg, sizeof(MsgEvent_t),
	    q->mtype, IPC_NOWAIT) == -1) {
    if(errno != ENOMSG) {
      perror("MsgNextEvent");
    }
    return -1;
  } else {
    
    memcpy(event_return, msg.event_data, sizeof(MsgEvent_t));
    return 0;
  }

}

int MsgSendEvent(MsgEventQ_t *q, MsgEventClient_t client,
		 MsgEvent_t *event_send)
{
  msg_t msg;
  int size = 0;
  msg.mtype = client; // the recipient of this message
  event_send->any.client = q->mtype; // the sender  

  switch(event_send->type) {
  case MsgEventQInitReq:
    size = sizeof(MsgQInitReqEvent_t);
    break;
  case MsgEventQInitGnt:  
    size = sizeof(MsgQInitGntEvent_t);
    break;
  case MsgEventQRegister:
    size = sizeof(MsgQRegisterCapsEvent_t);
    break;
  case MsgEventQNotify:
    size = sizeof(MsgQNotifyEvent_t);
    break;
  case MsgEventQReqCapability:
    size = sizeof(MsgQReqCapabilityEvent_t);
    break;
  case MsgEventQGntCapability:
    size = sizeof(MsgQGntCapabilityEvent_t);
    break;
  case MsgEventQPlayCtrl:
    size = sizeof(MsgQPlayCtrlEvent_t);
    break;
  case MsgEventQChangeFile:
    size = sizeof(MsgQAnyEvent_t)+strlen(event_send->changefile.filename)+1;
    break;
  case MsgEventQReqStreamBuf:
    size = sizeof(MsgQReqStreamBufEvent_t);
    break;
  case MsgEventQGntStreamBuf:
    size = sizeof(MsgQGntStreamBufEvent_t);
    break;
  case MsgEventQDecodeStreamBuf:
    size = sizeof(MsgQDecodeStreamBufEvent_t);
    break;
  case MsgEventQReqBuf:
    size = sizeof(MsgQReqBufEvent_t);
    break;
  case MsgEventQGntBuf:
    size = sizeof(MsgQGntBufEvent_t);
    break;
  case MsgEventQCtrlData:
    size = sizeof(MsgQCtrlDataEvent_t);
    break;
  case MsgEventQReqPicBuf:
    size = sizeof(MsgQReqPicBufEvent_t);
    break;
  case MsgEventQGntPicBuf:
    size = sizeof(MsgQGntPicBufEvent_t);
    break;
  case MsgEventQAttachQ:
    size = sizeof(MsgQAttachQEvent_t);
    break;
  case MsgEventQSPUPalette:
    size = sizeof(MsgQSPUPaletteEvent_t);
    break;
  case MsgEventQSPUHighlight:
    size = sizeof(MsgQSPUHighlightEvent_t);
    break;
  case MsgEventQSpeed:
    size = sizeof(MsgQSpeedEvent_t);
    break;
  case MsgEventQDVDCtrl:
    size = sizeof(MsgQDVDCtrlEvent_t);
    break;
  case MsgEventQFlow:
    size = sizeof(MsgQFlowEvent_t);
    break;
  case MsgEventQFlushData:
    size = sizeof(MsgQFlushDataEvent_t);
    break;
  case MsgEventQDemuxStream:
    size = sizeof(MsgQDemuxStreamEvent_t);
    break;
  case MsgEventQDemuxStreamChange:
    size = sizeof(MsgQDemuxStreamChangeEvent_t);
    break;
  default:
    fprintf(stderr, "MsgSendEvent: Unknown event: %d\n", event_send->type);
    return -1;
  }

  memcpy(msg.event_data, event_send, size);

#ifdef DEBUG
  fprintf(stderr, "Sending '%s' from: %ld to: %ld\n",
	  MsgEventType_str[msg.event.type],
	  msg.event.any.client,
	  msg.mtype);
#endif
  
  if(msgsnd(q->msqid, (void *)&msg, size, 0) == -1) {
    perror("MsgSendEvent");
    return -1;
  }
  
  return 0;
}


  
/*
  
  1 semaphore for each msg queue receiver
  
  one msgarray for each receiver


  msgsnd(msgqid, msg, size, nowait/wait) {
  
  semwait(msgqsem[msgqid]);
  qnr = qnrs[msgqid];
  qnrs[msgqid]++;
  sempost(msgqsem[msgqid]);

  fill 



  }
  
  if(msgsnd(q->msqid, (void *)&msg, size, 0) == -1) {
    perror("MsgSendEvent");
    return -1;
  }



 */
