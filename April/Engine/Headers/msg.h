/*
 * header file for message handling
 */
#ifndef _MSG_H_
#define _MSG_H_

#include "process.h"
#include <sys/time.h>

typedef struct msg_rec {
  processpo to;			/* Who the message is to */
  objPo sender;			/* Sender of the message */
  objPo opts;			/* Additional message attributes */
  objPo msg;			/* The message */
  long sequence;		/* Sequence number of this message */
  time_t lease;			/* What is the lease time on this message? */
  msgpo prev;			/* Previous message */
  msgpo next;			/* Next message */
} msg_record;

/* message processing functions */
retCode sendAmsg(objPo to,objPo msg,objPo sender,objPo opts);
void LocalMsg(processpo p,objPo msg,objPo sender,objPo opts);
void free_msg(processpo p,msgpo m);
msgpo get_msg(processpo P);
void set_msg_range(processpo P);
void reset_msg_q(processpo P);
void discard_msgs(processpo P);
void printMessages(void);
void printMsgs(processpo p);

int msgcount(processpo p);

void init_msgs(void);		/* initialize message queues etc. */

/* Escape interface */
retCode m_send(processpo p,objPo *args);	/* Send a message */
retCode m_send2(processpo p,objPo *args);	/* Privileged version of send */
retCode m_front_msg(processpo p,objPo *args); /* Post message on front */
retCode m_nextmsg(processpo p,objPo *args);
retCode m_waitmsg(processpo p,objPo *args);
retCode m_replacemsg(processpo p,objPo *args);
retCode m_mailer(processpo p,objPo *args);	/* what is our mail handler */
retCode m_set_mailer(processpo p,objPo *args); /* set name of the mailer process */

#endif
