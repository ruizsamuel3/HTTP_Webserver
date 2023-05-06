// TCP Library (framework only)
// Jason Losh
//Edited by Samuel Ruiz for IoT
//Implementing web server tcp statemachine
//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: -
// Target uC:       -
// System Clock:    -

// Hardware configuration:
// -

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tcp.h"
#include "timer.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
uint32_t seqNum = 0;
uint32_t ackNum = 0;
uint16_t sourcePort;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Determines whether packet is TCP packet
// Must be an IP packet
bool isTcp(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*) ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*) ((uint8_t*) ip + ipHeaderLength);
    uint32_t sum = 0;
    bool ok;
    uint16_t tmp16;
    ok = (ip->protocol == PROTOCOL_TCP);
    if (ok)
    {
        // 32-bit sum over pseudo-header
        sumIpWords(ip->sourceIp, 8, &sum);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        tmp16 = htons(ntohs(ip->length) - (ip->size * 4));
        sumIpWords(&tmp16, 2, &sum);
        // add tcp header and data
        sumIpWords(tcp, ntohs(ip->length) - (ip->size * 4), &sum);
        ok = (getIpChecksum(sum) == 0);
    }
    return ok;
}

void sendTcpMessage(etherHeader *ether, socket s, uint8_t ipTo[], uint8_t flags,
                    uint8_t tcpState, uint8_t data[], uint16_t dataSize)
{

    uint16_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint8_t *copyData;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader *ip = (ipHeader*) ether->data;
    uint8_t ipHeaderLength = 20;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = ipTo[i]; //s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    //------------------------------------------------------------------
    //TCP Header
    tcpHeader *tcp = (tcpHeader*) ((uint8_t*) ip + (ip->size * 4));
    uint16_t tempPort = tcp->destPort;
    tcp->destPort = tcp->sourcePort;
    tcp->sourcePort = tempPort;

    //Length, datasize will need to be adjusted later
    tcpLength = sizeof(tcpHeader) + dataSize;
    ip->length = htons(ipHeaderLength + tcpLength);
    // 32-bit sum over ip header
    calcIpChecksum(ip);

//    if (tcpState == TCP_LISTEN)
//    { //Syn sends random sequence
//        uint8_t random = rand();
//        tcp->sequenceNumber = htonl(random);
//        tcp->acknowledgementNumber = htonl(0);
//    }
    if (tcpState == TCP_LAST_ACK || tcpState == TCP_FIN_WAIT_1)
    {
        tcp->sequenceNumber = htonl(seqNum);
        tcp->acknowledgementNumber = htonl(ackNum);

    }
    else if (tcpState == TCP_ESTABLISHED)
    {

        tcp->sequenceNumber = htonl(seqNum);
        tcp->acknowledgementNumber = htonl(ackNum);
    }
    else if (tcpState == TCP_SYN_RECEIVED)
    {
        tcp->sequenceNumber = htonl(seqNum);
        tcp->acknowledgementNumber = htonl(ackNum);
    }
    else if (tcpState == TCP_CLOSE_WAIT)
    {
        tcp->sequenceNumber = htonl(seqNum);
        tcp->acknowledgementNumber = htonl(ackNum);
    }
    else
    {
        tcp->sequenceNumber = htonl(seqNum);
        tcp->acknowledgementNumber = htonl(ackNum);

    }
//    seqNum = htonl(tcp->sequenceNumber);
//    ackNum = htonl(tcp->acknowledgementNumber);
    // copy data

    copyData = tcp->data;
    for (i = 0; i < dataSize; i++)
    {
        copyData[i] = data[i];
    }
    //Offset with fields
    tcp->offsetFields = htons(flags | 0x5000);
    tcp->urgentPointer = htons(0); //Always zero for this project I think
    // set tcp window
    tcp->windowSize = htons(65495);
    //----------------------------------------------------------------
    // 32-bit sum over pseudo-header
    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tmp16 = htons(ntohs(ip->length) - (ip->size * 4));
    sumIpWords(&tmp16, 2, &sum);
    // add tcp header and data
    tcp->checksum = 0;
    sumIpWords(tcp, ntohs(ip->length) - (ip->size * 4), &sum);
    tcp->checksum = getIpChecksum(sum);
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}
void getTcpMessageSocket(etherHeader *ether, socket *s)
{
    ipHeader *ip = (ipHeader*) ether->data;
    uint8_t i;
    for (i = 0; i < HW_ADD_LENGTH; i++)
        s->remoteHwAddress[i] = ether->sourceAddress[i];
    for (i = 0; i < IP_ADD_LENGTH; i++)
        s->remoteIpAddress[i] = ip->sourceIp[i];

}
uint8_t tcpGetState(etherHeader *ether, uint8_t state)
{
    ipHeader *ip = (ipHeader*) ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    uint8_t tcpState;
    tcpHeader *tcp = (tcpHeader*) ((uint8_t*) ip + ipHeaderLength);
    uint16_t fields = 0;
    fields = htons(tcp->offsetFields) & 0x001F; //turns off all the bits except the flags and offset

    //Establishing Connection
//    if (fields == (SYN | ACK) & (state == TCP_SYN_SENT))
//    { //Sent syn, received syn ack
//        tcpState = TCP_SYN_RECEIVED;
//        seqNum = htonl(tcp->acknowledgementNumber);
//        ackNum = htonl(tcp->sequenceNumber) + 1;
//    }
//    else if (fields == (PSH | ACK) & (state == TCP_SYN_RECEIVED))
//    { //Sent ack and connect, received psh ack
//        tcpState = TCP_ESTABLISHED;
//        seqNum = htonl(tcp->acknowledgementNumber);
//        ackNum = htonl(tcp->sequenceNumber) + 1;
//    }
//    else if (fields == (ACK) & (state == TCP_SYN_RECEIVED))
//    { //Sent syn, received syn ack
//        tcpState = TCP_ESTABLISHED;
//        seqNum = tcp->acknowledgementNumber;
//        ackNum = tcp->sequenceNumber;
//    }
    //=========
    //==================================================================
    //MQTT Server has requested close
    if (fields == (FIN | ACK) && (state == TCP_ESTABLISHED)
            && (tcp->sourcePort == sourcePort))
    { //received fin ack from server to close
        tcpState = TCP_CLOSE_WAIT;
        seqNum = htonl(tcp->acknowledgementNumber);
        ackNum = htonl(tcp->sequenceNumber) + 1;

    }
    else if (fields == (FIN | ACK | PSH) && (state == TCP_ESTABLISHED)
            && (tcp->sourcePort == sourcePort))
    {
        tcpState = TCP_CLOSE_WAIT;
        seqNum = htonl(tcp->acknowledgementNumber);
        ackNum = htonl(tcp->sequenceNumber) + 1;
    }
    else if (fields == ACK && (state == TCP_CLOSE_WAIT)
            && (tcp->sourcePort == sourcePort))
    {
        tcpState = TCP_LAST_ACK;
        seqNum = htonl(tcp->acknowledgementNumber);
        ackNum = htonl(tcp->sequenceNumber) + 1;
    }
    //===============================================================
    //MQTT ack or push while established
    else if (fields == (PSH | ACK) && (state == TCP_ESTABLISHED)
            && (tcp->sourcePort == sourcePort))
    {
        tcpState = TCP_ESTABLISHED;
        seqNum = htonl(tcp->acknowledgementNumber);
        ackNum = htonl(tcp->sequenceNumber) + htons(ip->length) - 20 - 20;
    }
    else if (fields == (ACK) && (state == TCP_ESTABLISHED)
            && (tcp->sourcePort == sourcePort))
    {
        tcpState = TCP_ESTABLISHED;
        seqNum = htonl(tcp->acknowledgementNumber);
        ackNum = htonl(tcp->sequenceNumber);
    }

    //================================================================
    //Client requested close
//    else if (fields == (FIN | ACK) & (state == TCP_FIN_WAIT_1))
//    {
//        tcpState = TCP_FIN_WAIT_2;
//        seqNum = htonl(tcp->acknowledgementNumber);
//        ackNum = htonl(tcp->sequenceNumber) + 1;
//    }
    //=================================================================
    //SERVER SIDE
    else if ((fields == SYN) && (state == TCP_LISTEN))
    {
        tcpState = TCP_SYN_RECEIVED;
        seqNum = htonl(tcp->acknowledgementNumber);
        ackNum = htonl(tcp->sequenceNumber) + 1;
        sourcePort = tcp->sourcePort;

    }
    else if ((fields == ACK) && (state == TCP_SYN_RECEIVED)
            && (tcp->sourcePort == sourcePort))
    {
        tcpState = TCP_ESTABLISHED;
        seqNum = htonl(tcp->acknowledgementNumber);
        ackNum = htonl(tcp->sequenceNumber) + 1;
    }
    else if ((fields == RST | ACK) && (tcp->sourcePort == sourcePort))
    {
        tcpState = TCP_LISTEN;
    }
    else
    {
        tcpState = state;
    }
    return tcpState;
}
