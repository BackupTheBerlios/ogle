/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#include <errno.h>

#include "dvd.h"
#include "dvdevents.h"
#include "msgevents.h"

#ifdef HAVE_SYSV_MSG
#include <sys/ipc.h>
#include <sys/msg.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#include <sys/poll.h>
#endif

#if (defined(BSD) && (BSD >= 199306))
#include <unistd.h>
#endif

//#define DEBUG
//#define XDEBUG

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
  "MsgEventQReqStreamBuf", // 10
  "MsgEventQGntStreamBuf",
  "MsgEventQDecodeStreamBuf",
  "MsgEventQReqBuf",
  "MsgEventQGntBuf",
  "MsgEventQCtrlData",
  "MsgEventQReqPicQ",
  "MsgEventQGntPicQ",
  "MsgEventQAttachQ",
  "MsgEventQSPUPalette",
  "MsgEventQSPUHighlight", // 20
  "MsgEventQSpeed",
  "MsgEventQDVDCtrl",
  "MsgEventQFlow",
  "MsgEventQFlushData",
  "MsgEventQDemuxStream",
  "MsgEventQDemuxStreamChange",
  "MsgEventQDemuxDefault",
  "MsgEventQDVDCtrlLong",
  "MsgEventQDemuxDVD", // 29
  "MsgEventQDemuxDVDRoot",
  "MsgEventQSetAspectModeSrc",
  "MsgEventQSetSrcAspect",
  "MsgEventQSetZoom",
  "MsgEventQReqInput",
  "MsgEventQInputButtonPress",
  "MsgEventQInputButtonRelease",
  "MsgEventQInputPointerMotion",
  "MsgEventQInputKeyPress",
  "MsgEventQInputKeyRelease",
  "MsgEventQDestroyBuf",
  "MsgEventQAppendQ",
  "MsgEventQDetachQ",
  "MsgEventQQDetached",
  "MsgEventQDestroyQ",
  "MsgEventQDemuxStreamChange2",
  "MsgEventQSaveScreenshot",
  "MsgEventQReqPicBuf",
  "MsgEventQGntPicBuf",
  "MsgEventQDestroyBuf",
  NULL
};

void PrintMsgEventType(MsgEventType_t type)
{
  fprintf(stderr, MsgEventType_str[type]);
}
#endif

#ifdef SOCKIPC
MsgEventQ_t *MsgOpen(MsgEventQType_t type, char *name, int namelen)
{
  MsgEventQ_t *ret = NULL;
  msg_t msg;
  MsgQInitReqEvent_t initreq;
  MsgQInitGntEvent_t initgnt;
  int r;

  switch(type) {
#ifdef HAVE_SYSV_MSG
  case MsgEventQType_msgq:
    {
      int msqid = atoi(name);

      msg.msgq.mtype = CLIENT_RESOURCE_MANAGER; //the recipient of this message
      initreq.type = MsgEventQInitReq;   // we want a handle
      memcpy(msg.msgq.event_data, &initreq, sizeof(MsgQInitReqEvent_t));
  
      
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
	  
	  ret->msgq.type = MsgEventQType_msgq;
	  ret->msgq.msqid = msqid;       // which msq to wait for messages on
	  memcpy(&initgnt, msg.msgq.event_data, sizeof(MsgQInitGntEvent_t));
	  ret->msgq.mtype = initgnt.newclientid; // mtype to wait for
	  
	}
      }
    }
    break;
#endif
  case MsgEventQType_socket:
    {
      int sd;
      struct sockaddr_un unix_addr = { 0 };
      struct sockaddr_un tmp_addr = { 0 };
      struct sockaddr_un from_addr = { 0 };
      int from_addr_len;
      int n;
      static unsigned int ser = 0;
      long int tmp_clientid;
      
      if((sd = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1) {
	perror("socket");
	return NULL;
      }

      tmp_addr.sun_family = AF_UNIX;

      //try 10 times then fail
      for(n=0; n<10; n++) {
	//create a name for the socket
	tmp_clientid = getpid()*10+ser;
	ser = (ser+1)%10;
	snprintf(tmp_addr.sun_path, sizeof(tmp_addr.sun_path),
		 "%s/%ld", name, tmp_clientid);

	if(bind(sd, (struct sockaddr *)&tmp_addr, sizeof(tmp_addr)) == -1) {
	  perror("bind()");
	  //return NULL;
	} else {
	  break;
	}
      }
      if(n==10) {
	return NULL;
      }

      initreq.type = MsgEventQInitReq;   // we want a handle
      initreq.client = tmp_clientid;
      memcpy(msg.socket.event_data, &initreq, sizeof(MsgQInitReqEvent_t));

      unix_addr.sun_family = AF_UNIX;
      
      if(strlen(name) >= sizeof(unix_addr.sun_path)) {
	return NULL;
      }
      snprintf(unix_addr.sun_path, sizeof(unix_addr.sun_path),
	       "%s/%ld", name, CLIENT_RESOURCE_MANAGER);
      

#if 0      
      if(connect(sd, (struct sockaddr *)&unix_addr, sizeof(unix_addr)) == -1) {
	perror("connect");
	return NULL;
      }
#endif      

      //send init request
      if((r = sendto(sd, &msg, sizeof(MsgQInitReqEvent_t), 0,
		     (struct sockaddr *)&unix_addr,
		     sizeof(unix_addr))) == -1) {
	perror("MsgOpen, sendto");
	return NULL;
      }

      //wait for response
      from_addr_len = sizeof(from_addr);
      if((r = recvfrom(sd, &msg, sizeof(msg), 0, 
		       (struct sockaddr *)&from_addr,
		       &from_addr_len)) == -1) {
	perror("MsgOpen, recvfrom");
	return NULL;
      }
      
      ret = (MsgEventQ_t *)malloc(sizeof(MsgEventQ_t));
	  
      ret->socket.type = MsgEventQType_socket;
      //ret->socket.server_addr = unix_addr; // resource manager addr
      ret->socket.server_addr.sun_family = AF_UNIX;
      snprintf(ret->socket.server_addr.sun_path, 
	       sizeof(ret->socket.server_addr.sun_path),
	       "%s", name);
      
      memcpy(&initgnt, msg.socket.event_data, sizeof(MsgQInitGntEvent_t));
      
      ret->socket.client_addr.sun_family = AF_UNIX;
      snprintf(ret->socket.client_addr.sun_path, 
	       sizeof(ret->socket.client_addr.sun_path),
	       "%s/%d", name,  (int)initgnt.newclientid);
      ret->socket.clientid = initgnt.newclientid;


      if(unlink(tmp_addr.sun_path) == -1) {
	perror("msgopen, unlink");
	fprintf(stderr, "'%s'\n", tmp_addr.sun_path);
      }
      
      close(sd);

      if((sd = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1) {
	perror("socket");
	return NULL;
      }
      
      if(bind(sd, (struct sockaddr *)&ret->socket.client_addr,
	      sizeof(ret->socket.client_addr)) == -1) {
	perror("msgopen bind()");
	fprintf(stderr, "'%s' %d\n", 
		ret->socket.client_addr.sun_path,
		sizeof(ret->socket.client_addr));
	return NULL;
      }

      ret->socket.sd = sd;
    }
    break;
  case MsgEventQType_pipe:
    break;
  }
  
  return ret;
  
}
#else
MsgEventQ_t *MsgOpen(int msqid)
{
  MsgEventQ_t *ret = NULL;
  msg_t msg;
  MsgQInitReqEvent_t initreq;
  MsgQInitGntEvent_t initgnt;
  
  msg.mtype = CLIENT_RESOURCE_MANAGER; // the recipient of this message
  initreq.type = MsgEventQInitReq;   // we want a handle
  initreq.client = CLIENT_UNINITIALIZED;
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
#endif


#ifdef SOCKIPC
void MsgClose(MsgEventQ_t *q)
{
  
  switch(q->any.type) {
#ifdef HAVE_SYSV_MSG
  case MsgEventQType_msgq:  
    fprintf(stderr, "msg close FIX\n");
  
    // just in case someone tries to access this pointer after close
    q->msgq.msqid = 0;
    q->msgq.mtype = 0;
    break;
#endif
  case MsgEventQType_socket:
    unlink(q->socket.client_addr.sun_path);
    close(q->socket.sd);
    memset(&q->socket.server_addr, 0, sizeof(q->socket.server_addr));
    memset(&q->socket.client_addr, 0, sizeof(q->socket.client_addr));
    q->socket.sd = 0;
    break;
  case MsgEventQType_pipe:
    fprintf(stderr, "msg close FIX close pipe\n");
    break;
  }

  free(q);
  
}
#else
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
#endif

#ifdef SOCKIPC
static int MsgNextEvent_internal(MsgEventQ_t *q, MsgEvent_t *event_return, int interruptible)
{
  msg_t msg;
 
  switch(q->any.type) {
#ifdef HAVE_SYSV_MSG
  case MsgEventQType_msgq:
    while(1) {
      if(msgrcv(q->msgq.msqid, (void *)&msg, sizeof(MsgEvent_t),
		q->msgq.mtype, 0) == -1) {
	switch(errno) {
	case EINTR:  // syscall interrupted 
	  if(!interruptible) {
	    continue;
	  }
	  break;
	default:
	  perror("MsgNextEvent");
	  break;
	}
	return -1;
      } else {
	
	memcpy(event_return, msg.msgq.event_data, sizeof(MsgEvent_t));
	return 0;
      }
    }    
    break;
#endif
  case MsgEventQType_socket:
    while(1) {
      struct sockaddr_un from_addr;
      int from_addr_len;
      int rlen;
      
      from_addr_len = sizeof(from_addr);
      
      if((rlen = recvfrom(q->socket.sd, (void *)&msg, sizeof(MsgEvent_t), 0,
			  (struct sockaddr *)&from_addr,
			  &from_addr_len)) == -1) {
	switch(errno) {
	case EINTR:  // syscall interrupted 
	  if(!interruptible) {
	    continue;
	  }
	  break;
	default:
	  perror("MsgNextEvent");
	  break;
	}
	return -1;
      } else {
	
	memcpy(event_return, msg.socket.event_data, rlen);

#ifdef XDEBUG
	fprintf(stderr, "MsgNextEvent: pid: %d got %s (%d B) from %d\n",
		getpid(), MsgEventType_str[event_return->any.type],
		rlen, event_return->any.client);
#endif
	return 0;
      }
    }    
    break;
  case MsgEventQType_pipe:
    fprintf(stderr, "NOT IMPLEMENTED pipe\n");
    break;
  }
  return -1;
}
#else
static int MsgNextEvent_internal(MsgEventQ_t *q, MsgEvent_t *event_return, int interruptible)
{
  msg_t msg;
  
  while(1) {
    if(msgrcv(q->msqid, (void *)&msg, sizeof(MsgEvent_t),
	      q->mtype, 0) == -1) {
      switch(errno) {
      case EINTR:  // syscall interrupted 
	if(!interruptible) {
	  continue;
	}
	break;
      default:
	perror("MsgNextEvent");
	break;
      }
      return -1;
    } else {
      
      memcpy(event_return, msg.event_data, sizeof(MsgEvent_t));
      return 0;
    }
  }    

}
#endif

int MsgNextEvent(MsgEventQ_t *q, MsgEvent_t *event_return) {
#ifdef XDEBUG
  fprintf(stderr, "MsgNextEvent: pid: %d\n", getpid());
#endif
  return MsgNextEvent_internal(q, event_return, 0);
}

int MsgNextEventInterruptible(MsgEventQ_t *q, MsgEvent_t *event_return) {
#ifdef XDEBUG
  fprintf(stderr, "MsgNextEventInter: pid: %d\n", getpid());
#endif
  return MsgNextEvent_internal(q, event_return, 1);
}


#ifndef SOCKIPC
#if (defined(BSD) && (BSD >= 199306))
int MsgNextEventNonBlocking(MsgEventQ_t *q, MsgEvent_t *event_return)
{
  msg_t msg;
  
  while (1) {
    if(msgrcv(q->msqid, (void *)&msg, sizeof(MsgEvent_t),
             q->mtype, IPC_NOWAIT) == -1) {
      switch(errno) {
#ifdef ENOMSG
      case ENOMSG:
#endif
      case EAGAIN:
      case EINTR:     // interrupted by system call/signal, try again
       usleep(10000);
       continue;
       break;
      default:
       perror("MsgNextEvent");
       break;
      }
      return -1;
    } else {
      
      memcpy(event_return, msg.event_data, sizeof(MsgEvent_t));
      return 0;
    }
  }

}
#endif
#endif

int MsgCheckEvent(MsgEventQ_t *q, MsgEvent_t *event_return)
{
  msg_t msg;
#ifdef XDEBUG
  fprintf(stderr, "MsgCheckEvent: pid: %d\n", getpid());
#endif
#ifdef SOCKIPC  
  switch(q->any.type) {
#ifdef HAVE_SYSV_MSG
  case MsgEventQType_msgq:
    while (1) {
      if(msgrcv(q->msgq.msqid, (void *)&msg, sizeof(MsgEvent_t),
		q->msgq.mtype, IPC_NOWAIT) == -1) {
	switch(errno) {
#ifdef ENOMSG
	case ENOMSG:
#endif
	case EAGAIN:
	  break;
	case EINTR:     // interrupted by system call/signal, try again
	  continue;
	  break;
	default:
	  perror("MsgNextEvent");
	  break;
	}
	return -1;
      } else {
	
	memcpy(event_return, msg.msgq.event_data, sizeof(MsgEvent_t));
	return 0;
      }
    }
    break;
#endif
  case MsgEventQType_socket:
    while (1) {
      struct sockaddr_un from_addr;
      int from_addr_len;
      struct pollfd fds[1];
      int r;
      int rlen;

      fds[0].fd = q->socket.sd;
      fds[0].events = POLLIN;
      from_addr_len = sizeof(from_addr);

      r = poll(fds, 1, 0);
      if(r > 0) {
	if(fds[0].revents & POLLIN) {
	  if((rlen = recvfrom(q->socket.sd, (void *)&msg,
			      sizeof(MsgEvent_t), 0,
			      (struct sockaddr *)&from_addr,
			      &from_addr_len)) == -1) {
	    switch(errno) {
#ifdef ENOMSG
	    case ENOMSG:
#endif
	    case EAGAIN:
	      break;
	    case EINTR:     // interrupted by system call/signal, try again
	      continue;
	      break;
	    default:
	      perror("MsgNextEvent");
	      break;
	    }
	    return -1;
	  } else {
	    
	    memcpy(event_return, msg.socket.event_data, rlen);

#ifdef XDEBUG
	    fprintf(stderr, "MsgCheckEvent: pid: %d got %s (%d B), from: %d\n",
		    getpid(), MsgEventType_str[event_return->any.type],
		    rlen, event_return->any.client);
#endif
	    return 0;
	  }
	}
      } else if(r == -1) {
	switch(errno) {
	case EAGAIN:
	case EINTR:
	  continue;
	  break;
	default:
	  perror("MsgNextEvent, poll");
	  return -1;
	  break;
	}
      } else {
	return -1;
      }
    }
    break;
  case MsgEventQType_pipe:
    break;
  }
#else
  while (1) {
    if(msgrcv(q->msqid, (void *)&msg, sizeof(MsgEvent_t),
	      q->mtype, IPC_NOWAIT) == -1) {
      switch(errno) {
#ifdef ENOMSG
      case ENOMSG:
#endif
      case EAGAIN:
	break;
      case EINTR:     // interrupted by system call/signal, try again
	continue;
	break;
      default:
	perror("MsgNextEvent");
	break;
      }
      return -1;
    } else {
      
      memcpy(event_return, msg.event_data, sizeof(MsgEvent_t));
      return 0;
    }
  }
#endif
  return -1;
}

int MsgSendEvent(MsgEventQ_t *q, MsgEventClient_t client,
		 MsgEvent_t *event_send, int msgflg)
{
  msg_t msg;
  int size = 0;
#ifdef SOCKIPC
  struct sockaddr_un to_addr;
  int to_addr_len = sizeof(to_addr);

  switch(q->any.type) {
#ifdef HAVE_SYSV_MSG
  case MsgEventQType_msgq:
    msg.msgq.mtype = client; // the recipient of this message
    event_send->any.client = q->msgq.mtype; // the sender  
    break;
#endif
  case MsgEventQType_socket:
    event_send->any.client = q->socket.clientid; // the sender  
    to_addr.sun_family = AF_UNIX;
    snprintf(to_addr.sun_path, sizeof(to_addr.sun_path),
	     "%s/%d", q->socket.server_addr.sun_path, (int)client);
    break;
  case MsgEventQType_pipe:
    fprintf(stderr, "msgsendevent NOT IMP pipe\n");
    break;
  }
#else
  msg.mtype = client; // the recipient of this message
  event_send->any.client = q->mtype; // the sender  
#endif
#ifdef XDEBUG
  fprintf(stderr, "MsgSendEvent: pid: %d\n", getpid());
#endif
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
  case MsgEventQDestroyBuf:
    size = sizeof(MsgQDestroyBufEvent_t);
    break;
  case MsgEventQReqPicBuf:
    size = sizeof(MsgQReqPicBufEvent_t);
    break;
  case MsgEventQGntPicBuf:
    size = sizeof(MsgQGntPicBufEvent_t);
    break;
  case MsgEventQDestroyPicBuf:
    size = sizeof(MsgQDestroyPicBufEvent_t);
    break;
  case MsgEventQCtrlData:
    size = sizeof(MsgQCtrlDataEvent_t);
    break;
  case MsgEventQReqPicQ:
    size = sizeof(MsgQReqPicQEvent_t);
    break;
  case MsgEventQGntPicQ:
    size = sizeof(MsgQGntPicQEvent_t);
    break;
  case MsgEventQAttachQ:
  case MsgEventQAppendQ:
    size = sizeof(MsgQAttachQEvent_t);
    break;
  case MsgEventQDetachQ:
  case MsgEventQQDetached:
  case MsgEventQDestroyQ:
    size = sizeof(MsgQDetachQEvent_t);
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
  case MsgEventQDemuxStreamChange2:
    size = sizeof(MsgQDemuxStreamChange2Event_t);
    break;
  case MsgEventQDemuxDefault:
    size = sizeof(MsgQDemuxDefaultEvent_t);
    break;
  case MsgEventQDVDCtrlLong:
    switch(event_send->dvdctrllong.cmd.type) {
    case DVDCtrlLongSetDVDRoot:
      size = sizeof(MsgQAnyEvent_t) + sizeof(DVDCtrlLongAnyEvent_t)
	+ strlen(event_send->dvdctrllong.cmd.dvdroot.path)+1;
      break;
    case DVDCtrlLongSetState:
      size = sizeof(MsgQAnyEvent_t) + sizeof(DVDCtrlLongAnyEvent_t)
	+ strlen(event_send->dvdctrllong.cmd.state.xmlstr)+1;
      break;
    case DVDCtrlLongVolIds:
      size = sizeof(MsgQAnyEvent_t) + sizeof(DVDCtrlLongVolIdsEvent_t);
      break;
    default:
      size = sizeof(MsgQDVDCtrlLongEvent_t);
      break;
    }
    break;
  case MsgEventQDemuxDVD:
    size = sizeof(MsgQDemuxDVDEvent_t);
    break;
  case MsgEventQDemuxDVDRoot:
    size = sizeof(MsgQAnyEvent_t)+strlen(event_send->demuxdvdroot.path)+1;
    break;
  case MsgEventQSetAspectModeSrc:
    size = sizeof(MsgQSetAspectModeSrcEvent_t);
    break;
  case MsgEventQSetSrcAspect:
    size = sizeof(MsgQSetSrcAspectEvent_t);
    break;
  case MsgEventQSetZoomMode:
    size = sizeof(MsgQSetZoomModeEvent_t);
    break;
  case MsgEventQReqInput:
    size = sizeof(MsgQReqInputEvent_t);
    break;
  case MsgEventQInputButtonPress:
  case MsgEventQInputButtonRelease:
  case MsgEventQInputPointerMotion:
  case MsgEventQInputKeyPress:
  case MsgEventQInputKeyRelease:
    size = sizeof(MsgQInputEvent_t);
    break;
  case MsgEventQSaveScreenshot:
    size = sizeof(MsgQAnyEvent_t) + sizeof(ScreenshotMode_t) + 
      strlen(event_send->savescreenshot.formatstr)+1;
    break;
  default:
    fprintf(stderr, "MsgSendEvent: Unknown event: %d\n", event_send->type);
    return -1;
  }

#ifdef SOCKIPC
  switch(q->any.type) {
#ifdef HAVE_SYSV_MSG
  case MsgEventQType_msgq:
    memcpy(msg.msgq.event_data, event_send, size);
    break;
#endif
  case MsgEventQType_socket:
    memcpy(msg.socket.event_data, event_send, size);

    break;
  case MsgEventQType_pipe:
    break;
  }
#else
  memcpy(msg.event_data, event_send, size);
#endif

#ifdef DEBUG
  fprintf(stderr, "Sending '%s' from: %ld to: %ld\n",
	  MsgEventType_str[event_send->any.type],
	  event_send->any.client,
	  client);
#endif
  
#ifdef SOCKIPC
  switch(q->any.type) {
#ifdef HAVE_SYSV_MSG
  case MsgEventQType_msgq:
    while(1) {
      if(msgsnd(q->msgq.msqid, (void *)&msg, size, msgflg) == -1) {
	switch(errno) {
	case EINTR: //interrupted by syscall/signal, try again
	  continue;
	  break;
	default:
	  perror("MsgSendEvent: msgsnd");
	break;
	}
	return -1;
      } else {
	return 0;
      }
    }
    break;
#endif
  case MsgEventQType_socket:
    while(1) {
      int rlen;
      if((rlen = sendto(q->socket.sd, (void *)&msg, size, 0,
			(struct sockaddr *)&to_addr, to_addr_len)) == -1) {
	switch(errno) {
	case EINTR: //interrupted by syscall/signal, try again
	  continue;
	  break;
	default:
	  perror("MsgSendEvent: sendto");
	  fprintf(stderr, "'%s'\n", to_addr.sun_path);
	break;
	}
	return -1;
      } else {
#ifdef XDEBUG
	fprintf(stderr, "Sending  %d (%d B) from: %ld to: %ld\n",
		event_send->any.type,
		rlen,
		event_send->any.client,
		client);
#endif
	return 0;
      }
    }
    break;
  case MsgEventQType_pipe:
    break;
  }
#else
  while(1) {
    if(msgsnd(q->msqid, (void *)&msg, size, msgflg) == -1) {
      switch(errno) {
      case EINTR: //interrupted by syscall/signal, try again
	continue;
	break;
      default:
	perror("MsgSendEvent: msgsnd");
	fprintf(stderr, "msqid: %d, size: %d, mtype %d, msg: %08x\n",
		q->msqid, size, q->mtype, &msg);
	break;
      }
      return -1;
    } else {
      return 0;
    }
  }
#endif  
  return -1;
}


#ifdef SOCKIPC
int get_msgqtype(char *msgq_str, MsgEventQType_t *type, char **id)
{

  if(!strncmp(msgq_str, "socket:", strlen("socket:"))) {
    *type = MsgEventQType_socket;
    *id = strdup(&msgq_str[7]);
#ifdef HAVE_SYSV_MSG
  } else if(!strncmp(msgq_str, "msg:", strlen("msg:"))) {
    *type = MsgEventQType_msgq;
    *id = strdup(&msgq_str[4]);
#endif
  } else if(!strncmp(msgq_str, "pipe:", strlen("pipe:"))) {

  } else {
    return -1;
  }

  return 0;
}
#endif
  
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
