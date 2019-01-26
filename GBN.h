//
// Created by Subangkar on 26-Jan-19.
//

#ifndef RDT_GBN_H
#define RDT_GBN_H
#define BIDIRECTIONAL 0    /* change to 1 if you're doing extra credit */
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

/********* FUNCTION PROTOTYPES. DEFINED IN THE LATER PART******************/
void starttimer(int AorB, float increment);

void stoptimer(int AorB);

void tolayer3(int AorB, struct pkt packet);

void tolayer5(int AorB, char datasent[20]);


/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
void A_output(msg);

void A_init();

void A_input(pkt);

void B_output(msg);

void B_init();

void B_input(pkt);


/* common defines */
#define   A    0
#define   B    1
/* timeout for the timer */
#define TIMEOUT 20.0
/* when A send a data package to B,
   we do not need to set acknum,
   so we set it to this default number */
#define DEFAULT_ACK 111

/* current expected seq for B */
int B_seqnum = 0;

/* defines for statistic */
int A_from_layer5 = 0;
int A_to_layer3 = 0;
int B_from_layer3 = 0;
int B_to_layer5 = 0;

/* defines for sender */
/* define window size */
#define N 10
int base = 0;
int nextseq = 0;
/* ring buffer for all packets in window */
int packets_base = 0;
struct pkt packets[N];

/* extra buffer used when window is full */
struct node {
	struct msg message;
	struct node *next;
};
struct node *list_head = NULL;
struct node *list_end = NULL;
int Extra_BufSize = 0;

void append_msg(struct msg *m) {
	int i;
	/*allocate memory*/
	struct node *n = (node *) malloc(sizeof(struct node));
	if (n == NULL) {
		printf("no enough memory\n");
		return;
	}
	n->next = NULL;
	/*copy packet*/
	for (i = 0; i < 20; ++i) {
		n->message.data[i] = m->data[i];
	}

	/* if list empty, just add into the list*/
	if (list_end == NULL) {
		list_head = n;
		list_end = n;
		++Extra_BufSize;
		return;
	}
	/* otherwise, add at the end*/
	list_end->next = n;
	list_end = n;
	++Extra_BufSize;
}

struct node *pop_msg() {
	struct node *p;
	/* if the list is empty, return NULL*/
	if (list_head == NULL) {
		return NULL;
	}

	/* retrive the first node*/
	p = list_head;
	list_head = p->next;
	if (list_head == NULL) {
		list_end = NULL;
	}
	--Extra_BufSize;

	return p;
}

/* common tools */
/*This is the implementation of checksum. Data packets and ACK packets use the same method. */
/*
 * There will be no carry flow.
 * seqnum < 1000;
 * acknum < 1000;
 * each payload < 255;
 * so the maximum of checksum < 1000 + 1000 + 20*255 < MAX_INT
 * Why add seqnum and acknum?
Because checksum is used to make sure the packet is correct, not corrupted.
Seqnum and acknum may also be corrupted, so we need to add seqnum and acknum.

 * About the check sum:
 * There is no perfect method for check sum, only suitable method.
 * In this assignment, we only need to add all of them as it is simple and it works well.
*/

int calc_checksum(struct pkt *p) {
	int i;
	int checksum = 0; /*First init to Zero*/
	if (p == NULL) {
		return checksum;
	}
/*Add all the characters in payload*/
	for (i = 0; i < 20; i++) {
		checksum += (unsigned char) p->payload[i];
	}
	/*add the seqnum*/
	checksum += p->seqnum;
	/*add the acknum*/
	checksum += p->acknum;
/*Then we get the final checksum.*/
	return checksum;
}

void Debug_Log(int AorB, char *msg, struct pkt *p, struct msg *m) {
	char ch = (AorB == A) ? 'A' : 'B';
	if (AorB == A) {
		if (p != NULL) {
			printf("[%c] %s. Window[%d,%d) Packet[seq=%d,ack=%d,check=%d,data=%c..]\n", ch, msg,
			       base, nextseq, p->seqnum, p->acknum, p->checksum, p->payload[0]);
		} else if (m != NULL) {
			printf("[%c] %s. Window[%d,%d) Message[data=%c..]\n", ch, msg, base, nextseq, m->data[0]);
		} else {
			printf("[%c] %s.Window[%d,%d)\n", ch, msg, base, nextseq);
		}
	} else {
		if (p != NULL) {
			printf("[%c] %s. Expected[%d] Packet[seq=%d,ack=%d,check=%d,data=%c..]\n", ch, msg,
			       B_seqnum, p->seqnum, p->acknum, p->checksum, p->payload[0]);
		} else if (m != NULL) {
			printf("[%c] %s. Expected[%d] Message[data=%c..]\n", ch, msg, B_seqnum, m->data[0]);
		} else {
			printf("[%c] %s.Expected[%d]\n", ch, msg, B_seqnum);
		}
	}
}

/* helper functions for the window */
int window_isfull() {
	if (nextseq >= base + N)
		return 1;
	else
		return 0;
}

struct pkt *get_free_packet() {
	int cur_index = 0;

	/* check is window full */
	if (nextseq - base >= N) {
		Debug_Log(A, "Alloc packet failed. The window is full already", NULL, NULL);
		return NULL;
	}

	/* get next free packet */
	cur_index = (packets_base + nextseq - base) % N;

	return &(packets[cur_index]);
}

struct pkt *get_packet(int seqnum) {
	int cur_index = 0;
	if (seqnum < base || seqnum >= nextseq) {
		Debug_Log(A, "Seqnum is not within the window", NULL, NULL);
		return NULL;
	}

	cur_index = (packets_base + seqnum - base) % N;
	return &(packets[cur_index]);
}

void free_packets(int acknum) {
	int count;
	if (acknum < base || acknum >= nextseq)
		return;

	packets_base += (acknum - base + 1);
	count = acknum - base + 1;
	packets_base = packets_base % N;
	base = acknum + 1;

	//when the window size decrease, check the
	//extra buffer, if we need any message need to sned
	while (count > 0) {
		struct node *n = pop_msg();
		if (n == NULL) {
			break;
		}
		A_output(n->message);
		free(n);
		count--;
	}
}

/* called from layer 5, passed the data to be sent to other side */

/* Every time there is a new packet come,
 * a) we append this packet at the end of the extra buffer.
 * b) Then we check the window is full or not; If the window is full, we just leave the packet in the extra buffer;
 * c) If the window is not full, we retrieve one packet at the beginning of the extra buffer, and process it.
 */
void A_output(struct msg message) {
	printf("================================ Inside A_output===================================\n");
	int i;
	int checksum = 0;
	struct pkt *p = NULL;
	struct node *n;

/* append message to buffer */
/*Step (a)*/
	append_msg(&message);

/* If the last packet have not been ACKed, just drop this message */
/*Step (b)*/
	if (window_isfull()) {
		Debug_Log(A, "Window is full already, save message to extra buffer", NULL, &message);
		return;
	}

/* pop a message from the extra buffer */
/* Step(c)*/
	n = pop_msg();
	if (n == NULL) {
		printf("No message need to process\n");
		return;
	}
/* get a free packet from the buf */
	p = get_free_packet();
	if (p == NULL) {
		Debug_Log(A, "BUG! The window is full already", NULL, &message);
		return;
	}
	++A_from_layer5;
	Debug_Log(A, "Receive an message from layer5", NULL, &message);
/* copy data from msg to pkt */
	for (i = 0; i < 20; i++) {
		p->payload[i] = n->message.data[i];
	}
/* after used, free the node */
	free(n);

/* set current seqnum */
	p->
			seqnum = nextseq;
/* we are send package, do not need to set acknum */
	p->
			acknum = DEFAULT_ACK;

/* calculate check sum including seqnum and acknum */
	checksum = calc_checksum(p);
/* set check sum */
	p->
			checksum = checksum;

/* send pkg to layer 3 */
	tolayer3(A, *p);
	++
			A_to_layer3;

/* we only start the timer for the oldest packet */
	if (base == nextseq) {
		starttimer(A, TIMEOUT);
	}

	++
			nextseq;

	Debug_Log(A, "Send packet to layer3", p, &message);

	printf("================================ Outside A_output===================================\n");
}

void B_output(struct msg message)  /* need be completed only for extra credit */
{
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet) {
	printf("================================ Inside A_input===================================\n");
	Debug_Log(A, "Receive ACK packet from layer3", &packet, NULL);

/* check checksum, if corrupted, do nothing */
	if (packet.checksum !=
	    calc_checksum(&packet)
			) {
		Debug_Log(A, "ACK packet is corrupted", &packet, NULL);
		return;
	}

/* Duplicate ACKs, do nothing */
	if (packet.acknum < base) {
		Debug_Log(A, "Receive duplicate ACK", &packet, NULL);
		return;
	} else if (packet.acknum >= nextseq) {
		Debug_Log(A, "BUG: receive ACK of future packets", &packet, NULL);
		return;
	}

/* go to the next seq, and stop the timer */
	free_packets(packet
			             .acknum);

/* stop timer of the oldest packet, and if there
   is still some packets on network, we need start
   another packet  */
	stoptimer(A);
	if (base != nextseq) {
		starttimer(A, TIMEOUT);
	}

	Debug_Log(A, "ACK packet process successfully accomplished!!", &packet, NULL);
	printf("================================ Outside A_input===================================\n");
}

/* called when A's timer goes off */
void A_timerinterrupt() {
	int i;
	Debug_Log(A, "Time interrupt occur", NULL, NULL);
	/* if current package no finished, we resend it */
	for (i = base; i < nextseq; ++i) {
		struct pkt *p = get_packet(i);
		tolayer3(A, *p);
		++A_to_layer3;
		Debug_Log(A, "Timeout! Send out the package again", p, NULL);
	}

	/* If there is still some packets, start the timer again */
	if (base != nextseq) {
		starttimer(A, TIMEOUT);
	}
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
}


/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/

void B_input(struct pkt packet) {
	printf("================================ Inside B_input===================================\n");
	Debug_Log(B, "Receive a packet from layer3", &packet, NULL);
	++B_from_layer3;

/* check checksum, if corrupted, just drop the package */
	if (packet.checksum != calc_checksum(&packet)
			) {
		Debug_Log(B, "Packet is corrupted", &packet, NULL);
		return;
	}

/* normal package, deliver data to layer5 */
	if (packet.seqnum == B_seqnum) {
		++
				B_seqnum;
		tolayer5(B, packet.payload);
		++
				B_to_layer5;
		Debug_Log(B, "Send packet to layer5", &packet, NULL);
	}
/* duplicate package, do not deliver data again.
   just resend the latest ACK again */
	else if (packet.seqnum < B_seqnum) {
		Debug_Log(B, "Duplicated packet detected", &packet, NULL);
	}
/* disorder packet, discard and resend the latest ACK again */
	else {
		Debug_Log(B, "Disordered packet received", &packet, NULL);
	}

/* send back ack */
	if (B_seqnum - 1 >= 0) {
		packet.
				acknum = B_seqnum - 1;    /* resend the latest ACK */
		packet.
				checksum = calc_checksum(&packet);
		tolayer3(B, packet);
		Debug_Log(B, "Send ACK packet to layer3", &packet, NULL);
	}
	printf("================================ Outside B_input(packet) =========================\n");
}

/* called when B's timer goes off */
void B_timerinterrupt() {
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init() {
}


void printStat() {
	printf("#Sent Packets from Layer5-to   A: %d\n", A_from_layer5);
	printf("#Sent Packets to   Layer3-from A: %d\n", A_to_layer3);
	printf("#Sent Packets from Layer3-to   B: %d\n", B_from_layer3);
	printf("#Sent Packets to   Layer5-from B: %d\n", B_to_layer5);
}

#endif //RDT_GBN_H
