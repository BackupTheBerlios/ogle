#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>

#include "../ogle/msgevents.h"




void process_msgid(int msqid)
{
  size_t bufsize;
  struct msgsnap_head *buf;
  struct msgsnap_mhead *mhead;
  int i;

  /* allocate a minimum-size buffer */
  buf = malloc(bufsize = sizeof(struct msgsnap_head));

  /* read all of the messages from the queue */
  for (;;) {
    if (msgsnap(msqid, buf, bufsize, 0) != 0) {

	perror("msgsnap");
      free(buf);
      return;
    }
    if (bufsize >= buf->msgsnap_size)  /* we got them all */
      break;
    /* we need a bigger buffer */
    buf = realloc(buf, bufsize = buf->msgsnap_size);
  }

  fprintf(stderr, "bufsize: %d\n", buf->msgsnap_size);

  /* process each message in the queue (there may be none) */
  mhead = (struct msgsnap_mhead *)(buf + 1);  /* first message */
  for (i = 0; i < buf->msgsnap_nmsg; i++) {
    MsgEvent_t ev;
    msg_t *msg;
    size_t mlen = mhead->msgsnap_mlen;

    /* process the message contents */
    //process_message(mhead->msgsnap_mtype, (char *)(mhead+1), mlen);
    fprintf(stderr, "type: %d, size: %d\n",
	    mhead->msgsnap_mtype, mlen);
    
    memcpy(&ev, mhead+1, mhead->msgsnap_mlen);
    
    fprintf(stderr, "type: %d, from: %d, to: %d\n",
	    ev.type, ev.any.client, mhead->msgsnap_mtype);
    
    /* advance to the next message header */
    mhead = (struct msgsnap_mhead *)
      ((char *)mhead + sizeof(struct msgsnap_mhead) +
       ((mlen + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1)));
  }

  free(buf);
}




int main(int argc, char **argv)
{
  

  process_msgid(atoi(argv[1]));

  return 0;
}
