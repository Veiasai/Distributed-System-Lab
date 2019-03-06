/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_receiver.h"

#include <algorithm>
#include <iostream>

using namespace std;

static const int window_size = 10;
static struct message * window[window_size];
static int cur;
extern volatile int send_cur;

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
    memset(window, 0, sizeof(window));
    cur = 0;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    try{
        unsigned long long checksum = 0;
        struct message *msg;
        /* 1-byte header indicating the size of the payload */
        const int header_size = 8;

        for (int i=0;i<RDT_PKTSIZE;i+=8){
            checksum ^= *(unsigned long long *)(pkt->data+i);
        }
        checksum = (checksum ^ (checksum >> 32));
        checksum = (checksum ^ (checksum >> 16));
        checksum = (checksum ^ (checksum >> 8)) & 0xFF;
        // cout << "RE" << checksum << endl;
        // exit(0);
        if (checksum != 0) {
            //cout << "sum error" << endl; 
            return;
        }

        // check id
        int re_id = *(int*)(pkt->data+4);
        if (re_id >= cur + window_size) { 
            //cout << "RB" << re_id << " " << cur << endl; 
            return; }
        else if (re_id < cur) {
            //cout << "RC" << cur << endl; 
            goto ACK;
            }
        // ensure index
        re_id = re_id % window_size;

        if (window[re_id] != NULL){
            //cout << "redundant" << endl;
            goto ACK;
        }
        /* construct a message and deliver to the upper layer */
        msg = (struct message*) malloc(sizeof(struct message));
        ASSERT(msg!=NULL);

        msg->size = pkt->data[0];

        /* sanity check in case the packet is corrupted */
        if (msg->size<0) msg->size=0;
        if (msg->size>RDT_PKTSIZE-header_size) msg->size=RDT_PKTSIZE-header_size;

        msg->data = (char*) malloc(msg->size);
        ASSERT(msg->data!=NULL);
        memcpy(msg->data, pkt->data+header_size, msg->size);
        window[re_id] = msg;
        while((msg = window[cur % window_size])){
            Receiver_ToUpperLayer(msg);
            window[cur % window_size] = NULL;
            /* don't forget to free the space */
            if (msg->data!=NULL) free(msg->data);
            if (msg!=NULL) free(msg);
            cur++;

            //cout << "receive " << cur << endl;
        }
        
    ACK: // return ack
        packet pkt_ack;
        pkt_ack.data[1] = -1;
        *(int*)(pkt_ack.data+4) = cur - 1;
        *(int*)(pkt_ack.data+8) = cur - 1;
        Receiver_ToLowerLayer(&pkt_ack);
    }catch(exception e){
        return;
    }
}
