//
// Created by Subangkar on 26-Jan-19.
//

#ifndef RDT_ABP_H
#define RDT_ABP_H

#include "Utils.h"

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

enum sender_state {
	WAITING_FOR_PKT, WAITING_FOR_ACK
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
		++nDroppedMessages;
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
	int acknum = rtp_layer.seqnum;

	/* normal packet, deliver data to layer5 */
	if (packet.seqnum == rtp_layer.seqnum && packet.checksum == calc_checksum(&packet)) {
		rtp_layer.seqnum = (rtp_layer.seqnum + 1) % 2;
		tolayer5(B, packet.payload);
		++rtp_layer.cnt_layer5;
		printLog(B, const_cast<char *>("Sent packet to layer5"), &packet, NULL);
	} else if (packet.checksum != calc_checksum(&packet)) {/* if corrupted send NACK */
		printLog(B, const_cast<char *>("Data Packet is corrupted"), &packet, NULL);
		acknum = !rtp_layer.seqnum;
		pkt ack = make_pkt(packet.payload, packet.seqnum, acknum);// send packet seq no as it is received
		tolayer3(B, ack); // nextseqnum doesn't have any meaning here
		printLog(B, const_cast<char *>("Send NACK packet to layer3"), &ack, NULL);
		return;
	} else if (packet.seqnum != rtp_layer.seqnum) {/* duplicate pkt resends the ACK*/
		printLog(B, const_cast<char *>("Duplicated packet detected"), &packet, NULL);
	}
	pkt ack = make_pkt(packet.payload, packet.seqnum, acknum);// send packet seq no as it is received
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


void writeLog(FILE *fp, int AorB, char *msg, const struct pkt *p, struct msg *m, float time) {
	char ch = (AorB == A) ? 'A' : 'B';
	if (p != NULL) {
		fprintf(fp, "[%c] @%f %s. Packet[seq=%d,ack=%d,check=%d,data=%s..]\n", ch, time,
		        msg, p->seqnum, p->acknum, p->checksum, p->payload);
	} else if (m != NULL) {
		fprintf(fp, "[%c] @%f %s. Message[data=%s..]\n", ch, time, msg, m->data);
	} else {
		fprintf(fp, "[%c] @%f %s.\n", ch, time, msg);
	}
}


void printStat() {
	printf("#Sent Messages from Layer5-to   A: %d\n", A_rtp.cnt_layer5);
	printf("#Sent Messages to   Layer3-from A: %d\n", A_rtp.cnt_layer5 - nDroppedMessages);
	printf("#Sent Packets  to   Layer3-from A: %d\n", A_rtp.cnt_layer3);
	printf("#Sent Packets  from Layer3-to   B: %d\n", B_rtp.cnt_layer3);
	printf("#Sent Messages to   Layer5-from B: %d\n", B_rtp.cnt_layer5);
}

#endif //RDT_ABP_H
