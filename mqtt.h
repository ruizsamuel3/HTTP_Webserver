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

#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include "tcp.h"

#define CONNECT 1
#define CONNACK 2
#define PUBLISH 3
#define PUBACK 4
#define PUBREC 5
#define PUBREL 6
#define PUBCOMP 7
#define SUBSCRIBE 8
#define SUBACK 9
#define UNSUBSCRIBE  10
#define UNSUBACK 11
#define PINGREQ 12
#define PINGRESP 13
#define DISCONNECT 14

//Connect Flags
#define CONNECT_CLEAN       0x00000010
#define CONNECT_WILL_FLAG   0x00000100
#define CONNECT_WILL_QOS    0x00011000
#define CONNECT_WILL_RETAIN 0x00100000
#define CONNECT_PASSWORD    0x01000000
#define CONNECT_USERNAME    0x10000000

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
void createMqttMsg(uint8_t mqttType,uint8_t *mqttData, char* topic, char* payload);
uint8_t getMqttMsgType(etherHeader *ether);
char* getMqttPubData(etherHeader *ether);
#endif

