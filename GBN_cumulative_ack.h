//
// Created by Subangkar on 26-Jan-19.
//

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
	int base;// current send base
	int nextseqnum;// current expected seq
	int cnt_layer3;// kept only for stat
	int cnt_layer5;// kept only for stat
	pkt *windowbuffer;// congestion windows buffer
	std::queue<msg> *msgbuffer;// extra buffer
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
/* Multiple timer implemented in cpp with single timer using following functions */

void starttimer(int AorB, float increment);
void startTimer(int AorB, int timerNo, float increment);

void stoptimer(int AorB);
void stopTimer(int AorB, int timerNo = 0);

void resetTimers(int AorB);

void clearTimerFlag(int AorB);
///=========================================================================

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

void A_output(msg);


#define   A    0
#define   B    1

#define TIMEOUT 40.0

#define EXTRA_BUFFER_SIZE 50
#define SEQ_NUM_SIZE 8
#define WINDOW_SIZE 7

std::queue<msg> A_buffer;
pkt A_window[SEQ_NUM_SIZE];


rtp_layer_gbn_t A_rtp, B_rtp;


bool windowIsEmpty(const rtp_layer_gbn_t &rtp_layer) {
	return rtp_layer.nextseqnum == rtp_layer.base;
}

bool windowIsFull(const rtp_layer_gbn_t &rtp_layer) {
	return rtp_layer.nextseqnum == ((rtp_layer.base + WINDOW_SIZE) % SEQ_NUM_SIZE);
}

void free_packets(int acknum) {
	rtp_layer_gbn_t &rtp_layer = A_rtp;
	if (acknum == rtp_layer.nextseqnum)return;// ack num out of window

	int count = (acknum - rtp_layer.base + 1 + SEQ_NUM_SIZE) % SEQ_NUM_SIZE;
	rtp_layer.base = (acknum + 1) % SEQ_NUM_SIZE;

	for (int i = 0; i < count; ++i) stopTimer(A);

	//when the window size decrease, check the
	//extra buffer, if we need any message need to send
	while (count > 0 && !A_rtp.msgbuffer->empty()) {
		msg message = A_rtp.msgbuffer->front();
		A_rtp.msgbuffer->pop();
		A_output(message);
		count--;
	}
}

/* adds a packet at last of the window and also forwards nextseqnum */
void addPacketToWindow(rtp_layer_gbn_t &rtp_layer, pkt &packet) {
	rtp_layer.windowbuffer[rtp_layer.nextseqnum] = packet;
	rtp_layer.nextseqnum = (rtp_layer.nextseqnum + 1) % SEQ_NUM_SIZE;
}

void retransmitCurrentWindow(rtp_layer_gbn_t &rtp_layer) {
	for (int i = rtp_layer.base; i != rtp_layer.nextseqnum; i = (i + 1) % SEQ_NUM_SIZE) {
		tolayer3(A, rtp_layer.windowbuffer[i]);
		startTimer(A, i, TIMEOUT);
		++A_rtp.cnt_layer3;
		printLog(A, const_cast<char *>(">Resend the packet again"), &rtp_layer.windowbuffer[i], NULL);
	}
}
/* called from layer 5, passed the data to be sent to other side */

/* Every time there is a new packet come,
 * a) append this packet at the end of the extra buffer.
 * b) check the window is full or not; If the window is full, we just leave the packet in the extra buffer;
 * c) If the window is not full, retrieve one packet at the beginning of the extra buffer, and process it.*/
void A_output(struct msg message) {
	rtp_layer_gbn_t &rtp_layer = A_rtp;

	printLog(A, const_cast<char *>("Receive an message from layer5"), NULL, &message);
	if (rtp_layer.msgbuffer->size() >= EXTRA_BUFFER_SIZE) {
		++nDroppedMessages;
		printLog(A, const_cast<char *>("Extra Buffer full. Dropping Message!!!"), nullptr, &message);
		return;
	}
	rtp_layer.msgbuffer->push(message);

	/// if windows is full then just push
	if (windowIsFull(A_rtp)) {
		printLog(A, const_cast<char *>("Window is full already, save message to extra buffer"), NULL, &message);
		return;
	}

	// if windows is not full then current message is the only message in buffer
	rtp_layer.msgbuffer->pop();

	++rtp_layer.cnt_layer5;
	printLog(A, const_cast<char *>("Processed an message from layer5"), NULL, &message);


	pkt packet = make_pkt(message, rtp_layer.nextseqnum, ACK_GBN_DEFAULT);
	addPacketToWindow(rtp_layer, packet);//nextseqnum is updated here
	tolayer3(A, packet);
	startTimer(A, rtp_layer.nextseqnum, TIMEOUT);
	++rtp_layer.cnt_layer3;

	printLog(A, const_cast<char *>("Send packet to layer3"), &packet, &message);
}

void B_output(struct msg message) {
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet) {
	rtp_layer_gbn_t &rtp_layer = A_rtp;
	printLog(A, const_cast<char *>("Receive ACK packet from layer3"), &packet, NULL);

	/* Corrupted ACK -> ignore */
	if (packet.checksum != calc_checksum(&packet)) {
		printLog(A, const_cast<char *>("ACK packet is corrupted"), &packet, NULL);
		return;
	}

	/* Duplicate ACK: NACK -> resend all */
	if (packet.acknum == ((rtp_layer.base + WINDOW_SIZE) % SEQ_NUM_SIZE)) {
		printLog(A, const_cast<char *>("Received duplicate ACK"), &packet, NULL);
		resetTimers(A);
		retransmitCurrentWindow(A_rtp);
		return;
	}

	/* go to the next base,seq and stop the timer */
	free_packets(packet.acknum);

	printLog(A, const_cast<char *>("ACK packet processed successfully !!"), &packet, NULL);
}

/* called when A's timer goes off */
void A_timerinterrupt() {
	clearTimerFlag(A);
	resetTimers(A);
	rtp_layer_gbn_t &rtp_layer = A_rtp;
	if (windowIsEmpty(rtp_layer)) {
		perror(">> Problem: timer interrupt occurred with empty window\n");
		return;
	}
	printLog(A, const_cast<char *>("Timer interrupt occurred"), NULL, NULL);
	/* if current packets not ACKed, resend it */
	retransmitCurrentWindow(rtp_layer);
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
bool ackTimerStarted = false;

void B_input(struct pkt packet) {
	rtp_layer_gbn_t &rtp_layer = B_rtp;
	printLog(B, const_cast<char *>("Receive a packet from layer3"), &packet, NULL);
	++rtp_layer.cnt_layer3;

	/* if corrupted, just drop the package */
	if (packet.checksum != calc_checksum(&packet)) {
		printLog(B, const_cast<char *>("Packet is corrupted"), &packet, NULL);
//		return;
	} else if (packet.seqnum == rtp_layer.nextseqnum) {
		/* normal packet, deliver data to layer5 */
		tolayer5(B, packet.payload);
		rtp_layer.nextseqnum = (rtp_layer.nextseqnum + 1) % SEQ_NUM_SIZE;
		++rtp_layer.cnt_layer5;
		printLog(B, const_cast<char *>("Send packet to layer5"), &packet, NULL);
		if (!ackTimerStarted) {
			printLog(B, const_cast<char *>("Cumulative Timer Started!!!"), nullptr, nullptr);
			starttimer(B, static_cast<float>(TIMEOUT * 0.6));
			ackTimerStarted = true;
		}
		return;
	} else {
		/* duplicate packet, resend the latest ACK again */
		printLog(B, const_cast<char *>("Disordered packet received"), &packet, NULL);
	}

	if (ackTimerStarted) {
		stoptimer(B);
		ackTimerStarted = false;
	}
	/* send back ACK with the last received seqnum */
	packet.acknum = (rtp_layer.nextseqnum + SEQ_NUM_SIZE - 1) % SEQ_NUM_SIZE;    /* resend the latest ACK */
	packet.checksum = calc_checksum(&packet);
	tolayer3(B, packet);
	printLog(B, const_cast<char *>("Send NACK packet to layer3"), &packet, NULL);
}

/* called when B's timer goes off */
void B_timerinterrupt() {
	printLog(B, const_cast<char *>("Timer interrupt occurred @B !!!"), nullptr, nullptr);
	pkt packet = make_pkt("ack_packet", 0xFF, (B_rtp.nextseqnum + SEQ_NUM_SIZE - 1) % SEQ_NUM_SIZE);
	ackTimerStarted = false;
	tolayer3(B, packet);
	printLog(B, const_cast<char *>("Send cumulative ACK packet to layer3"), &packet, NULL);
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
			fprintf(fp, "[%c] @%f %s.Window[%d,%d) Message[data=%s..]\n", ch, time, msg, A_rtp.base, A_rtp.nextseqnum,
			        m->data);
		} else {
			fprintf(fp, "[%c] @%f %s.Window[%d,%d)\n", ch, time, msg, A_rtp.base, A_rtp.nextseqnum);
		}
	} else {
		if (p != NULL) {
			fprintf(fp, "[%c] @%f %s.Expected[%d] Packet[seq=%d,ack=%d,check=%d,data=%s..]\n", ch, time, msg,
			        B_rtp.nextseqnum, p->seqnum, p->acknum, p->checksum, p->payload);
		} else if (m != NULL) {
			fprintf(fp, "[%c] @%f %s.Expected[%d] Message[data=%s..]\n", ch, time, msg, B_rtp.nextseqnum, m->data);
		} else {
			fprintf(fp, "[%c] @%f %s.Expected[%d]\n", ch, time, msg, B_rtp.nextseqnum);
		}
	}
}

void printStat() {
	printf("#Sent Messages from Layer5-to   A: %d\n", A_rtp.cnt_layer5);
	printf("#Sent Messages to   Layer3-from A: %d\n", A_rtp.cnt_layer5 - nDroppedMessages);
	printf("#Sent Packets  to   Layer3-from A: %d\n", A_rtp.cnt_layer3);
	printf("#Sent Packets  from Layer3-to   B: %d\n", B_rtp.cnt_layer3);
	printf("#Sent Messages to   Layer5-from B: %d\n", B_rtp.cnt_layer5);
}

#endif //RDT_GBN_H
