//
// Created by Subangkar on 26-Jan-19.//

#include "Utils.h"


#ifndef RDT_GBN_H
#define RDT_GBN_H
#define BIDIRECTIONAL 0    /* change to 1 if you're doing extra credit */
/* and write a routine called B_output */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.*/
struct msg {
	char data[20];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).Note the pre-defined packet structure, which all   */
/* students must follow.*/
struct pkt {
	int seqnum;
	int acknum;
	int checksum;
	char payload[20];
};

/// base-nextseq-cnt3-cnt5-windBuf-msgBuf
struct rtp_layer_gbn_t {
	int base;
	int nextseqnum;/// current expected seq
	int cnt_layer3;
	int cnt_layer5;
	pkt *windowbuffer;
	std::queue<msg> *msgbuffer;
};

/********* FUNCTION PROTOTYPES.DEFINED IN THE LATER PART******************/
void tolayer3(int AorB, struct pkt packet);

void tolayer5(int AorB, char datasent[20]);

void printLog(int AorB, char *msg, const struct pkt *p, struct msg *m);

void printStat();

int calc_checksum(const pkt *p);

pkt make_pkt(msg message, int seqNum, int ackNum = ACK_ABP_DEFAULT);

pkt make_pkt(const char data[MSG_LEN], int seqNum, int ackNum);

///============================ Timer Functions ============================
//#define MAX_TIMERS (WINDOW_SIZE+1)
//bool hasTimerStarted;
//float timersQ[MAX_TIMERS];
//std::queue<std::pair<float, float >> timersQ;// startTime,Timeout

///
void startTimer(int AorB, int timerNo, float increment);

void stopTimer(int AorB, int timerNo = 0);

void resetTimers(int AorB);

void startNextTimer(int AorB);
///=========================================================================

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/



void A_output(msg);


/* common defines */
#define   A    0
#define   B    1
/* timeout for the timer */
#define TIMEOUT 20.0

#define EXTRA_BUFFER_SIZE 50
#define WINDOW_SIZE 7
#define SEQ_NUM_SIZE 8

#define N WINDOW_SIZE

std::queue<msg> A_buffer, B_buffer;
pkt A_window[WINDOW_SIZE], B_window[WINDOW_SIZE];


rtp_layer_gbn_t A_rtp, B_rtp;

/* current expected seq for B */
int B_seqnum = 0;

/* defines for statistic */
int A_from_layer5 = 0;
int A_to_layer3 = 0;
int B_from_layer3 = 0;
int B_to_layer5 = 0;

int base = 0;
int nextseq = 0;
/* ring buffer for all packets in window */
int packets_base = 0;
struct pkt packets[N];

/// ====================== Queue - Implementation =======================
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
/// ====================== Queue - Implementation - END =======================

bool windowIsFull(const rtp_layer_gbn_t &rtp_layer) {
//	return nextseq >= base + N;
	return rtp_layer.nextseqnum == ((rtp_layer.base + WINDOW_SIZE) % SEQ_NUM_SIZE);
}

struct pkt *get_free_packet() {
	/* check is window full */
	if (windowIsFull(A_rtp)) {
		printLog(A, const_cast<char *>("Alloc packet failed.The window is full already"), NULL, NULL);
		return nullptr;
	}

	/* get next free packet */
//	int cur_index = (packets_base + nextseq - base) % N;

	return &(packets[A_rtp.nextseqnum]);
}

struct pkt *get_packet(int seqnum) {
	int cur_index = 0;
	if (seqnum < base || seqnum >= nextseq) {
		printLog(A, const_cast<char *>("Seqnum is not within the window"), NULL, NULL);
		return NULL;
	}

	cur_index = (packets_base + seqnum - base) % N;
	return &(packets[cur_index]);
}

void free_packets(int acknum) {
	if (acknum < base || acknum >= nextseq)return;
	rtp_layer_gbn_t& rtp_layer = A_rtp;

//	int count = acknum - base + 1;
//	packets_base += count;
//	packets_base = packets_base % N;
//	base = (acknum + 1) % SEQ_NUM_SIZE;
	int count = acknum - rtp_layer.base + 1;
	rtp_layer.base = (acknum + 1) % SEQ_NUM_SIZE;

	for (int i = 0; i < count; ++i) stopTimer(A);

	//when the window size decrease, check the
	//extra buffer, if we need any message need to send
	while (count > 0 && !A_rtp.msgbuffer->empty()) {
//		stopTimer(A);
//		struct node *n = pop_msg();
//		if (n == NULL) {
//			break;
//		}
		msg message = A_rtp.msgbuffer->front();
		A_rtp.msgbuffer->pop();
		A_output(message);
//		free(n);
//		if (!A_rtp.msgbuffer->empty()) {
//			tolayer3(A,make_pkt(q.front()))
//		}
		count--;
	}
}

/// also forwards nextseqnum
void addPacketToWindow(rtp_layer_gbn_t &rtp_layer, pkt &packet) {
	rtp_layer.windowbuffer[rtp_layer.nextseqnum] = packet;
	rtp_layer.nextseqnum = (rtp_layer.nextseqnum + 1) % SEQ_NUM_SIZE;
}
/* called from layer 5, passed the data to be sent to other side */

/* Every time there is a new packet come,
 * a) we append this packet at the end of the extra buffer.* b) Then we check the window is full or not; If the window is full, we just leave the packet in the extra buffer;
 * c) If the window is not full, we retrieve one packet at the beginning of the extra buffer, and process it.*/
void A_output(struct msg message) {
	rtp_layer_gbn_t &rtp_layer = A_rtp;
	struct node *n;

	if (rtp_layer.msgbuffer->size() >= EXTRA_BUFFER_SIZE) {
		printLog(A, const_cast<char *>("Extra Buffer full. Dropping Message!!!"), nullptr, &message);
		return;
	}
/*Step (a)*/
	rtp_layer.msgbuffer->push(message);
//	append_msg(&message);

/* If the last packet have not been ACKed, just drop this message */
/*Step (b)*/
/// if windows is full just push then
	if (windowIsFull(A_rtp)) {
		printLog(A, const_cast<char *>("Window is full already, save message to extra buffer"), NULL, &message);
		return;
	}
/// window not full hence queue has only one element
/* Step(c)*/
	if (rtp_layer.msgbuffer->empty()) { // Error
		perror("No message need to process\n");
		return;
	}
//	n = pop_msg();
	rtp_layer.msgbuffer->pop();
/* get a free packet from the buf */
//	pkt *p = get_free_packet();
//	if (p == NULL) {
//		printLog(A, const_cast<char *>("BUG! The window is full already"), NULL, &message);
//		return;
//	}
	++A_from_layer5;
	++rtp_layer.cnt_layer5;
	printLog(A, const_cast<char *>("Receive an message from layer5"), NULL, &message);
//	strncpy(p->payload, message.data, MSG_LEN);
//	free(n);

//	p->seqnum = nextseq;
//	p->acknum = ACK_GBN_DEFAULT;
//	p->checksum = calc_checksum(p);

//	rtp_layer.msgbuffer->pop();
//	if (base == nextseq) {
//		starttimer(A, TIMEOUT);
//	}

	pkt packet = make_pkt(message, rtp_layer.nextseqnum, ACK_GBN_DEFAULT);
	++nextseq;
	addPacketToWindow(rtp_layer, packet);
	tolayer3(A, packet);
	startTimer(A, rtp_layer.nextseqnum, TIMEOUT);
	++A_to_layer3;
	++rtp_layer.cnt_layer3;

	printLog(A, const_cast<char *>("Send packet to layer3"), &packet, &message);
}

void B_output(struct msg message)  /* need be completed only for extra credit */
{
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet) {
	printLog(A, const_cast<char *>("Receive ACK packet from layer3"), &packet, NULL);

/* check checksum, if corrupted, do nothing */
	if (packet.checksum != calc_checksum(&packet)) {
		printLog(A, const_cast<char *>("ACK packet is corrupted"), &packet, NULL);
		return;
	}

/* Duplicate ACKs, do nothing */
	if (packet.acknum < base) {
		printLog(A, const_cast<char *>("Receive duplicate ACK"), &packet, NULL);
		return;
	} else if (packet.acknum >= nextseq) {
		printLog(A, const_cast<char *>("BUG: receive ACK of future packets"), &packet, NULL);
		return;
	}

/* go to the next seq, and stop the timer */
	free_packets(packet.acknum);

/* stop timer of the oldest packet, and if there
   is still some packets on network, we need start
   another packet  */
//	stopTimer(A);
//	stoptimer(A);
//	if (base != nextseq) {
//		starttimer(A, TIMEOUT);
//	}

	printLog(A, const_cast<char *>("ACK packet process successfully accomplished!!"), &packet, NULL);
//	printf("================================ Outside A_input===================================\n");
}

/* called when A's timer goes off */
void A_timerinterrupt() {
	resetTimers(A);
	rtp_layer_gbn_t &rtp_layer = A_rtp;
	printLog(A, const_cast<char *>("Time interrupt occur"), NULL, NULL);
	/* if current package no finished, we resend it */
	for (int i = rtp_layer.base; i < rtp_layer.nextseqnum; i = (i + 1) % SEQ_NUM_SIZE) {
//		struct pkt *p = get_packet(i);
//		tolayer3(A, *p);
		tolayer3(A, rtp_layer.windowbuffer[i]);
		startTimer(A, i, TIMEOUT);
		++A_to_layer3;
		++A_rtp.cnt_layer3;
		printLog(A, const_cast<char *>("Timeout! Send out the package again"), &rtp_layer.windowbuffer[i], NULL);
	}

	/* If there is still some packets, start the timer again */
//	if (base != nextseq) {
//		starttimer(A, TIMEOUT);
//	}
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called.You can use it to do any initialization */
void A_init() {
	A_rtp = {0, 0, 0, 0, nullptr, nullptr};
	A_rtp.msgbuffer = &A_buffer;
	A_rtp.windowbuffer = A_window;
}


/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/

void B_input(struct pkt packet) {
//	printf("================================ Inside B_input===================================\n");
	printLog(B, const_cast<char *>("Receive a packet from layer3"), &packet, NULL);
	++B_from_layer3;

/* check checksum, if corrupted, just drop the package */
	if (packet.checksum != calc_checksum(&packet)) {
		printLog(B, const_cast<char *>("Packet is corrupted"), &packet, NULL);
		return;
	}

/* normal package, deliver data to layer5 */
	if (packet.seqnum == B_seqnum) {
		++B_seqnum;
		tolayer5(B, packet.payload);
		++B_to_layer5;
		printLog(B, const_cast<char *>("Send packet to layer5"), &packet, NULL);
	}
/* duplicate package, do not deliver data again.just resend the latest ACK again */
	else if (packet.seqnum < B_seqnum) {
		printLog(B, const_cast<char *>("Duplicated packet detected"), &packet, NULL);
	}
/* disorder packet, discard and resend the latest ACK again */
	else {
		printLog(B, const_cast<char *>("Disordered packet received"), &packet, NULL);
	}

/* send back ack */
	if (B_seqnum - 1 >= 0) {
		packet.acknum = B_seqnum - 1;    /* resend the latest ACK */
		packet.checksum = calc_checksum(&packet);
		tolayer3(B, packet);
		printLog(B, const_cast<char *>("Send ACK packet to layer3"), &packet, NULL);
	}
//	printf("================================ Outside B_input(packet) =========================\n");
}

/* called when B's timer goes off */
void B_timerinterrupt() {
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called.You can use it to do any initialization */
void B_init() {
	B_rtp = {0, 0, 0, 0, nullptr, nullptr};
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

//===================== Log Files ==================
void writeLog(FILE *fp, int AorB, char *msg, const struct pkt *p, struct msg *m, float time) {
	char ch = (AorB == A) ? 'A' : 'B';
	if (AorB == A) {
		if (p != NULL) {
			fprintf(fp, "[%c] @%f %s.Window[%d,%d) Packet[seq=%d,ack=%d,check=%d,data=%s..]\n", ch, time, msg,
			        A_rtp.base, A_rtp.nextseqnum, p->seqnum, p->acknum, p->checksum, p->payload);
		} else if (m != NULL) {
			fprintf(fp, "[%c] @%f %s.Window[%d,%d) Message[data=%s..]\n", ch, time, msg, A_rtp.base, A_rtp.nextseqnum, m->data);
		} else {
			fprintf(fp, "[%c] @%f %s.Window[%d,%d)\n", ch, time, msg, A_rtp.base, A_rtp.nextseqnum);
		}
	} else {
		if (p != NULL) {
			fprintf(fp, "[%c] @%f %s.Expected[%d] Packet[seq=%d,ack=%d,check=%d,data=%s..]\n", ch, time, msg,
			        B_seqnum, p->seqnum, p->acknum, p->checksum, p->payload);
		} else if (m != NULL) {
			fprintf(fp, "[%c] @%f %s.Expected[%d] Message[data=%s..]\n", ch, time, msg, B_seqnum, m->data);
		} else {
			fprintf(fp, "[%c] @%f %s.Expected[%d]\n", ch, time, msg, B_seqnum);
		}
	}
}

void printStat() {
//	printf("#Sent Packets from Layer5-to   A: %d\n", A_from_layer5);
//	printf("#Sent Packets to   Layer3-from A: %d\n", A_to_layer3);
	printf("#Sent Packets from Layer5-to   A: %d\n", A_rtp.cnt_layer5);
	printf("#Sent Packets to   Layer3-from A: %d\n", A_rtp.cnt_layer3);
	printf("#Sent Packets from Layer3-to   B: %d\n", B_from_layer3);
	printf("#Sent Packets to   Layer5-from B: %d\n", B_to_layer5);
}

#endif //RDT_GBN_H
