/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
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
#include <vector>
#include <iostream>

#include "rdt_struct.h"
#include "rdt_sender.h"

using namespace std;

static int id;
static vector<packet> window;
volatile int send_cur;

static const int header_size = 8;

void generate_check_sum(char * data){
    unsigned long long checksum;
    checksum = 0;
    data[1] = 0;
    for (int i=0;i<RDT_PKTSIZE;i+=8){
        checksum ^= *(unsigned long long *)(data+i);
    }
    checksum = checksum ^ (checksum >> 32);
    checksum = checksum ^ (checksum >> 16);
    checksum = checksum ^ (checksum >> 8);

    //cout << "GE" << checksum << endl;
    data[1] = checksum & 0xFF;
    //cout << "GED" << (int) data[1] << endl;
}
/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    window.clear();
    id = 0;
    send_cur = 0;
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    /* 1-byte header indicating the size of the payload */
    

    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - header_size;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size-cursor > maxpayload_size) {
        /* fill in the packet */
        pkt.data[0] = maxpayload_size;
        *(int *)(pkt.data+4) = id++;
        
        memcpy(pkt.data+header_size, msg->data+cursor, maxpayload_size);
        
        generate_check_sum(pkt.data);
        /* send it out through the lower layer */

        if (id - send_cur < 10)
            Sender_ToLowerLayer(&pkt);
        window.push_back(std::move(pkt));
        /* move the cursor */
        cursor += maxpayload_size;
    }

    /* send out the last packet */
    if (msg->size > cursor) {
        /* fill in the packet */
        pkt.data[0] = msg->size-cursor;
        *(int *)(pkt.data+4) = id++;
        
        memcpy(pkt.data+header_size, msg->data+cursor, pkt.data[0]);

        generate_check_sum(pkt.data);
        /* send it out through the lower layer */
        if (id - send_cur < 10) 
            Sender_ToLowerLayer(&pkt);
        window.push_back(std::move(pkt));
    }
    if (!Sender_isTimerSet())
            Sender_StartTimer(0.2);
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    // check ack
    int re_id = *(int*)(pkt->data+4);
    int check = *(int*)(pkt->data+8);
    if (re_id != check) return;

    if (pkt->data[1] != 0 && re_id >= send_cur){
        send_cur = re_id + 1;
        for (int i=send_cur;i<window.size() && i<send_cur+2;i++){
            Sender_ToLowerLayer(&window[send_cur]);
        }
    }

    Sender_StartTimer(0.3);
    
    //cout << "SF " << send_cur << " " << id << endl;
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    if (send_cur == id)
        return;
    //cout << "Timeout" << send_cur << " " << id << " "<< window.size()<< endl;
    Sender_StartTimer(0.3);
    Sender_ToLowerLayer(&window[send_cur]);
    
}
