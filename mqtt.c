// MQTT Library (framework only)
// Jason Losh

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
#include <string.h>
#include "mqtt.h"
#include "timer.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
void createMqttMsg(uint8_t mqttType, uint8_t *mqttData, char *topic,
                   char *payload)
{
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t k = 0;
    uint8_t topicLength = 0;
    uint8_t payloadLength = 0;
    uint8_t remainingLength = 0;

    switch (mqttType)
    {
    case CONNECT:
        //Transfer data to uint8_t array for data
        //This should make it possible to use copydata on tcp payload

        //Control Header 11 bytes + with no will + client id bytes
        mqttData[0] = 0x10; //packet type and flags
        mqttData[1] = 0x11; //remaining length - 2

        //Variable Header
        mqttData[2] = 0x00; //Protocol Length MSB
        mqttData[3] = 0x04; //Protocol Length LSB
        mqttData[4] = 'M'; //
        mqttData[5] = 'Q'; //
        mqttData[6] = 'T'; //
        mqttData[7] = 'T'; //
        mqttData[8] = 0x04; //Protocol Level
        mqttData[9] = 0x02; //Connect Flags
        mqttData[10] = 0x00; //Keep Alive MSB
        mqttData[11] = 0xFF; //Keep Alive LSB

        //Payload
        mqttData[12] = 0x00; //Client id length
        mqttData[13] = 0x05; //Client id length
        mqttData[14] = 'S'; //client id
        mqttData[15] = 'A'; //client id
        mqttData[16] = 'M'; //client id
        mqttData[17] = 'P'; //client id
        mqttData[18] = 'C'; //client id

        break;
    case DISCONNECT:
        //Control Header | 2 bytes
        mqttData[0] = 0xE0; //Disconnect
        mqttData[1] = 0x00;
        break;
    case PUBLISH:
        topicLength = strlen(topic);
        payloadLength = strlen(payload);
        remainingLength = topicLength + payloadLength + 2;

        //Control Header | 4 bytes with no topic or data included
        mqttData[0] = 0x30;
        mqttData[1] = remainingLength; //Remaining Length
        //Variable Header
        mqttData[2] = 0x00; //Topic Length Msb
        mqttData[3] = topicLength; //Topic Length lsb
        for (i = 4; i < (topicLength + 4); i++)
        {
            mqttData[i] = topic[i - 4];
        }
        //Payload
        k = 0;
        for (j = i; j < (payloadLength + i); j++)
        {
            mqttData[j] = payload[k];
            k++;
        }
        break;
    case SUBSCRIBE:
        //(1byte type/flags) + (1-4byte remaining length)+
        //(2byte packet ID)+ (2byte topic length) + (n-byte topic name)
        //6 byte + n-byte name

        topicLength = strlen(topic);
        remainingLength = topicLength + 5;
        //Control Header
        mqttData[0] = 0x82; //subscribe
        mqttData[1] = remainingLength; //remaining length
        //Variable Header
        mqttData[2] = 0x00; //Packet ID MSB
        mqttData[3] = 0x12; //Packet ID 0x12 for testing
        //Payload
        mqttData[4] = 0x00; //Topic length MSB
        mqttData[5] = topicLength; //Topic Length LSB
        for (i = 6; i < (topicLength + 6); i++)
        {
            mqttData[i] = topic[i - 6];
        }
        mqttData[i] = 0x00; //QOS
        break;
    case UNSUBSCRIBE:
        topicLength = strlen(topic);
        remainingLength = topicLength + 4;
        //Control Header
        mqttData[0] = 0xA2; //subscribe
        mqttData[1] = remainingLength; //remaining length
        //Variable Header
        mqttData[2] = 0x00; //Packet ID MSB
        mqttData[3] = 0x12; //Packet ID 0x12 for testing
        //Payload
        mqttData[4] = 0x00; //Topic length MSB
        mqttData[5] = topicLength; //Topic Length LSB
        for (i = 6; i < (topicLength + 6); i++)
        {
            mqttData[i] = topic[i - 6];
        }
        break;

    }
}
uint8_t getMqttMsgType(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*) ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*) ((uint8_t*) ip + ipHeaderLength);
    uint8_t *type = tcp->data;
    return type[0];
}
char* getMqttPubData(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*) ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*) ((uint8_t*) ip + ipHeaderLength);

    uint8_t i;

    uint8_t remainingLength = tcp->data[1];
    uint8_t topicLength = tcp->data[3];



    char* data;
    for(i = 0; i < remainingLength+2; i++){
        data[i] = tcp->data[i+4+topicLength];
    }
    return data;

}
