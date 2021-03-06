//
// Created by Subangkar on 25-Jan-19.
//

#ifndef RDT_UTILS_H
#define RDT_UTILS_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdio>


//#define DEBUG
//#define DEBUG_ABP
//#define DEBUG_GBN

#define DEBUG_NMSG 20
#define DEBUG_PROB_LOSS 0.20//0.05
#define DEBUG_PROB_CORP 0.20//0.45
#define DEBUG_TIME 100
#define DEBUG_TRACE 2

///======================= Data Defn ===================

#define MSG_LEN 20


#define ACK_ABP_DEFAULT 0x0F
#define SEQ_ABP_DEFAULT 0x0F
#define ACK_GBN_DEFAULT 0x0F

int nDroppedMessages;// keeps stat of number of dropped messages from layer 5

///=============================================================

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

#endif //RDT_UTILS_H
