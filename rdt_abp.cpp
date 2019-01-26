#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdio>

#include "Utils.h"

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: SLIGHTLY MODIFIED
 FROM VERSION 1.1 of J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
       are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
       or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
       (although some can be lost).
**********************************************************************/

#define BIDIRECTIONAL 0 /* change to 1 if you're doing extra credit */
/* and write a routine called B_output */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
	char data[20];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
	int seqnum;
	int acknum;
	int checksum;
	char payload[20];
};

struct rtp_layer_abp_t {
	int seqnum;/// current expected seq
	sender_state senderState;
	int cnt_layer3;
	int cnt_layer5;
	pkt *buffer;
};

/********* FUNCTION PROTOTYPES. DEFINED IN THE LATER PART******************/
void starttimer(int AorB, float increment);

void stoptimer(int AorB);

void tolayer3(int AorB, struct pkt packet);

void tolayer5(int AorB, char datasent[20]);

void printLog(int AorB, char *msg, const struct pkt *p, struct msg *m);

void printStat();

int calc_checksum(const pkt *p);

pkt make_pkt(msg message, int seqNum, int ackNum = ACK_ABP_DEFAULT);

pkt make_pkt(const char data[MSG_LEN], int seqNum, int ackNum);
/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
#define A 0
#define B 1

#define TIMEOUT 20.0

rtp_layer_abp_t A_rtp, B_rtp;
struct pkt cur_pkt;

/* called from layer 5, passed the data to be sent to other side
 * called with msg to send pkt
*/
void A_output(struct msg message) {
	rtp_layer_abp_t &rtp_layer = A_rtp;

	++rtp_layer.cnt_layer5;

	/* If the last packet have not been ACKed, just drop this message */
	if (rtp_layer.senderState != WAITING_FOR_PKT) {
		printLog(A, const_cast<char *>("Drop this message, last message have not finished"), NULL, &message);
		return;
	}

	rtp_layer.senderState = WAITING_FOR_ACK;/* set current pkt to not finished */

	cur_pkt = make_pkt(message, rtp_layer.seqnum, ACK_ABP_DEFAULT);/* sending packets do not need to set acknum */

	tolayer3(A, cur_pkt);
	++rtp_layer.cnt_layer3;

	printLog(A, const_cast<char *>("Sent packet to layer3"), &cur_pkt, &message);
	starttimer(A, TIMEOUT);
}

/* need be completed only for extra credit */
void B_output(struct msg message) {

}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet) {
	rtp_layer_abp_t &rtp_layer = A_rtp;
	printLog(A, const_cast<char *>("Receive ACK packet from layer3"), &packet, NULL);

	/// if packet has been received successfully
	if (packet.acknum == rtp_layer.seqnum && packet.checksum == calc_checksum(&packet)) {
		rtp_layer.seqnum = (rtp_layer.seqnum + 1) % 2;
		rtp_layer.senderState = WAITING_FOR_PKT;
		stoptimer(A);/// stop timer before changing state

		printLog(A, const_cast<char *>("ACK packet process successfully accomplished!!"), &packet, NULL);
		return;
	}

	/* check checksum, if corrupted*/
	else if (packet.checksum != calc_checksum(&packet)) {
		printLog(A, const_cast<char *>("ACK packet is corrupted"), &packet, NULL);
	}
	/* NAK or duplicate ACK : corrupted packet received */
	else if (packet.acknum != rtp_layer.seqnum) {
		stoptimer(A);
		printLog(A, const_cast<char *>("ACK is not expected : NACK"), &packet, NULL);
		printLog(A, const_cast<char *>("Resend the packet again"), &cur_pkt, NULL);
		tolayer3(A, cur_pkt);
		++rtp_layer.cnt_layer3;
		starttimer(A, TIMEOUT);
	}
}

/* called when A's timer goes off */
void A_timerinterrupt() {
	rtp_layer_abp_t &rtp_layer = A_rtp;
	printLog(A, const_cast<char *>("Timeout occurred"), &cur_pkt, NULL);
	if (rtp_layer.senderState == WAITING_FOR_ACK) { // should be a redundant check
		printLog(A, const_cast<char *>("Timeout! Resend the packet again"), &cur_pkt, NULL);
		tolayer3(A, cur_pkt);
		++rtp_layer.cnt_layer3;
		starttimer(A, TIMEOUT);
	} else {
		perror(">> Problem\n");
	}
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
	A_rtp = {0, WAITING_FOR_PKT, 0, 0, nullptr};
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet) {
	rtp_layer_abp_t &rtp_layer = B_rtp;

	printLog(B, const_cast<char *>("Receive a packet from layer3"), &packet, NULL);
	++rtp_layer.cnt_layer3;
	pkt ack = make_pkt(packet.payload, SEQ_ABP_DEFAULT, packet.seqnum);// send packet seq no as it is received

	/* normal packet, deliver data to layer5 */
	if (packet.seqnum == rtp_layer.seqnum && packet.checksum == calc_checksum(&packet)) {
		rtp_layer.seqnum = (rtp_layer.seqnum + 1) % 2;
		tolayer5(B, packet.payload);
		++rtp_layer.cnt_layer5;
		printLog(B, const_cast<char *>("Sent packet to layer5"), &packet, NULL);
	}

	/* check checksum, if corrupted*/
	else if (packet.checksum != calc_checksum(&packet)) {
		printLog(B, const_cast<char *>("Data Packet is corrupted"), &packet, NULL);
	}

		/* duplicate pkt resends the ACK*/
	else if (packet.seqnum != rtp_layer.seqnum) {
		printLog(B, const_cast<char *>("Duplicated packet detected"), &packet, NULL);
	}
	tolayer3(B, ack); // nextseqnum doesn't have any meaning here
	printLog(B, const_cast<char *>("Send ACK packet to layer3"), &ack, NULL);
}

/* called when B's timer goes off */
void B_timerinterrupt(void) {
	printf("  B_timerinterrupt: B doesn't have a timer. ignore.\n");
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init() {
	B_rtp = {0, WAITING_FOR_ACK, 0, 0, nullptr};
}

/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
    - emulates the tranmission and delivery (possibly with bit-level corruption
        and packet loss) of packets across the layer 3/4 interface
    - handles the starting/stopping of a timer, and generates timer
        interrupts (resulting in calling students timer handler).
    - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event {
	float evtime;       /* event time */
	int evtype;         /* event type code */
	int eventity;       /* entity where event occurs */
	struct pkt *pktptr; /* ptr to packet (if any) assoc w/ this event */
	struct event *prev;
	struct event *next;
};
struct event *evlist = NULL; /* the event list */

/* possible events: */
#define TIMER_INTERRUPT 0
#define FROM_LAYER5 1
#define FROM_LAYER3 2

#define OFF 0
#define ON 1

int TRACE = 1;     /* for my debugging */
int nsim = 0;      /* number of messages from 5 to 4 so far */
int nsimmax = 0;   /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;    /* probability that a packet is dropped  */
float corruptprob; /* probability that one bit is packet is flipped */
float lambda;      /* arrival rate of messages from layer 5 */
int ntolayer3;     /* number sent into layer 3 */
int nlost;         /* number lost in media */
int ncorrupt;      /* number corrupted by media*/

void init();

void generate_next_arrival(void);

void insertevent(struct event *p);

int main() {
	struct event *eventptr;
	struct msg msg2give;
	struct pkt pkt2give;

	int i, j;
	char c;

	init();
	A_init();
	B_init();

	while (1) {
		eventptr = evlist; /* get next event to simulate */
		if (eventptr == NULL)
			goto terminate;
		evlist = evlist->next; /* remove this event from event list */
		if (evlist != NULL)
			evlist->prev = NULL;
		if (TRACE >= 2) {
			printf("\nEVENT time: %f,", eventptr->evtime);
			printf("  type: %d", eventptr->evtype);
			if (eventptr->evtype == 0)
				printf(", timerinterrupt  ");
			else if (eventptr->evtype == 1)
				printf(", fromlayer5 ");
			else
				printf(", fromlayer3 ");
			printf(" entity: %d\n", eventptr->eventity);
		}
		time = eventptr->evtime; /* update time to next event time */
		if (eventptr->evtype == FROM_LAYER5) {
			if (nsim < nsimmax) {
				if (nsim + 1 < nsimmax)
					generate_next_arrival(); /* set up future arrival */
				/* fill in msg to give with string of same letter */
				j = nsim % 26;
				for (i = 0; i < 20; i++)
					msg2give.data[i] = 97 + j;
				msg2give.data[19] = 0;
				if (TRACE > 2) {
					printf("          MAINLOOP: data given to student: ");
					for (i = 0; i < 20; i++)
						printf("%c", msg2give.data[i]);
					printf("\n");
				}
				nsim++;
				if (eventptr->eventity == A)
					A_output(msg2give);
				else
					B_output(msg2give);
			}
		} else if (eventptr->evtype == FROM_LAYER3) {
			pkt2give.seqnum = eventptr->pktptr->seqnum;
			pkt2give.acknum = eventptr->pktptr->acknum;
			pkt2give.checksum = eventptr->pktptr->checksum;
			for (i = 0; i < 20; i++)
				pkt2give.payload[i] = eventptr->pktptr->payload[i];
			if (eventptr->eventity == A) /* deliver packet by calling */
				A_input(pkt2give); /* appropriate entity */
			else
				B_input(pkt2give);
			free(eventptr->pktptr); /* free the memory for packet */
		} else if (eventptr->evtype == TIMER_INTERRUPT) {
			if (eventptr->eventity == A)
				A_timerinterrupt();
			else
				B_timerinterrupt();
		} else {
			printf("INTERNAL PANIC: unknown event type \n");
		}
		free(eventptr);
	}

	terminate:
	printf(
			" Simulator terminated at time %f\n after sending %d msgs from layer5\n",
			time, nsim);
	printStat();
}

void init() /* initialize the simulator */
{
	int i;
	float sum, avg;
	float jimsrand();
#ifdef DEBUG_ABP
//    freopen("in.log","r",stdin);
	nsimmax = DEBUG_ABP_NMSG;
	lossprob = DEBUG_ABP_PROB_LOSS;
	corruptprob = DEBUG_ABP_PROB_CORP;
	lambda = DEBUG_ABP_TIME;
	TRACE = DEBUG_ABP_TRACE;
	printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
	printf("Enter the number of messages to simulate: %d\n", nsimmax);
	printf("Enter  packet loss probability [enter 0.0 for no loss]: %f\n", lossprob);
	printf("Enter packet corruption probability [0.0 for no corruption]: %f\n", corruptprob);
	printf("Enter average time between messages from sender's layer5 [ > 0.0]: %f\n", lambda);
	printf("Enter TRACE: %d\n", TRACE);
#endif
#ifndef DEBUG_ABP
	printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
	printf("Enter the number of messages to simulate: ");
	scanf("%d",&nsimmax);
	printf("Enter  packet loss probability [enter 0.0 for no loss]:");
	scanf("%f",&lossprob);
	printf("Enter packet corruption probability [0.0 for no corruption]:");
	scanf("%f",&corruptprob);
	printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
	scanf("%f",&lambda);
	printf("Enter TRACE:");
	scanf("%d",&TRACE);
#endif
	srand(9999); /* init random number generator */
	sum = 0.0;   /* test random number generator for students */
	for (i = 0; i < 1000; i++)
		sum = sum + jimsrand(); /* jimsrand() should be uniform in [0,1] */
	avg = sum / 1000.0;
	if (avg < 0.25 || avg > 0.75) {
		printf("It is likely that random number generation on your machine\n");
		printf("is different from what this emulator expects.  Please take\n");
		printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
		exit(1);
	}

	ntolayer3 = 0;
	nlost = 0;
	ncorrupt = 0;

	time = 0.0;              /* initialize time to 0.0 */
	generate_next_arrival(); /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand(void) {
	double mmm = RAND_MAX;
	float x;                 /* individual students may need to change mmm */
	x = rand() / mmm;        /* x should be uniform in [0,1] */
	return (x);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

void generate_next_arrival(void) {
	double x, log(), ceil();
	struct event *evptr;
	float ttime;
	int tempint;

	if (TRACE > 2)
		printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

	x = lambda * jimsrand() * 2; /* x is uniform on [0,2*lambda] */
	/* having mean of lambda        */
	evptr = (struct event *) malloc(sizeof(struct event));
	evptr->evtime = time + x;
	evptr->evtype = FROM_LAYER5;
	if (BIDIRECTIONAL && (jimsrand() > 0.5))
		evptr->eventity = B;
	else
		evptr->eventity = A;
	insertevent(evptr);
}

void insertevent(struct event *p) {
	struct event *q, *qold;

	if (TRACE > 2) {
		printf("            INSERTEVENT: time is %lf\n", time);
		printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
	}
	q = evlist;      /* q points to header of list in which p struct inserted */
	if (q == NULL)   /* list is empty */
	{
		evlist = p;
		p->next = NULL;
		p->prev = NULL;
	} else {
		for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next)
			qold = q;
		if (q == NULL)   /* end of list */
		{
			qold->next = p;
			p->prev = qold;
			p->next = NULL;
		} else if (q == evlist)     /* front of list */
		{
			p->next = evlist;
			p->prev = NULL;
			p->next->prev = p;
			evlist = p;
		} else     /* middle of list */
		{
			p->next = q;
			p->prev = q->prev;
			q->prev->next = p;
			q->prev = p;
		}
	}
}

void printevlist(void) {
	struct event *q;
	int i;
	printf("--------------\nEvent List Follows:\n");
	for (q = evlist; q != NULL; q = q->next) {
		printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype,
		       q->eventity);
	}
	printf("--------------\n");
}

/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB /* A or B is trying to stop timer */) {
	struct event *q, *qold;

	if (TRACE > 2)
		printf("          STOP TIMER: stopping timer at %f\n", time);
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
	for (q = evlist; q != NULL; q = q->next)
		if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
			/* remove this event */
			if (q->next == NULL && q->prev == NULL)
				evlist = NULL;          /* remove first and only event on list */
			else if (q->next == NULL) /* end of list - there is one in front */
				q->prev->next = NULL;
			else if (q == evlist)   /* front of list - there must be event after */
			{
				q->next->prev = NULL;
				evlist = q->next;
			} else     /* middle of list */
			{
				q->next->prev = q->prev;
				q->prev->next = q->next;
			}
			free(q);
			return;
		}
	printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

void starttimer(int AorB /* A or B is trying to start timer */, float increment) {
	struct event *q;
	struct event *evptr;

	if (TRACE > 2)
		printf("          START TIMER: starting timer at %f\n", time);
	/* be nice: check to see if timer is already started, if so, then  warn */
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
	for (q = evlist; q != NULL; q = q->next)
		if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
			printf("Warning: attempt to start a timer that is already started\n");
			return;
		}

	/* create future event for when timer goes off */
	evptr = (struct event *) malloc(sizeof(struct event));
	evptr->evtime = time + increment;
	evptr->evtype = TIMER_INTERRUPT;
	evptr->eventity = AorB;
	insertevent(evptr);
}

/************************** TOLAYER3 ***************/
void tolayer3(int AorB, struct pkt packet) {
	struct pkt *mypktptr;
	struct event *evptr, *q;
	float lastime, x;
	int i;

	ntolayer3++;

	/* simulate losses: */
	if (jimsrand() < lossprob) {
		nlost++;
		if (TRACE > 0)
			printf("          TOLAYER3: packet being lost\n");
		return;
	}

	/* make a copy of the packet student just gave me since he/she may decide */
	/* to do something with the packet after we return back to him/her */
	mypktptr = (struct pkt *) malloc(sizeof(struct pkt));
	mypktptr->seqnum = packet.seqnum;
	mypktptr->acknum = packet.acknum;
	mypktptr->checksum = packet.checksum;
	for (i = 0; i < 20; i++)
		mypktptr->payload[i] = packet.payload[i];
	if (TRACE > 2) {
		printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
		       mypktptr->acknum, mypktptr->checksum);
		for (i = 0; i < 20; i++)
			printf("%c", mypktptr->payload[i]);
		printf("\n");
	}

	/* create future event for arrival of packet at the other side */
	evptr = (struct event *) malloc(sizeof(struct event));
	evptr->evtype = FROM_LAYER3;      /* packet will pop out from layer3 */
	evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
	evptr->pktptr = mypktptr;         /* save ptr to my copy of packet */
	/* finally, compute the arrival time of packet at the other end.
	   medium can not reorder, so make sure packet arrives between 1 and 10
	   time units after the latest arrival time of packets
	   currently in the medium on their way to the destination */
	lastime = time;
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
	for (q = evlist; q != NULL; q = q->next)
		if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity))
			lastime = q->evtime;
	evptr->evtime = lastime + 1 + 9 * jimsrand();

	/* simulate corruption: */
	if (jimsrand() < corruptprob) {
		ncorrupt++;
		if ((x = jimsrand()) < .75)
			mypktptr->payload[0] = 'Z'; /* corrupt payload */
		else if (x < .875)
			mypktptr->seqnum = 999999;
		else
			mypktptr->acknum = 999999;
		if (TRACE > 0)
			printf("          TOLAYER3: packet being corrupted\n");
	}

	if (TRACE > 2)
		printf("          TOLAYER3: scheduling arrival on other side\n");
	insertevent(evptr);
}

void tolayer5(int AorB, char datasent[20]) {
	int i;
	if (TRACE > 2) {
		printf("          TOLAYER5: data received: ");
		for (i = 0; i < 20; i++)
			printf("%c", datasent[i]);
		printf("\n");
	}
}


/******************* DEBUG ***********************/
void printLog(int AorB, char *msg, const struct pkt *p, struct msg *m) {
	static bool opened = false;
	if (!opened) opened = true, fclose(fopen("out.log", "w"));
//    freopen("out.log","a",stdout);
	FILE *fp = fopen("out.log", "a");
	char ch = (AorB == A) ? 'A' : 'B';
	if (p != NULL) {
//        printLog(ch,msg,p->nextseqnum,p->acknum,p->checksum,p->payload,m->data);
		fprintf(fp, "[%c] @%f %s. Packet[seq=%d,ack=%d,check=%d,data=%s..]\n", ch, time,
		        msg, p->seqnum, p->acknum, p->checksum, p->payload);
	} else if (m != NULL) {
//	    printLog(ch,msg,p->nextseqnum,p->acknum,p->checksum,p->payload,m->data);
		fprintf(fp, "[%c] @%f %s. Message[data=%s..]\n", ch, time, msg, m->data);
	} else {
		fprintf(fp, "[%c] @%f %s.\n", ch, time, msg);
	}
//    freopen("out.log","a",stdout);
	fclose(fp);
}


//===================== Utils =====================
int calc_checksum(const pkt *p) {
	calc_checksum(p->seqnum, p->acknum, p->payload);
}

pkt make_pkt(msg message, int seqNum, int ackNum) {
	pkt packet = {seqNum, ackNum, 0, ""};
	strncpy(packet.payload, message.data, MSG_LEN);
	packet.checksum = calc_checksum(&packet);
	return packet;
}

pkt make_pkt(const char data[MSG_LEN], int seqNum, int ackNum) {
	pkt packet = {seqNum, ackNum, 0, ""};
	strncpy(packet.payload, data, MSG_LEN);
	packet.checksum = calc_checksum(&packet);
	return packet;
}

void printStat() {
	printf("#Sent Packets from Layer5-to   A: %d\n", A_rtp.cnt_layer5);
	printf("#Sent Packets to   Layer3-from A: %d\n", A_rtp.cnt_layer3);
	printf("#Sent Packets from Layer3-to   B: %d\n", B_rtp.cnt_layer3);
	printf("#Sent Packets to   Layer5-from B: %d\n", B_rtp.cnt_layer5);
}
