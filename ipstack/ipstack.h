#ifndef IPSTACK_H
#define IPSTACK_H

#include "stdint.h"
#include <stdbool.h>

extern uint8_t deviceIP[4];

#define MAXPACKETLEN 500

typedef struct
{
  uint8_t DestAddrs[6];
  uint8_t SrcAddrs[6];
  uint16_t type;
}  EtherNetII;
// Ethernet packet types
#define ARPPACKET 0x0806
#define IPPACKET 0x0800

typedef struct
{
  EtherNetII eth;
  uint16_t hardware;
  uint16_t protocol;
  uint8_t hardwareSize;
  uint8_t protocolSize;
  uint16_t opCode;
  uint8_t senderMAC[6];
  uint8_t senderIP[4];
  uint8_t targetMAC[6];
  uint8_t targetIP[4];
}ARP;

//ARP opCodes
#define ARPREPLY  0x0002
#define ARPREQUEST 0x0001
//ARP hardware types
#define ETHERNET 0x0001
// Switch to host order for the enc28j60
#define HTONS(x) ((x<<8)|(x>>8))

typedef struct
{
  EtherNetII eth;
  uint8_t hdrlen : 4;
  uint8_t version : 4;
  uint8_t diffsf;
  uint16_t len;
  uint16_t ident;
  uint16_t fragmentOffset1: 5;
  uint16_t flags : 3;
  uint16_t fragmentOffset2 : 8;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t chksum;
  uint8_t source[4];
  uint8_t dest[4];
}IPhdr;
// IP protocols
#define ICMPPROTOCOL 0x01
#define UDPPROTOCOL 0x11
#define TCPPROTOCOL 0x6

typedef struct
{
  IPhdr ip;
  uint16_t sourcePort;
  uint16_t destPort;
  uint8_t seqNo[4];
  uint8_t ackNo[4];
  uint8_t NS:1;
  uint8_t reserverd : 3;
  uint8_t hdrLen : 4;
  uint8_t FIN:1;
  uint8_t SYN:1;
  uint8_t RST:1;
  uint8_t PSH:1;
  uint8_t ACK:1;
  uint8_t URG:1;
  uint8_t ECE:1;
  uint8_t CWR:1;
  uint16_t wndSize;
  uint16_t chksum;
  uint16_t urgentPointer;
  //uint8_t options[8];
}TCPhdr;

typedef enum {tcpCONNECTING, tcpOPEN, tcpCLOSED} tcpStates_t;
extern tcpStates_t tcpState;

typedef struct
{
  IPhdr ip;
  uint16_t sourcePort;
  uint16_t destPort;
  uint16_t len;
  uint16_t chksum;
}UDPhdr;


typedef struct
{
  IPhdr ip;
  uint8_t type;
  uint8_t code;
  uint16_t chksum;
  uint16_t iden;
  uint16_t seqNum;
}ICMPhdr;

#define ICMPREPLY 0x0
#define ICMPREQUEST 0x8

typedef struct
{
  UDPhdr udp;
  uint16_t id;
  uint16_t flags;
//  uint8_t QR : 1;
//  uint8_t opCode :4;
//  uint8_t AA:1;
//  uint8_t TC:1;
//  uint8_t RD:1;
//  uint8_t RA:1;
//  uint8_t Zero:3;
//  uint8_t Rcode:4;
  uint16_t qdCount;
  uint16_t anCount;
  uint16_t nsCount;
  uint16_t arCount;
}DNShdr;

#define DNSUDPPORT 53
#define DNSQUERY 0
#define DNSREPLY 1

void add32(uint8_t *op32, uint16_t op16);
uint16_t chksum(uint16_t sum, uint8_t *data, uint16_t len);
void SetupBasicIPPacket(uint8_t* packet, uint8_t protocol, uint8_t* destIP);
void SendArpPacket(uint8_t* targetIP);
void ReplyArpPacket(ARP* arpPacket);
uint16_t setupAckTcp(TCPhdr* tcp, uint16_t len, bool sendSYN);
void swapDirection(TCPhdr* tcp);
void SendPing(uint8_t* targetIP);
void PingReply(ICMPhdr* ping, uint16_t len);
uint8_t GetPacket(uint8_t protocol, uint8_t* packet);
uint16_t IPstackInit();
uint16_t IPstackIdle();

#endif
