// Ethernet Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "eeprom.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "timer.h"
#include "eth0.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "mqtt.h"
#include "timer_wireless.h"
#include "spi1.h"
#include "wireless.h"
#include "http.h"

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4

// EEPROM Map
#define EEPROM_DHCP        1
#define EEPROM_IP          2
#define EEPROM_SUBNET_MASK 3
#define EEPROM_GATEWAY     4
#define EEPROM_DNS         5
#define EEPROM_TIME        6
#define EEPROM_MQTT        7
#define EEPROM_ERASED      0xFFFFFFFF

//Addresses and Ports
uint8_t ipFrom[] = { 192, 168, 1, 115 };

uint8_t ipTo[] = { 192, 168, 1, 1 }; //Laptop
uint16_t transmitCounter = 0;
//uint8_t ipTo[] = { 192, 168, 1, 238 }; //UTA Switch
//uint8_t ipTo[] = { 169, 254, 223, 113 }; //Rpi
#define localPort 6765
#define remotePort 1883

uint8_t tcpState;
bool connected = false;
bool closeConnection = false;
// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522
uint8_t *udpData;
uint8_t ebuffer[MAX_PACKET_SIZE];
etherHeader *datat = (etherHeader*) ebuffer;
socket s;

//MQTT
uint8_t mqttType;

//Wireless
bool isBridge = false;
bool nrfSyncEnabled = false;
bool nrfJoinEnabled = false;
bool nrfJoinEnabled_BR = false;
uint8_t nrfJoinCount;

uint8_t syncMsg[7] = { 0x55, 0xAA, 0x55, 0xAA, 0x55 };
uint8_t startCode[2] = { 0xFE, 0xFE };
uint8_t myMac[6] = { 75, 78, 1, 2, 3, 74 };

uint8_t slotNo = 0;
uint16_t syncFrameCount = 0;
uint16_t packetLength = 0;

uint8_t allocatedDevNum = 0;
uint32_t lastMsgDevNo_br = 0;

uint8_t Rxpacket[DATA_MAX_SIZE] = { 0 };
uint16_t Rx_index = 0;                               // read index
uint16_t Rx_wrIndex = 0;                             // write index
uint8_t payloadlength = 32;

//*************CTX RX Structs and flags***************
uint8_t buffer[MAX_WIRELESS_PACKET_SIZE] = { 0 };
bool dataReceivedFlag = false;
//BR flags
bool sendPingRequestFlag = false;
bool sendDevCapsRequestFlag = false;
//Device Flags
bool sendPingResponseFlag = false;
bool sendDevCapsResponseFlag = false;
//Flags Shared by BR and Device
bool sendPushFlag = false;
#define MAX_PACKCET_SIZE 22
//*******************************************************

//Packet Type flags
bool isSync = false;
bool isMyPacket = false;
bool msgAcked = false;
bool isDevPacket = false;
bool isJoinResp_dev = false;
bool isBridgePacket = false;
bool isJoinPacket = false;

//Timer flags
bool downlinkSlotStart_br = false;
bool fastackSlotStart_br = false;
bool acessSlotStart_br = false;
bool uplinkSlot_br = false;
bool sendJoinResponse_BR = false;

bool timersStarted_br = false;

bool acessSlotStart_dev = false;
bool uplinkSlot_dev = false;
bool deviceJoined = false;


//Transmit status flags
bool txTimeStatus = false; // Its okay for you to transmit
bool appSendFlag = false; // Flag for the application layer to send the data. Make it true to send data.
//----------------------------------------------------
//Debug
//----------------------------------------------------

uint8_t fifoStatus = 0;
bool debugMsg = false;
//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    //    enablePort(PORTF);
    //    _delay_cycles(3);
    //
    //    // Configure LED and pushbutton pins
    //    selectPinPushPullOutput(RED_LED);
    //    selectPinPushPullOutput(GREEN_LED);
    //    selectPinPushPullOutput(BLUE_LED);
    //    selectPinDigitalInput(PUSH_BUTTON);
    //    enablePinPullup(PUSH_BUTTON);

}

void displayConnectionInfo()
{
    uint8_t i;
    char str[10];
    uint8_t mac[6];
    uint8_t ip[4];
    getEtherMacAddress(mac);
    putsUart0("  HW:    ");
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%02"PRIu8, mac[i]);
        putsUart0(str);
        if (i < HW_ADD_LENGTH - 1)
            putcUart0(':');
    }
    putcUart0('\n');
    getIpAddress(ip);
    putsUart0("  IP:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putsUart0(" (static)");
    putcUart0('\n');
    getIpSubnetMask(ip);
    putsUart0("  SN:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpGatewayAddress(ip);
    putsUart0("  GW:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpDnsAddress(ip);
    putsUart0("  DNS:   ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpTimeServerAddress(ip);
    putsUart0("  Time:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpMqttBrokerAddress(ip);
    putsUart0("  MQTT:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    if (isEtherLinkUp())
        putsUart0("Link is up\n");
    else
        putsUart0("Link is down\n");
}

void readConfiguration()
{
    uint32_t temp;
    uint8_t *ip;

    temp = readEeprom(EEPROM_IP);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*) &temp;
        setIpAddress(ip);
    }
    temp = readEeprom(EEPROM_SUBNET_MASK);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*) &temp;
        setIpSubnetMask(ip);
    }
    temp = readEeprom(EEPROM_GATEWAY);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*) &temp;
        setIpGatewayAddress(ip);
    }
    temp = readEeprom(EEPROM_DNS);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*) &temp;
        setIpDnsAddress(ip);
    }
    temp = readEeprom(EEPROM_TIME);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*) &temp;
        setIpTimeServerAddress(ip);
    }
    temp = readEeprom(EEPROM_MQTT);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t*) &temp;
        setIpMqttBrokerAddress(ip);
    }
}

#define MAX_CHARS 80
char strInput[MAX_CHARS + 1];
char *token;
uint8_t count = 0;
//MQTT
char *topic;
char *payloadMsg;
uint8_t asciiToUint8(const char str[])
{
    uint8_t datat;
    if (str[0] == '0' && tolower(str[1]) == 'x')
        sscanf(str, "%hhx", &datat);
    else
        sscanf(str, "%hhu", &datat);
    return datat;
}

void processShell(wirelessPacket *message)
{
    bool end;
    char c;
    uint8_t i;
    uint8_t ip[IP_ADD_LENGTH];
    uint32_t *p32;

    if (kbhitUart0())
    {
        c = getcUart0();

        end = (c == 13) || (count == MAX_CHARS);
        if (!end)
        {
            if ((c == 8 || c == 127) && count > 0)
                count--;
            if (c >= ' ' && c < 127)
                strInput[count++] = c;
        }
        else
        {
            strInput[count] = '\0';
            count = 0;
            token = strtok(strInput, " ");
            if (strcmp(token, "ifconfig") == 0)
            {
                displayConnectionInfo();
            }
            if (strcmp(token, "reboot") == 0)
            {
                NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            }
            if (strcmp(token, "set") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "ip") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpAddress(ip);
                    p32 = (uint32_t*) ip;
                    writeEeprom(EEPROM_IP, *p32);
                }
                if (strcmp(token, "sn") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpSubnetMask(ip);
                    p32 = (uint32_t*) ip;
                    writeEeprom(EEPROM_SUBNET_MASK, *p32);
                }
                if (strcmp(token, "gw") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpGatewayAddress(ip);
                    p32 = (uint32_t*) ip;
                    writeEeprom(EEPROM_GATEWAY, *p32);
                }
                if (strcmp(token, "dns") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpDnsAddress(ip);
                    p32 = (uint32_t*) ip;
                    writeEeprom(EEPROM_DNS, *p32);
                }
                if (strcmp(token, "time") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpTimeServerAddress(ip);
                    p32 = (uint32_t*) ip;
                    writeEeprom(EEPROM_TIME, *p32);
                }
                if (strcmp(token, "mqtt") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpMqttBrokerAddress(ip);
                    p32 = (uint32_t*) ip;
                    writeEeprom(EEPROM_MQTT, *p32);
                }
            }

            if (strcmp(token, "help") == 0)
            {
                putsUart0("Commands:\r");
                putsUart0("  ifconfig\r");
                putsUart0("  reboot\r");
                putsUart0("  set ip|gw|dns|time|mqtt|sn w.x.y.z\r");
                putsUart0("push\n");
                putsUart0("ping\n");
                putsUart0("devCaps\n");
                putsUart0("reboot\n");
                putsUart0("debug on/off\n");
                putsUart0("reset inteeprom\n");
            }

            if (strcmp(token, "connect") == 0)
            {
                putsUart0("Connection...");
                sendTcpMessage(datat, s, ipTo, SYN, tcpState, NULL, 0);
                tcpState = TCP_SYN_SENT;
            }
            if (strcmp(token, "disconnect") == 0)
            {
                uint8_t tcpPayload[2];
                createMqttMsg(DISCONNECT, tcpPayload, NULL, NULL);
                sendTcpMessage(datat, s, ipTo, (PSH | ACK), tcpState, tcpPayload,
                               2);
            }
            if (strcmp(token, "subscribe") == 0)
            {
                topic = strtok(NULL, " ");
                if (strcmp(topic, '\0') != 0)
                {
                    uint8_t payloadSize = strlen(topic);
                    uint8_t tcpPayload[100];
                    createMqttMsg(SUBSCRIBE, tcpPayload, topic, NULL);
                    sendTcpMessage(datat, s, ipTo, (PSH | ACK), tcpState,
                                   tcpPayload, 7 + payloadSize);
                }
            }
            if (strcmp(token, "unsubscribe") == 0)
            {
                topic = strtok(NULL, " ");
                if (strcmp(topic, '\0') != 0)
                {
                    uint8_t payloadSize = strlen(topic);
                    uint8_t tcpPayload[100];
                    createMqttMsg(UNSUBSCRIBE, tcpPayload, topic, NULL);
                    sendTcpMessage(datat, s, ipTo, (PSH | ACK), tcpState,
                                   tcpPayload, 6 + payloadSize);
                }
            }
            if (strcmp(token, "publish") == 0)
            {
                topic = strtok(NULL, " ");
                payloadMsg = strtok(NULL, " ");
                //Makes sure only publishes with topic and data go through
                if (strcmp(topic, '\0') != 0)
                {
                    if (strcmp(payloadMsg, '\0') != 0)
                    {
                        uint8_t payloadSize = strlen(topic) + strlen(payloadMsg)
                                                                                                + 4;
                        uint8_t tcpPayload[100];
                        createMqttMsg(PUBLISH, tcpPayload, topic, payloadMsg);
                        sendTcpMessage(datat, s, ipTo, (PSH | ACK), tcpState,
                                       tcpPayload, payloadSize); //Is the default size with no topic or payload
                    }
                }
            }
            if (strcmp(token, "push") == 0)
            {
                message = (wirelessPacket*) buffer;
                message->packetType = PUSH;
                sendPushFlag = true;
                appSendFlag = true;
            }
            if (strcmp(token, "ping") == 0)
            {
                message = (wirelessPacket*) buffer;
                message->packetType = PING_REQUEST;
                sendPingRequestFlag = true;
                appSendFlag = true;
            }

            if (strcmp(token, "devCaps") == 0)
            {
                message = (wirelessPacket*) buffer;
                message->packetType = DEVCAPS_REQUEST;
                sendDevCapsRequestFlag = true;
                appSendFlag = true;
            }

            if (strcmp(token, "reboot") == 0)
            {
                NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            }

            if (strcmp(token, "debug") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "on") == 0)
                {
                    debugMsg = true;
                }
                else if (strcmp(token, "off") == 0)
                {
                    debugMsg = false;
                }
                else
                {
                    debugMsg = false;
                }
            }

            if (strcmp(token, "reset") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "inteeprom") == 0)
                {
                    for (i = 0; i < 200; i++)
                    {
                        writeEeprom(i + NO_OF_DEV_IN_BRIDGE, 0x00); // Flash the eeprom

                    }
                    putsUart0("Cleared Internal Eeprom until 200 bytes\n");
                }
            }

        }
    }
}

void nrf24l0Init()
{

    // Assume that the clocks are initialized already

    enablePort(PORTF);
    selectPinPushPullOutput(SYNC_LED);                      //RED LED
    selectPinPushPullOutput(JOIN_LED);                      //BLUE LED
    selectPinDigitalInput(JOIN_BUTTON);
    enablePinPullup(JOIN_BUTTON);

    enablePort(PORTB);                                   // used for Chip enable
    selectPinPushPullOutput(CE_GPIO);

    setPinValue(CE_GPIO, 1); // init always in Rx mode so that isnrfDataAvailable checks for available data
    waitMicrosecond(TX_RX_DELAY);

    // SPI1 Initialization
    initSpi1(USE_SSI_RX);
    setSpi1BaudRate(1e6, 40e6); // 1Mhz baud
    setSpi1Mode(0, 0);
    selectPinPushPullOutput(SSI1FSS);

    //Initialize nrf chip
    nrf24l0ChipInit();

    initTimer_ms();

    setPinValue(JOIN_LED, 1);                               // Init Flash
    waitMicrosecond(100000);
    setPinValue(JOIN_LED, 0);
    waitMicrosecond(100000);

}
void nrf24l0ChipInit()
{
    uint8_t data = 0;

    //configure the module
    setPinValue(CE_GPIO, 0);
    disableNrfCs();

    waitMicrosecond(5000);                  // Wait for the nrf module to settle

    data = 0x0C;                        // Interrupts off,Enable CRC, 2 byte CRC
    writeNrfReg(W_REGISTER | CONFIG, data);

    data = 0x00; // Turn off retransmission   /*Check for using retransmission later*/
    writeNrfReg(W_REGISTER | SETUP_RETR, data);

    data = 0x03;   // RF data rate to 1Mbps / 1 Mhz and power is 0 dbm max power
    writeNrfReg(W_REGISTER | RF_SETUP, data);

    data = 0x05;           // enable dynamic payloads, enable Tx_payload, NO_ACK
    writeNrfReg(W_REGISTER | FEATURE, data);

    data = 0x3F;         // Configure dynamic payload lengths for all data pipes
    writeNrfReg(W_REGISTER | DYNPD, data);

    data = 0x70;
    writeNrfReg(W_REGISTER | STATUS, data);

    data = 76;                       // Set RF channel to be 2400 + 76(data) mhz
    writeNrfReg(W_REGISTER | RF_CH, data);

    data = 0x00;                                           // Turn off auto ACKs
    writeNrfReg(W_REGISTER | EN_AA, data);

    enableNrfCs();                         // Flush Tx FIFO after sending a SYNC
    writeSpi1Data(W_REGISTER | FLUSH_TX);
    readSpi1Data();
    disableNrfCs();

    enableNrfCs();                         // Flush Tx FIFO after sending a SYNC
    writeSpi1Data(W_REGISTER | FLUSH_RX);
    readSpi1Data();
    disableNrfCs();

    nrf24l0PowerUp();

}
//----------------------------------------------------
//Device slots

void joinAccessSlot_DEV()
{
    txTimeStatus = true;
    slotNo = 3;
    acessSlotStart_dev = true;

    //debug
    if (debugMsg)
    {
        putsUart0("JOINstart-D\n");
    }
}
void uplinkSlot_DEV()
{
    txTimeStatus = true;
    slotNo = eepromGetDevInfo_DEV() + 4;                    // Uplink slot start
    acessSlotStart_dev = false;
    uplinkSlot_dev = true;                        // Turn off after transmission
    //    appSendFlag = true;

    //debug
    if (debugMsg)
    {
        putsUart0("UPLINKstart-D\n");
    }
}
void syncRxDevSlot()
{
    // Check if Sync was received by the device and wait for respective slots
    if (nrfJoinEnabled)
    {
        stopTimer_ms(joinAccessSlot_DEV);
        startOneshotTimer_ms(joinAccessSlot_DEV, ACCESS_SLOT);
    }
    else
    {
        uint32_t myUplinkSlot = getMySlot(eepromGetDevInfo_DEV()); // Get the time you have to wait for your slotNo
        //myUplinkSlot += GUARD_TIMER;

        char str[30];
        snprintf(str, sizeof(str), "uplink slot time : %lu\n", myUplinkSlot);
        putsUart0(str);
        uint8_t mydev = eepromGetDevInfo_DEV();
        snprintf(str, sizeof(str), "My dev no : %u\n", (1 << mydev));
        putsUart0(str);

        stopTimer_ms(uplinkSlot_DEV);
        startOneshotTimer_ms(uplinkSlot_DEV, myUplinkSlot);
    }
    /*reset all the checks/data */
    isSync = false;
    uplinkSlot_dev = false;                    // turned on by timer in its slot
    acessSlotStart_dev = false;  // Turned on by timer if join enabled in device
    txTimeStatus = false;                        // Turned on by one shot timers

}
//----------------------------------------------------
//Bridge Slots

void syncSlot_BR()
{
    nrfSyncEnabled = true;
    slotNo = 0;
    setPinValue(SYNC_LED, 1);                // Toggle SYNC for each sync sent
    if (debugMsg)
    {
        putsUart0("\nSYNC-BR\n");
    }
}
void downlinkSlot_BR()
{
    setPinValue(SYNC_LED, 0);                // Toggle SYNC for each sync sent
    slotNo = 1;
    txTimeStatus = true;
    downlinkSlotStart_br = true;

    if (debugMsg)
    {
        putsUart0("DL-BR\n");
    }
}
void fastackSlot_BR()
{
    txTimeStatus = true;
    slotNo = 2;
    downlinkSlotStart_br = false;
    fastackSlotStart_br = true;
    appSendFlag = true;

    if (debugMsg)
    {
        putsUart0("FACK-BR\n");
    }

}
void joinAccessSlot_BR()
{
    txTimeStatus = true;
    slotNo = 3;
    fastackSlotStart_br = false;
    acessSlotStart_br = true;

    if (debugMsg)
    {
        putsUart0("JOIN-BR\n");
    }
}
void uplinkSlot_BR()
{
    acessSlotStart_br = false;
    uplinkSlot_br = false;           // since uplink is not being used by bridge

    if (debugMsg)
    {
        putsUart0("UL-BR\n");
    }
}

void TimerHandler_BR()
{
    if (!timersStarted_br)
    {                  // Start timers if not started
        stopTimer_ms(downlinkSlot_BR);
        stopTimer_ms(fastackSlot_BR);
        stopTimer_ms(joinAccessSlot_BR);
        stopTimer_ms(uplinkSlot_BR);

        putsUart0("ts -BR\n");
        startOneshotTimer_ms(downlinkSlot_BR, DL_SLOT);
        startOneshotTimer_ms(fastackSlot_BR, FACK_SLOT);
        startOneshotTimer_ms(joinAccessSlot_BR, ACCESS_SLOT);
        startOneshotTimer_ms(uplinkSlot_BR, UL0_SLOT);

        timersStarted_br = true;
    }
}
//----------------------------------------------------
// SPI bus functions
//----------------------------------------------------

void enableNrfCs(void)
{
    setPinValue(SSI1FSS, 0);
    _delay_cycles(4);                    // allow line to settle
}

void disableNrfCs(void)
{
    setPinValue(SSI1FSS, 1);
}

void writeNrfReg(uint8_t reg, uint8_t data)
{
    enableNrfCs();
    writeSpi1Data(reg);
    readSpi1Data();
    writeSpi1Data(data);
    readSpi1Data();
    disableNrfCs();
}

void writeNrfData(uint8_t data)
{
    writeSpi1Data(data);
    readSpi1Data();
}

void readNrfReg(uint8_t reg, uint8_t *data)
{
    enableNrfCs();
    writeSpi1Data(reg);
    readSpi1Data();
    writeSpi1Data(0);
    *data = readSpi1Data();
    disableNrfCs();
}

void readNrfData(uint8_t *data)
{
    writeSpi1Data(0);
    *data = readSpi1Data();
}
//----------------------------------------------------
// Timing and Slot management functions
//----------------------------------------------------
void enableSync_BR()
{
    nrfSyncEnabled = true;
    isBridge = true;
}

void enableJoin_DEV()
{
    if (!getPinValue(JOIN_BUTTON))
    { // wait for PB2 button press on device
        nrfJoinEnabled = true;
        nrfJoinCount = 0;
    }
}

void enableJoin_BR()
{
    if (!getPinValue(JOIN_BUTTON))
    { // wait for PB2 button press on bridge
        nrfJoinEnabled_BR = true;
        setPinValue(JOIN_LED, 1);
    }
}

//----------------------------------------------------
// Tx/ Rx functions
//----------------------------------------------------

void nrf24l0PulseCE()
{

    setPinValue(CE_GPIO, 1);
    waitMicrosecond(15);
    setPinValue(CE_GPIO, 0);

}

void putNrf24l0DataPacket(uint8_t *data, uint16_t size)
{
    uint8_t j = 0;
    int i = 0;

    // go to standby 1 mode
    setPinValue(CE_GPIO, 0);
    waitMicrosecond(TX_RX_DELAY);

    // Tx mode setting
    writeNrfReg(W_REGISTER | CONFIG, 0x0E);    // CRC enabled, PWRUP, Prim_rx =0
    enableNrfCs();                              // Frame begins
    writeNrfData(W_TX_PAYLOAD_NO_ACK);

    //Write data byte by byte
    for (i = 0, j = 0; i < size; i++, j++)
    {

        if ((j != 0) && (j % 32) == 0)
        {
            nrf24l0PulseCE();
            while (true != nrf24l0TxStatus())
                ;   //for every 32 bytes high to low transition //

            writeNrfReg(W_REGISTER | STATUS, 0x20);
            disableNrfCs();

            waitMicrosecond(TX_RX_DELAY);
            enableNrfCs();

            writeNrfData(W_TX_PAYLOAD_NO_ACK);
        }
        writeNrfData(data[i]);
    }
    disableNrfCs();                             // Frame ends

    nrf24l0PulseCE();

    waitMicrosecond(TX_RX_DELAY);

    // poll till data is transmitted
    while (true != nrf24l0TxStatus())
        ;
    writeNrfReg(W_REGISTER | STATUS, 0x20);        // clear the TX bit

}
void getnrf24l01DataPacket()
{                   // Get 32 bytes of data at a time

    int i = 0;
    //CE enable to Rx mode//
    setPinValue(CE_GPIO, 1);

    writeNrfReg(W_REGISTER | CONFIG, 0x0F);    // CRC enabled, PWRUP, Prim_Rx =1
    waitMicrosecond(TX_RX_DELAY);
    readNrfReg(R_REGISTER | R_RX_PL_WID, &payloadlength); // get the payload length of Rx packet

    enableNrfCs();                              // Frame begins
    writeNrfData(R_RX_PAYLOAD);

    while (i < payloadlength)
    {
        readNrfData(&Rxpacket[Rx_wrIndex]);
        Rx_wrIndex = (Rx_wrIndex + 1) % DATA_MAX_SIZE;
        ++i;
    }
    disableNrfCs();                             // Frame ends

    enableNrfCs();                         // Flush Tx FIFO after sending a SYNC
    writeSpi1Data(W_REGISTER | FLUSH_RX);
    readSpi1Data();
    disableNrfCs();
}

void parsenrf24l01DataPacket()
{
    // Parse the 32 bytes from getRxpacket and check for Sync, Start code, Slot number, Length , Data
    uint8_t deviceNum = 0;
    uint32_t devBits = 0;

    if (debugMsg)
    {
        int i;
        uint8_t j = 0;
        char str[40];
        putsUart0("debug Rxpacket data: \n");
        for (i = 0; i < payloadlength; i++)
        {
            snprintf(str, sizeof(str), "%u ", Rxpacket[i]);
            putsUart0(str);
        }
        putsUart0("\n");
    }

    // check sync. Last byte for frame count is omitted
    if (strncmp((char*) &Rxpacket[Rx_wrIndex - payloadlength], (char*) syncMsg,
                sizeof(syncMsg) - 2) == 0)
    {
        isSync = true;
        Rx_index = 0;
        Rx_wrIndex = 0;
    }
    else
    {

        if (packetLength > 32)
        { // Check for continuos transmission of data and we dont check for the payload length in subsequent packets
            putsUart0("More than 32 bytes HANDLE LATER\n");
        }
        else if (strncmp((char*) &Rxpacket[Rx_index], (char*) startCode,
                         sizeof(startCode)) == 0)
        { // check start code
            deviceNum = eepromGetDevInfo_DEV(); //Get device number stored in EEPROm. For already stored devices

            if (!nrfJoinEnabled && !isBridge)
            { // check whether myDevice got an ACK
                devBits = devBits | (Rxpacket[Rx_index + 8] << 24);
                devBits = devBits | (Rxpacket[Rx_index + 7] << 16);
                devBits = devBits | (Rxpacket[Rx_index + 6] << 8);
                devBits = devBits | (Rxpacket[Rx_index + 5]);

                if ((devBits & 0xFFFF0000) || (devBits & (1 << deviceNum)) == 0)
                {
                    return;
                }

                if (Rxpacket[Rx_index + 2] == 2)
                {         //Packet in  FACK slot (slot no =2) received by DEVICE
                    //                    if( Rxpacket[Rx_index +5] == deviceNum){         // check whether myDevice got an ACK
                    msgAcked = true;           // Flag used by APPLICATION LAYER
                    //                    }
                }
            }

            else if (isBridge)
            {
                devBits = devBits | (Rxpacket[Rx_index + 8] << 24);
                devBits = devBits | (Rxpacket[Rx_index + 7] << 16);
                devBits = devBits | (Rxpacket[Rx_index + 6] << 8);
                devBits = devBits | (Rxpacket[Rx_index + 5]);

                if (devBits & 0xFFFF0000)
                {
                    lastMsgDevNo_br = (devBits & 0x0000FFFF);
                }
            }
            if ((Rxpacket[Rx_index + 4]) != 0x80)
            {
                packetLength = Rxpacket[Rx_index + 4] << 8; // MSB ; Remlen is sent as LSB,MSB in the uint16 from tx side
            }
            packetLength |= Rxpacket[Rx_index + 3];             // LSB of Remlen

            if (Rxpacket[Rx_index + 2] == 1)
            {                    // packet in DL slot received by DEVICE

                if (Rxpacket[Rx_index + 4] == 0x80)
                {            // Distinguish between JOIN RESP and normal DL slot
                    isJoinResp_dev = true;
                }
                else
                {
                    isDevPacket = true;
                    putsUart0("DL data received\n");
                }
                /*Used in setting dev no in downlink slotNo */
            }

            else if ((Rxpacket[Rx_index + 2] == 3) && nrfJoinEnabled_BR)
            { // Msg in ACCESS slot received by the Bridge
                if (Rxpacket[Rx_index + 4] == 0x80)
                { // Check for JOIN REQ by checking for MSB bit set in uint16 remlen
                    isJoinPacket = true;
                }
            }
            else if (Rxpacket[Rx_index + 2] > 3)
            {                 // packet in Uplink slots slot
                isBridgePacket = true; // Is this packet received by the Bridge in Uplink Slots
            }
        }
        else
        {                                 // Error handling in case of junk data
            Rx_index = 0;
            Rx_wrIndex = 0;
        }
    }
}

bool nrf24l0TxStatus()
{
    uint8_t data = 0;
    readNrfReg(R_REGISTER | STATUS, &data);
    return ((data & 0x20) >> 5);      // bit 5 is Tx_DS data sent fifo interrupt
}

bool nrf24l0RxStatus()
{
    uint8_t data = 0;
    readNrfReg(R_REGISTER | STATUS, &data);
    if (((data & 0x0E) == 0x0E) || ((data & 0x0E) == 0x0C))
    { // check for the data pipe in RX_P_NO in FIFO_STATUS
        return false;
    }
    else
    {
        readNrfReg(R_REGISTER | STATUS, &data); // Read status register to check RX_DR
        writeNrfReg(R_REGISTER | STATUS, (data & ~0x40)); // Set the RX_DR bit to clear the interrupt
        return true;
    }
}

void nrf24l0PowerUp()
{
    uint8_t data;

    data = 0;
    readNrfReg(R_REGISTER | CONFIG, &data);

    if (!(data & 0x02))
    {
        writeNrfReg(W_REGISTER | CONFIG, 0x0E); // CRC enabled, PWRUP, PRIM_RX=0
        waitMicrosecond(5000); // Wait 5 ms for checking that data write is complete

    }

}

bool isNrf24l0DataAvailable(void)     // Check that data is available in Rx FIFO
{
    uint8_t data = 0;

    nrf24l0PowerUp();

    writeNrfReg(W_REGISTER | CONFIG, 0x0F);     // CRC enabled, PWRup, PRIM_RX=1

    data = 0x70;
    writeNrfReg(W_REGISTER | STATUS, data);

    setPinValue(CE_GPIO, 1);

    waitMicrosecond(TX_RX_DELAY);

    if (nrf24l0RxStatus())
    {
        writeNrfReg(W_REGISTER | STATUS, 0x40);        // Clear the Rx interrupt
        return true;
    }
    else
    {
        return false;
    }
}

callback dataReceived(uint8_t *data, uint16_t size)
{
    int i;
    uint8_t j = 0;
    char str[40];
    putsUart0("data received to app layer\n");
    /*data copy*/
    for (i = 0; i <size; i++)
    {
        buffer[i] = data[i];
        snprintf(str, sizeof(str), "%u ", buffer[i]);
        putsUart0(str);
    }
    putsUart0("\n");

    dataReceivedFlag = true;
}

uint8_t nrf24l0GetChecksum(uint8_t *ptr, uint16_t size)
{
    uint8_t sum = 0;
    uint8_t i = 0;

    /*1 bytes checksum */
    for (i = 0; i < size; i++)
    {
        sum += ptr[i];
    }

    return ~sum;
}

void nrf24l0TxSync()
{
    if (nrfSyncEnabled)
    {

        syncMsg[sizeof(syncMsg) - 2] =
                (uint8_t) ((syncFrameCount & 0xFF00) >> 8);      // Frame count
        syncMsg[sizeof(syncMsg) - 1] = (uint8_t) (syncFrameCount & 0x00FF);
        putNrf24l0DataPacket(syncMsg, sizeof(syncMsg)); /*send sync message*/
        nrfSyncEnabled = false;
        ++syncFrameCount;
        char str[40];
        uint32_t mySlotTemp = getMySlot(readEeprom(NO_OF_DEV_IN_BRIDGE));
        snprintf(str, sizeof(str), "%"PRIu32"\n", mySlotTemp);
        putsUart0(str);
        mySlotTemp += GUARD_TIMER;
        stopTimer_ms(syncSlot_BR);
        startOneshotTimer_ms(syncSlot_BR, mySlotTemp);

        timersStarted_br = false;
        //        appSendFlag = true;
        msgAcked = false;

        Rx_index = 0;
        Rx_wrIndex = 0;
    }
}

void nrf24l0TxJoinReq_DEV()
{
    if (nrfJoinCount < 10)
    {
        uint8_t joinBuffer[15] = { 0 };
        uint8_t *ptr = joinBuffer;
        uint8_t slot = 3;                                         // ACCESS slot
        uint16_t remlen = (1 + sizeof(myMac)) | (0x8000); // 0x8000 in MSB byte separates a JOIN RESP from a normal DL packet

        strncpy((char*) ptr, (char*) startCode, sizeof(startCode));
        ptr += sizeof(startCode);
        strncpy((char*) ptr, (char*) &slot, sizeof(slot));
        ptr += sizeof(slot);
        strncpy((char*) ptr, (char*) &remlen, sizeof(remlen));
        ptr += sizeof(remlen);
        strncpy((char*) ptr, (char*) myMac, sizeof(myMac));
        ptr += sizeof(myMac);
        *ptr += nrf24l0GetChecksum(joinBuffer, sizeof(joinBuffer));

        //send join request in access slot
        putNrf24l0DataPacket(joinBuffer, sizeof(joinBuffer));
        txTimeStatus = false;
        putsUart0("JOIN REQ-D\n");
        acessSlotStart_dev = false;
        nrfJoinCount++;
    }
}

void nrf24l0TxJoinResp_BR()
{
    uint8_t joinBuffer[10] = { 0 };
    uint8_t *ptr = joinBuffer;
    uint8_t slot = 1;                                           // DL slot
    uint16_t remlen = ((1 + 1) | 0x8000); //crc + allocated dev no + 0x8000 MSB set for JOIN RESP

    strncpy((char*) ptr, (char*) startCode, sizeof(startCode));
    ptr += sizeof(startCode);
    strncpy((char*) ptr, (char*) &slot, sizeof(slot));
    ptr += sizeof(slot);
    strncpy((char*) ptr, (char*) &remlen, sizeof(remlen));
    ptr += sizeof(remlen);
    strncpy((char*) ptr, (char*) &allocatedDevNum, sizeof(allocatedDevNum));
    ptr += sizeof(allocatedDevNum);
    *ptr += nrf24l0GetChecksum(joinBuffer, sizeof(joinBuffer));

    //send join response in access slot//
    putNrf24l0DataPacket(joinBuffer, sizeof(joinBuffer));
    txTimeStatus = false;
    nrfJoinEnabled_BR = false;
    sendJoinResponse_BR = false;
    downlinkSlotStart_br = false; // Make this false so that no more data is sent along with Join RESP in DL slot
    setPinValue(JOIN_LED, 0);

}

int nrf24l0TxMsg(uint8_t *data, uint16_t size, uint32_t devBitNum)
{
    uint8_t packet[DATA_MAX_SIZE] = { 0 };
    uint8_t *ptr = packet;
    uint8_t temp = 0;
    uint16_t remlen = size + 1 + sizeof(devBitNum); /*Increment length for checksum */

    if (size > (DATA_MAX_SIZE - META_DATA_SIZE))
    {
        return -1;
    }

    /*meta data*/
    strncpy((char*) ptr, (char*) startCode, sizeof(startCode));
    ptr += sizeof(startCode);
    strncpy((char*) ptr, (char*) &slotNo, sizeof(slotNo));
    ptr += sizeof(slotNo);
    strncpy((char*) ptr, (char*) &remlen, sizeof(remlen));
    ptr += sizeof(remlen);

    //user data//
    temp = (uint8_t) (devBitNum & 0xFF);
    strncpy((char*) ptr, (char*) &temp, 1); //Use devno for sending DL packets from Bridge or uplink packets from DEVICE
    ptr += 1;

    temp = (uint8_t) ((devBitNum >> 8) & 0xFF);
    strncpy((char*) ptr, (char*) &temp, 1); //Use devno for sending DL packets from Bridge or uplink packets from DEVICE
    ptr += 1;

    temp = (uint8_t) ((devBitNum >> 16) & 0xFF);
    strncpy((char*) ptr, (char*) &temp, 1); //Use devno for sending DL packets from Bridge or uplink packets from DEVICE
    ptr += 1;

    temp = (uint8_t) ((devBitNum >> 24) & 0xFF);
    strncpy((char*) ptr, (char*) &temp, 1); //Use devno for sending DL packets from Bridge or uplink packets from DEVICE
    ptr += 1;

    strncpy((char*) ptr, (char*) data, size);
    ptr += size;

    //calculate checksum//
    *ptr = nrf24l0GetChecksum(packet, size + META_DATA_SIZE);

    putNrf24l0DataPacket(packet, size + META_DATA_SIZE);
    //guard timer //
    waitMicrosecond(GUARD_TIMER);

    enableNrfCs();                         // Flush Tx FIFO after sending a SYNC
    writeSpi1Data(W_REGISTER | FLUSH_TX);
    readSpi1Data();
    disableNrfCs();

    memset(packet, 0, sizeof(packet));

    return 0;
}

void nrf24l0RxMsg(callback fn)
{
    // Devices use this function as used in template code to keep on polling the data to check for received messages
    //**SYNC
    // Check for Sync message and start timers for the devices transmission slot
    // If a new device is Joining a timer is started to wait for transmission in ACCESS slot
    // If the bridge is transmitting (assumption that Bridge device also sends the Sync packets), Wait for Downlink and FASTACK slots to transmit data

    uint16_t size = 0;
    uint8_t Rxchecksum = 0;                             // Received checksum
    uint8_t devMac[6] = { 0 };
    int i, j = 0;
    char str[30];

    if (isNrf24l0DataAvailable())
    {

        getnrf24l01DataPacket();
        parsenrf24l01DataPacket();   // set flags if syn message or startcode or

        if (isSync)                                      //if sync //
        {
            syncRxDevSlot();                         // Handle device sync slots
            msgAcked = false;         // Check for ACKs is reset after each SYNC

            for (i = 0; i < 32; i++)
            {
                Rxpacket[i] = 0;
            }

            setPinValue(SYNC_LED, 1);      // Toggle SYNC for each sync received
            waitMicrosecond(1000);
            setPinValue(SYNC_LED, 0);
            putsUart0("SYNC-D\n");
        }

        else if (nrfJoinEnabled && isJoinResp_dev)          //JOIN RESP for dev
        {
            putsUart0("joinresponse-D\n");
            snprintf(str, sizeof(str), "allocated dev no: %"PRIu8,
                     Rxpacket[Rx_index + 5]);
            putsUart0(str);
            uint32_t temp = (1 << Rxpacket[Rx_index + 5]);
            snprintf(str, sizeof(str), "\n allocated devbits no: %"PRIu32,
                     temp);
            putsUart0(str);

            eepromSetDevInfo_DEV(Rxpacket[Rx_index + 5]);
            setPinValue(JOIN_LED, 0);
            nrfJoinEnabled = false; // This is true from the moment dev sends a join req and receives a
            Rx_index += payloadlength - 1;
            isJoinResp_dev = false;
        }
        else if (msgAcked)                         //check acks for my device //
        {
            appSendFlag = false; // Received ack . Stop retransmission. Send this to Application layer to stop retransmissting messages
            Rx_index += payloadlength - 1;
        }

        else if (isJoinPacket)
        { // Bridge checks for packets received in Access slot if join button pressed on Bridge
            if (nrfJoinEnabled_BR && acessSlotStart_br)
            {
                for (i = 0, j = Rx_index + 5; i < 6; i++, j++)
                {
                    devMac[i] = Rxpacket[j];
                }
                sendJoinResponse_BR = true;
                allocatedDevNum = eepromSetGetDevInfo_BR(devMac); // Set mac address of bridge in eeprom
                Rx_index += payloadlength - 1;
                putsUart0("Join Packet Req- BR\n");
            }
        }
        else if ((isBridgePacket) || (isDevPacket))
        {         // is data for me? (Device in DL slots and BRIDGE in UL slots)
            //crc check*/
            Rxchecksum = Rxpacket[Rx_index + payloadlength - 1];
            if (Rxchecksum
                    == nrf24l0GetChecksum(&Rxpacket[Rx_index],
                                          payloadlength - 1))
            {
                //Devices and Bridge can pop the data out and utilize it as needed
                size = payloadlength - META_DATA_SIZE;
                fn(&Rxpacket[Rx_index + META_DATA_SIZE - 1], size); // downlink data. Pointer to User data start and size of the user data
                Rx_index = 0;
                Rx_wrIndex = 0;                      // reset for next packet Rx
                packetLength = 0;
                isBridgePacket = false;
                isDevPacket = false;
                Rx_index += payloadlength - 1;

            }
            //reset the buffer. */ //Extra precaution
            for (i = 0; i < sizeof(Rxpacket); i++)
            {
                Rxpacket[i] = 0;
            }
        }
    }

}

uint32_t getMySlot(uint8_t devno)
{

    //Handle slots for all transmissions for Devices and bridges separately
    // default value of EEPROM
    if (devno == 0xFF)
    {
        devno = 0;
    }

    return UL0_SLOT
            + (((DEF_SLOT_WIDTH) + (TX_RX_DELAY_SLOT * _32BYTE_PACKETS)
                    + GUARD_TIMER) * devno);

}

uint32_t getDeviceNum_BR()
{

}

void eepromSetDevInfo_DEV(uint8_t deviceNum)
{
    uint8_t *ptr;
    uint8_t i;

    writeEeprom(NO_OF_DEV_IN_BRIDGE, 1);               // Number of devices is 1
    writeEeprom(DEV1_NO_START, deviceNum);                      // Device number

    ptr = (uint8_t*) DEV1_MAC_START;

    /*My mac address*/
    for (i = 0; i < sizeof(myMac); i++, ++ptr)
    {
        writeEeprom(ptr, myMac[i]);
    }

}

uint8_t eepromGetDevInfo_DEV()
{
    return readEeprom(DEV1_NO_START);  // Only one device for Dev; Device number
}

uint8_t eepromSetGetDevInfo_BR(uint8_t *data)
{
    int i = 0;
    int j = 0;
    uint8_t devNo = 0;
    uint8_t *ptr = (uint8_t*) NO_OF_DEV_IN_BRIDGE;
    char str[30];
    uint8_t mac[6] = { 0 };

    uint8_t noOfDev =
            (readEeprom(NO_OF_DEV_IN_BRIDGE) == 0xFF) ?
                    0 : readEeprom(NO_OF_DEV_IN_BRIDGE);
    ptr = (uint8_t*) DEV1_MAC_START;

    for (i = 0; i < noOfDev; i++)
    {

        for (j = 0; j < 6; j++, ptr++)
        {
            mac[j] = readEeprom(ptr);
        }

        if (strncmp((char*) mac, (char*) data, sizeof(myMac)) == 0)
        {
            ptr -= (sizeof(myMac) + 1);
            devNo = readEeprom(ptr);

            putsUart0("Existing Dev No:-BR:  ");
            snprintf(str, sizeof(str), "%u", devNo);
            putsUart0(str);
            putsUart0("\n");

            return devNo;
        }
        ptr += 1 + sizeof(myMac);
    } // for ends

    if (noOfDev == 0)
    {
        ptr = (uint8_t*) DEV1_MAC_START;
    }

    --ptr;
    ++noOfDev;
    writeEeprom(ptr, noOfDev);
    devNo = readEeprom(ptr);
    writeEeprom(NO_OF_DEV_IN_BRIDGE, devNo); /* store total no of devices */
    ++ptr;

    putsUart0("Dev No: -BR:  ");
    snprintf(str, sizeof(str), "%u", devNo);
    putsUart0(str);
    putsUart0("\n");

    /*mac*/
    for (i = 0; i < sizeof(myMac); i++, ++ptr)
    {
        writeEeprom(ptr, data[i]);
    }

    return devNo;
}

uint32_t getBridgeDevNoAddr_DEV()
{
    uint32_t temp = 0xFFFF0000 | eepromGetDevInfo_DEV();
    return temp;
}
//Timer interrupt
void resendLastTcpMsg(void)
{
    switch (tcpState)
    {
    case TCP_LISTEN:
        sendArpRequest(datat, ipFrom, ipTo);
        break;
    case TCP_SYN_RECEIVED:
        //        sendTcpMessage(data, s, ipTo, (SYN | ACK), tcpState,
        //                                                   NULL, 0);
        tcpState = TCP_LISTEN;
        break;
        //-------------------------------------------------------------------------
        //Server has requested to close connection
    case TCP_CLOSE_WAIT:
        sendTcpMessage(datat, s, ipTo, (ACK), tcpState, NULL, 0);
        sendTcpMessage(datat, s, ipTo, (FIN), tcpState, NULL, 0);
        break;
    default:
        break;
    }
    restartTimer(resendLastTcpMsg);
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)

{
    initSystemClockTo40Mhz();
    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // Init timer
    initTimer();

    // Init ethernet interface (eth0)
    // Use the value x from the spreadsheet
    putsUart0("Device Powered up \n");
    putsUart0("\nStarting eth0\n");
    initEther(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    setEtherMacAddress(2, 3, 4, 5, 6, 73);

    // Init EEPROM
    initEeprom();
    readConfiguration();
    nrf24l0Init();
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    uint8_t i = 0;
    //uint8_t count = 0;
    uint8_t data[16] = { }; //Strcpy your data into here

    char MTRSP[5] = { 'M', 'T', 'R', 'S', 'P' };
    char MTRDR[5] = { 'M', 'T', 'R', 'D', 'R' };
    char MTRPW[5] = { 'M', 'T', 'R', 'P', 'W' };
    char TEMPF[5] = { 'T', 'E', 'M', 'P', 'F' };
    char BARCO[5] = { 'B', 'A', 'R', 'C', 'O' };
    char DISTC[5] = { 'D', 'I', 'S', 'T', 'C' };
    char DISTO[5] = { 'D', 'I', 'S', 'T', 'O' };

    wirelessPacket *wp = (wirelessPacket *)buffer;

    //wirelessPacket *pingResp = (wirelessPacket*) buffer;
    //    if(0 < eepromGetDevInfo_DEV()) {
    //        deviceJoined = true;
    //    }
    //Send Sync ARP message to get Hardware Address
    sendArpRequest(datat, ipFrom, ipTo);
    tcpState = TCP_LISTEN;
    //This function will set a 1 second timer to resend the last msg
    //It will use tcpState to keep track of what the last message was
    //startPeriodicTimer(resendLastTcpMsg, 2);
    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    //    while (true)
    //    {
    // Put terminal processing here
    //  processShell(wp);
    //Device functions
    //nrf24l0RxMsg(dataReceived);

    char barcode[10] = {0};
    while (true)
    {
        if(0 < eepromGetDevInfo_DEV()) {
            deviceJoined = true;
        }

        processShell(wp);
        enableJoin_DEV();
        //cmdHandler();

        //Common functions
        //readNrfReg(R_REGISTER|FIFO_STATUS,&fifoStatus);
        nrf24l0RxMsg(dataReceived);
        if (dataReceivedFlag == true)
        {
            //wp = (wirelessPacket*) buffer;
            if (wp->packetType == PING_REQUEST)
            {
                //send ping response
                sendPingResponseFlag = true;
                putsUart0("IT IS A PING PACKET!!\n");

            }
            else if (wp->packetType == DEVCAPS_REQUEST)
            {
                // send device response packet
                sendDevCapsResponseFlag = true;
                putsUart0("IT IS A DEVCAPS_REQUEST!!\n");
            }
            else if (wp->packetType == PUSH)
            {
                char str[5];
                // handle data
                pushMessage *RxData = (pushMessage*) (wp->data);
//                snprintf(str, 5, RxData->topicName);
////                putsUart0(str);
//                putsUart0("\n");
//                putsUart0(str);
//                                                    putsUart0("\n");
//                putsUart0(RxData->data);
//                                    putsUart0("\n");
                if (strncmp(RxData->topicName, "TEMPF", 5) == 0)
                {
                    putsUart0("tempf\n\n");
                    setTemperature(RxData->data);
                    putsUart0(RxData->data);
                    putsUart0("\n");
                }
                else if (strncmp(RxData->topicName, "BARCO", 5) == 0)
                {
                    setBarcode(RxData->data);
                }
                else if (strncmp(RxData->topicName, "DISTC", 5) == 0)
                {
                    setDistance(RxData->data);
                }
                else if (strncmp(RxData->topicName, "DISTO", 5) == 0)
                {
                    setDetect(RxData->data);
                }
                putsUart0("IT IS A PUSH Message!!\n");
            }
            else
            {
                putsUart0("Invalid Packet Type\n\n");
            }
            dataReceivedFlag = false;
        }

        //check for slot no
        if (txTimeStatus) // change later
        {
            //count ++;
            if(0 == eepromGetDevInfo_DEV())
            {
                if(count == 20) {
                    //putsUart0("No device no allocated. Please Join first \n");
                }
                deviceJoined = false;
            }
            if (strcmp(barcode, NULL) > 0)
                sendPushFlag = true;
            if (nrfJoinEnabled && acessSlotStart_dev)
            {
                nrf24l0TxJoinReq_DEV();
                setPinValue(JOIN_LED, 1);
            }
            else if(uplinkSlot_dev && deviceJoined)// Appsend flag is controlled by application layer.
            { // Appsend flag is controlled by application layer.
                if (sendPingResponseFlag)
                {
                    uint8_t buff[22];
                    wirelessPacket* pingResp = (wirelessPacket*)buff;
                    pingResp->packetType = PING_RESPONSE;
                    nrf24l0TxMsg((uint8_t*) pingResp, MAX_PACKCET_SIZE,
                                 getBridgeDevNoAddr_DEV()); //**********************************************************************************************************
                    //if (msgAcked)
                    {
                        sendPingResponseFlag = false;
                    }
                    putsUart0("Uplink Ping Response Sent\n");
                }
                else if (sendDevCapsResponseFlag)
                {
                    // convert deviceCaps struct to uint8_t
                    //define your device capabilities
                    uint8_t buff[22];
                    wirelessPacket* devCapsResponse = (wirelessPacket*)buff;
                    devCapsResponse->packetType = WEB_SERVER;
                    nrf24l0TxMsg((uint8_t*) devCapsResponse,
                                 MAX_PACKCET_SIZE,
                                 getBridgeDevNoAddr_DEV()); //always cast as uint8_t, size of whole struct
                    sendDevCapsResponseFlag = false;
                    putsUart0("DevCaps Response Sent\n");

                    //                        devCaps->deviceNum = eepromGetDevInfo_DEV(); //dev number is stored in eeprom
                    //                        devCaps->numOfCaps = 1;                 //add num of cap
                    //
                    //                        description *caps = (description*) devCaps->caps;

                    //strcpy(caps[0]->capDescription, "MTRSP");     //need description for each cap, change for num capabailities
                    //Only doing it this way to so there is no null character at the end
                    //Only needed for topic name
                    //                        for (i = 0; i < 5; i++)
                    //                        {
                    //                            caps->capDescription[i] = BARCO[i];
                    //                        }
                    //                        caps->inputOrOutput = 0; //1 is input, 0 is output, 2 in and out

                    //    wp->packetType = DEVCAPS_RESPONSE;

                }
              if (transmitCounter == 20)//(sendPushFlag)
                {
                  transmitCounter = 0;
                   // putsUart0("Push check\n");
                    // convert data struct to uint8_t
                    //                        pushMessage *pushData = (pushMessage*) wp->data;
                    //                        for (i = 0; i < 5; i++)
                    //                        {
                    //                            pushData->topicName[i] = BARCO[i]; //change to true topic
                    //                        }
                    //                        strcpy(data, barcode);
                    //                        strcpy(pushData->data, data); //store your data in the data string
                    //                                                      //data is a buffer array holding the data you want to send
                    //                                                      //ex. 98deg
                    //                        wp->packetType = PUSH; //always pushing our data
                    //
                    //                        nrf24l0TxMsg((uint8_t*) wp, sizeof(&wp),
                    //                                     getBridgeDevNoAddr_DEV()); //0xFFFFFFFF is who we are sending it to aka bridge address
                    //                        sendPushFlag = false;
                    //                        putsUart0("Push Msg Sent\n");
                    //count = 0;
                    //Build packet
                    uint8_t buff[22];
                    wirelessPacket *wirelessPush = (wirelessPacket*) buff;
                    pushMessage *pushData =
                            (pushMessage*) wirelessPush->data;

                    if (isMotorPower())
                    {
                        //putsUart0("\n\nMotor Power\n\n");
                        char *powerStr = getMotorPower();
                        for (i = 0; i < 5; i++)
                        {
                            pushData->topicName[i] = MTRPW[i]; //change to true topic
                        }
                        strcpy(data, powerStr);
                        strcpy(pushData->data, data);
                        wirelessPush->packetType = PUSH;
                        nrf24l0TxMsg((uint8_t*) wirelessPush,
                                     MAX_PACKCET_SIZE,
                                     getBridgeDevNoAddr_DEV()); //cast your own pushMessage struct

                    }
                    else if (isMotorSpeed())
                    {
                       // putsUart0("\n\nMotor speed\n\n");
                        char *speedStr = getMotorSpeed();
                        for (i = 0; i < 5; i++)
                        {
                            pushData->topicName[i] = MTRSP[i]; //change to true topic
                        }
                        strcpy(data, speedStr);
                        strcpy(pushData->data, data);
                        wirelessPush->packetType = PUSH;
                        nrf24l0TxMsg((uint8_t*) wirelessPush,
                                     MAX_PACKCET_SIZE,
                                     getBridgeDevNoAddr_DEV()); //cast your own pushMessage struct

                    }
                    else if (isMotorDirection())
                    {
                        //putsUart0("\n\nMotor direction\n\n");
//                        char *directionStr = getMotorDirection();
                        char *directionStr = getMotorDirection();
                        for (i = 0; i < 5; i++)
                        {
                            pushData->topicName[i] = MTRDR[i]; //change to true topic
                        }
                        strcpy(data, directionStr);
                        strcpy(pushData->data, data);
                        wirelessPush->packetType = PUSH;
                        nrf24l0TxMsg((uint8_t*) wirelessPush,
                                     MAX_PACKCET_SIZE,
                                     getBridgeDevNoAddr_DEV()); //cast your own pushMessage struct
                    }
                }
              else
              {
                  transmitCounter++;
              }
                //sendPushFlag = false;
            }
        }
        // Packet processing

        if (isEtherDataAvailable())
        {
            if (isEtherOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            // Get packet
            getEtherPacket(datat, 1522);

            // Handle ARP request
            if (isArpRequest(datat))
            {
                sendArpResponse(datat);
                getTcpMessageSocket(datat, &s);
            }
            //Handle initial sent sent ARP request to get HW address and IP
            if (isArpResponse(datat))
            {

                getTcpMessageSocket(datat, &s);
            }
            // Handle IP datagram
            if (isIp(datat))
            {
                if (isIpUnicast(datat))
                {
                    // Handle ICMP ping request
                    if (isPingRequest(datat))
                    {
                        sendPingResponse(datat);
                    }

                    // Handle UDP datatgram
                    if (isUdp(datat))
                    {
                        udpData = getUdpData(data);
                        if (strcmp((char*) udpData, "on") == 0)
                            setPinValue(GREEN_LED, 1);
                        if (strcmp((char*) udpData, "off") == 0)
                            setPinValue(GREEN_LED, 0);
                        getUdpMessageSocket(data, &s);
                        sendUdpMessage(data, s, (uint8_t*) "Received", 9);
                    }
                    // Handle TCP datagram
                    if (isTcp(datat))
                    {

                        tcpState = tcpGetState(datat, tcpState);
                        switch (tcpState)
                        {
                        //-------------------------------------------------------------
                        //Server has responded to Syn and is waiting on ACK to establish connection
                        //                        case TCP_SYN_RECEIVED:
                        //                            restartTimer(resendLastTcpMsg);
                        //                            sendTcpMessage(data, s, ipTo, ACK, tcpState, NULL,
                        //                                           0);
                        //                            uint8_t tcpPayload[19];
                        //                            createMqttMsg(CONNECT, tcpPayload, NULL, NULL);
                        //                            sendTcpMessage(data, s, ipTo, (PSH | ACK), tcpState,
                        //                                           tcpPayload, 19);
                        //                            connected = false;
                        //                            break;
                        //TCP WEBSERVER
                        case TCP_SYN_RECEIVED:
                            startPeriodicTimer(resendLastTcpMsg, 3);
                            sendTcpMessage(datat, s, ipTo, (SYN | ACK),
                                           tcpState,
                                           NULL,
                                           0);
                            break;
                        case TCP_ESTABLISHED:
                            stopTimer(resendLastTcpMsg);
                            mqttType = getMqttMsgType(datat);

                            //HTTP
                            if (mqttType == 0x47)
                            {
                                char *httpData = getHttpPage(datat);
                                sendTcpMessage(datat, s, ipTo, (PSH | ACK),
                                               tcpState, httpData,
                                               strlen(httpData));
                            }
                            else
                                mqttType = 0;

                            break;
                            //--------------------------------------------------------------
                            //MQTT Server has issues a close connection
                        case TCP_CLOSE_WAIT: //Received FIN ACK send ACK
                            startPeriodicTimer(resendLastTcpMsg, 1);
                            sendTcpMessage(datat, s, ipTo, (ACK), tcpState,
                                           NULL,
                                           0);
                            tcpState = TCP_LAST_ACK;
                            sendTcpMessage(datat, s, ipTo, (FIN | ACK),
                                           tcpState,
                                           NULL,
                                           0);
                            break;
                        case TCP_LAST_ACK: //Received ACK send FIN ACK
                            stopTimer(resendLastTcpMsg);
                            tcpState = TCP_LISTEN;
                            connected = false;
                            break;
                            //-----------------------------------------------------------------
                            //Client has issued a close connection
                        case TCP_FIN_WAIT_1:
                            closeConnection = false;
                            startPeriodicTimer(resendLastTcpMsg, 1);
                            sendTcpMessage(datat, s, ipTo, (FIN | ACK),
                                           tcpState,
                                           NULL,
                                           0);
                            break;
                        case TCP_FIN_WAIT_2:
                            stopTimer(resendLastTcpMsg);
                            sendTcpMessage(datat, s, ipTo, (ACK), tcpState,
                                           NULL,
                                           0);
                            waitMicrosecond(10000);
                            tcpState = TCP_LISTEN;
                            connected = false;
                            break;

                        }
                    }
                }
            }
        }
    }
    //    }
}

