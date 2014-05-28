#include "ipstack.h"
#include "enc28j60.h"
#include "usart.h"
#include <string.h>
#include "websocket.h"

// MAC address of the enc28j60
uint8_t deviceMAC[6] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
// Router MAC (Unknown before ARP request)
uint8_t routerMAC[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
// IP address of the enc28j60
uint8_t deviceIP[4] = { 192, 168, 1, 33 };
// IP address of the router
uint8_t routerIP[4] = { 192, 168, 1, 1 };

uint8_t serverIP[4];

uint8_t dnsIP[4] = { 208, 67, 222, 222 };

tcpStates_t tcpState = tcpCLOSED;

void add32(uint8_t *op32, uint16_t op16){
	op32[3] += (op16 & 0xff);
	op32[2] += (op16 >> 8);

	if (op32[2] < (op16 >> 8)) {
		++op32[1];
		if (op32[1] == 0) {
			++op32[0];
		}
	}

	if (op32[3] < (op16 & 0xff)) {
		++op32[2];
		if (op32[2] == 0) {
			++op32[1];
			if (op32[1] == 0) {
				++op32[0];
			}
		}
	}
}

uint16_t chksum(uint16_t sum, uint8_t *data, uint16_t len){
	uint16_t t;
	const uint8_t *dataptr;
	const uint8_t *last_byte;

	dataptr = data;
	last_byte = data + len - 1;

	while (dataptr < last_byte) {
		t = (dataptr[0] << 8) + dataptr[1];
		sum += t;
		if (sum < t) {
			sum++;
		}
		dataptr += 2;
	}

	if (dataptr == last_byte) {
		t = (dataptr[0] << 8) + 0;
		sum += t;
		if (sum < t) {
			sum++; /* carry */
		}
	}

	return sum;
}

void SetupBasicIPPacket(uint8_t* packet, uint8_t protocol, uint8_t* destIP){
	IPhdr* ip = (IPhdr*) packet;

	ip->eth.type = (uint16_t) HTONS(IPPACKET);
	memcpy(ip->eth.DestAddrs, routerMAC, sizeof(routerMAC));
	memcpy(ip->eth.SrcAddrs, deviceMAC, sizeof(deviceMAC));

	ip->version = 0x4;
	ip->hdrlen = 0x5;
	ip->diffsf = 0;
	ip->ident = 2;
	ip->flags = 0x2;
	ip->fragmentOffset1 = 0x0;
	ip->fragmentOffset2 = 0x0;
	ip->ttl = 128;
	ip->protocol = protocol;
	ip->chksum = 0x0;
	memcpy(ip->source, deviceIP, sizeof(deviceIP));
	memcpy(ip->dest, destIP, sizeof(deviceIP));
}

void SendArpPacket(uint8_t* targetIP){
	ARP arpPacket;

	//Setup EtherNetII Header
	memcpy(arpPacket.eth.SrcAddrs, deviceMAC, sizeof(deviceMAC));
	memset(arpPacket.eth.DestAddrs, 0xff, sizeof(deviceMAC));
	arpPacket.eth.type = (uint16_t) HTONS(ARPPACKET);

	//Setup ARP Header
	arpPacket.hardware = (uint16_t) HTONS(ETHERNET);
	arpPacket.protocol = (uint16_t) HTONS(IPPACKET);
	arpPacket.hardwareSize = sizeof(deviceMAC);
	arpPacket.protocolSize = sizeof(deviceIP);
	arpPacket.opCode = (uint16_t) HTONS(ARPREQUEST);

	//Setup ARP addresses
	memset(arpPacket.targetMAC, 0, sizeof(deviceMAC));
	memcpy(arpPacket.senderMAC, deviceMAC, sizeof(deviceMAC));
	memcpy(arpPacket.targetIP, targetIP, sizeof(routerIP));
	if (!memcmp(targetIP, deviceIP, sizeof(deviceIP))) {
		memset(arpPacket.senderIP, 0, sizeof(deviceIP));
	} else {
		memcpy(arpPacket.senderIP, deviceIP, sizeof(deviceIP));
	}

	//Send the packet
	enc28j60_send_packet((uint8_t*) &arpPacket, sizeof(ARP));
}

void ReplyArpPacket(ARP* arpPacket){
	if (!memcmp(arpPacket->targetIP, deviceIP, sizeof(deviceIP))) {

		//Swap the target and sender MACs and IPs
		memcpy(arpPacket->eth.DestAddrs, arpPacket->eth.SrcAddrs,
				sizeof(deviceMAC));
		memcpy(arpPacket->eth.SrcAddrs, deviceMAC, sizeof(deviceMAC));
		memcpy(arpPacket->targetMAC, arpPacket->senderMAC, sizeof(deviceMAC));
		memcpy(arpPacket->senderMAC, deviceMAC, sizeof(deviceMAC));
		memcpy(arpPacket->targetIP, arpPacket->senderIP, sizeof(deviceIP));
		memcpy(arpPacket->senderIP, deviceIP, sizeof(deviceIP));

		//Change the opCode to reply
		arpPacket->opCode = (uint16_t) HTONS(ARPREPLY);

		//Send the packet
		enc28j60_send_packet((uint8_t*) arpPacket, sizeof(ARP));
	}
}

//len: ETH + IP + TCP headers + payload
uint16_t setupAckTcp(TCPhdr* tcp, uint16_t len, bool sendSYN){
	//Zero the checksums
	tcp->chksum = 0x0;
	tcp->ip.chksum = 0x0;

	//Setup source and destination MAC addresses
	memcpy(tcp->ip.eth.DestAddrs, tcp->ip.eth.SrcAddrs, sizeof(deviceMAC));
	memcpy(tcp->ip.eth.SrcAddrs, deviceMAC, sizeof(deviceMAC));

	//Setup source and destination IP addresses
	memcpy(tcp->ip.dest, tcp->ip.source, sizeof(deviceIP));
	memcpy(tcp->ip.source, deviceIP, sizeof(deviceIP));

	//Setup ports
	uint16_t destPort = tcp->destPort;
	tcp->destPort = tcp->sourcePort;
	tcp->sourcePort = destPort;

	//Swap ack and seq
	uint8_t ack[4];
	memcpy(ack, tcp->ackNo, sizeof(ack));
	memcpy(tcp->ackNo, tcp->seqNo, sizeof(ack));
	memcpy(tcp->seqNo, ack, sizeof(ack));

	//Calculate ACK number
	if (tcp->SYN || tcp->FIN) {
		add32(tcp->ackNo, 1);
	} else {
		add32(tcp->ackNo, len - sizeof(TCPhdr));
	}

	//Send SYN in case of tcp server handshake
	if (sendSYN){
		tcp->SYN = 1;
	}
	else{
		tcp->SYN = 0;
	}

	//Setup flags and header length, the other flags (FIN, SYN, RST) remain as received
	tcp->ACK = 1;
	tcp->PSH = 0;
	tcp->hdrLen = (sizeof(TCPhdr) - sizeof(IPhdr)) / 4;

	//Remove payload and calculate tcp and ip checksum
	len = sizeof(TCPhdr);
	tcp->ip.len = (uint16_t) HTONS((len-sizeof(EtherNetII)));
	uint16_t pseudochksum = chksum(TCPPROTOCOL + len - sizeof(IPhdr),
			tcp->ip.source, sizeof(deviceIP) * 2);
	uint16_t chk1, chk2;
	chk1 = ~(chksum(pseudochksum, ((uint8_t*) tcp) + sizeof(IPhdr),
			len - sizeof(IPhdr)));
	tcp->chksum = (uint16_t) HTONS(chk1);
	chk2 = ~(chksum(0, ((uint8_t*) tcp) + sizeof(EtherNetII),
			sizeof(IPhdr) - sizeof(EtherNetII)));
	tcp->ip.chksum = (uint16_t) HTONS(chk2);
	return len;
}

void swapDirection(TCPhdr* tcp){
	//Zero checksum
	tcp->chksum = 0x0;
	tcp->ip.chksum = 0x0;

	//Setup source and destination MAC addresses
	memcpy(tcp->ip.eth.DestAddrs, tcp->ip.eth.SrcAddrs, sizeof(deviceMAC));
	memcpy(tcp->ip.eth.SrcAddrs, deviceMAC, sizeof(deviceMAC));

	//Setup source and destination IP addresses
	memcpy(tcp->ip.dest, tcp->ip.source, sizeof(deviceIP));
	memcpy(tcp->ip.source, deviceIP, sizeof(deviceIP));

	//Setup ports
	uint16_t destPort = tcp->destPort;
	tcp->destPort = tcp->sourcePort;
	tcp->sourcePort = destPort;

	//Swap ack and seq
	uint8_t ack[4];
	memcpy(ack, tcp->ackNo, sizeof(ack));
	memcpy(tcp->ackNo, tcp->seqNo, sizeof(ack));
	memcpy(tcp->seqNo, ack, sizeof(ack));
}

void SendPing(uint8_t* targetIP){
	ICMPhdr ping;

	//Setup ping header
	SetupBasicIPPacket((uint8_t*) &ping, ICMPPROTOCOL, targetIP);
	ping.ip.flags = 0x0;
	ping.type = 0x8;
	ping.code = 0x0;
	ping.chksum = 0x0;
	ping.iden = (uint16_t) HTONS(0x1);
	ping.seqNum = (uint16_t) HTONS(76);

	//Calculate checksum
	uint16_t chk1, chk2;
	chk1 = ~(chksum(0, ((uint8_t*) &ping) + sizeof(IPhdr),
			sizeof(ICMPhdr) - sizeof(IPhdr)));
	ping.chksum = (uint16_t) HTONS(chk1);
	chk2 = ~(chksum(0, (uint8_t*) &ping + sizeof(EtherNetII),
			sizeof(IPhdr) - sizeof(EtherNetII)));
	ping.ip.chksum = (uint16_t) HTONS(chk2);
	ping.ip.len = (uint16_t) HTONS((60-sizeof(EtherNetII)));

	//Send packet
	enc28j60_send_packet(&ping, sizeof(ICMPhdr));
	usartSendString("Ping sent\r");
}

void PingReply(ICMPhdr* ping, uint16_t len){
	if (ping->type == ICMPREQUEST) {
		//Setup ping packet
		ping->type = ICMPREPLY;
		ping->chksum = 0x0;
		ping->ip.chksum = 0x0;
		memcpy(ping->ip.eth.DestAddrs, ping->ip.eth.SrcAddrs,
				sizeof(deviceMAC));
		memcpy(ping->ip.eth.SrcAddrs, deviceMAC, sizeof(deviceMAC));
		memcpy(ping->ip.dest, ping->ip.source, sizeof(deviceIP));
		memcpy(ping->ip.source, deviceIP, sizeof(deviceIP));

		//Calculate checksum
		uint16_t chk1, chk2;
		chk1 = ~(chksum(0, ((uint8_t*) ping) + sizeof(IPhdr),
				len - sizeof(IPhdr)));
		ping->chksum = (uint16_t) HTONS(chk1);
		chk2 = ~(chksum(0, ((uint8_t*) ping) + sizeof(EtherNetII),
				sizeof(IPhdr) - sizeof(EtherNetII)));
		ping->ip.chksum = (uint16_t) HTONS(chk2);

		//Send ping reply
		enc28j60_send_packet(ping, len);
		usartSendString("Ping replied\r");
	}else if (ping->type == ICMPREPLY){
		usartSendString("Ping received\r");
	}
}

uint8_t GetPacket(uint8_t protocol, uint8_t* packet){
	uint8_t i;
	for (i = 0; i < 255; ++i) {
		uint16_t len;
		if (len = enc28j60_recv_packet(packet, MAXPACKETLEN)) {
			EtherNetII* eth = (EtherNetII*) packet;
			if (eth->type == (uint16_t) HTONS(ARPPACKET)) {
				ARP* arpPacket = (ARP*) packet;
				if (arpPacket->opCode == (uint16_t) HTONS(ARPREQUEST))
					ReplyArpPacket(arpPacket);
			} else if (eth->type == (uint16_t) HTONS(IPPACKET)) {
				IPhdr* ip = (IPhdr*) packet;
				if (ip->protocol == protocol) {
					return 1;
				}
				if (ip->protocol == ICMPPROTOCOL) {
					PingReply((ICMPhdr*) packet, len);
				}
			}
		}
	}
	return 0;
}

uint16_t IPstackInit(){
	enc28j60_init(deviceMAC);

	//Send ARP packet
	SendArpPacket(deviceIP);
	uint16_t i;
	for (i = 0; i < 0x0fff; i++) {
		ARP arpPacket;
		if (enc28j60_recv_packet((uint8_t*) &arpPacket, sizeof(ARP))) {
			if (arpPacket.eth.type == (uint16_t) HTONS(ARPPACKET)) {
				//Check if the IP address is taken
				if (!memcmp(arpPacket.senderIP, deviceIP, sizeof(deviceIP))) {
					return 0;
				}
			}
		}
	}
	// Get the router's MAC address
	SendArpPacket(routerIP);
	for (i = 0; i < 0x5fff; i++) {
		ARP arpPacket;
		if (enc28j60_recv_packet((uint8_t*) &arpPacket, sizeof(ARP))) {
			if (arpPacket.eth.type == (uint16_t) HTONS(ARPPACKET)) {
				if (arpPacket.opCode == (uint16_t) HTONS(ARPREPLY)
						&& !memcmp(arpPacket.senderIP, routerIP,
								sizeof(routerIP))) {
					memcpy(routerMAC, arpPacket.senderMAC, sizeof(routerMAC));
					return 1;
				}
			}
		}
	}
	return 0;
}

uint16_t IPstackIdle() {
	uint8_t packet[MAXPACKETLEN];
	GetPacket(0, packet);
	return 1;
}
