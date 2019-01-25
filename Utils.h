//
// Created by Subangkar on 25-Jan-19.
//

#ifndef RDT_UTILS_H
#define RDT_UTILS_H

#define DEBUG_ABP
#define DEBUG_GBN

#define DEBUG_ABP_NMSG 10
#define DEBUG_ABP_PROB_LOSS 0.1
#define DEBUG_ABP_PROB_CORP 0.0
#define DEBUG_ABP_TIME 100
#define DEBUG_ABP_TRACE 0

//#include <cstdio>

///======================= Data Defn ===================

#define MSG_LEN 20
#define BUF_SIZE 7

struct msg;
struct pkt;

#define ACK_ABP_DEFAULT 0x0F
#define SEQ_ABP_DEFAULT 0x0F

enum sender_state {WAITING_FOR_PKT,WAITING_FOR_ACK};

struct rtp_layer_t{
	int seqnum;/// current expected seq
	sender_state senderState;
	int cnt_layer3;
	int cnt_layer5;
	pkt* buffer;
};
///=============================================================

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
 * There is no perfect method for checksum, only suitable method.
 * In this assignment, we only need to add all of them as it is simple and it works well.
*/
/// simple sum of all
int calc_checksum(int seqnum, int acknum, const char payload[MSG_LEN]) {
	int i;
	int checksum = 0;
	for (i = 0; i < MSG_LEN; i++) { // check till NULL
		checksum += (unsigned char) payload[i];
	}
	checksum += seqnum;
	checksum += acknum;
	return checksum;
}

void printLog(char chan, char *msg, int seqnum, int acknum, int checksum,const char payload[MSG_LEN], const char *data) {
	static FILE *fp = fopen("out.log", "w");

	fprintf(fp, "[%c] %s. Packet[seq=%d,ack=%d,check=%d,data=%c..]\n", chan,
	        msg, seqnum, acknum, checksum, payload[0]);
	if (data != NULL) {
		fprintf(fp, "[%c] %s. Message[data=%c..]\n", chan, msg, data[0]);
	} else {
		fprintf(fp, "[%c] %s.\n", chan, msg);
	}

}

#endif //RDT_UTILS_H
