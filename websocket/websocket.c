#include "websocket.h"
#include "usart.h"
#include "stdint.h"
#include <string.h>
#include "enc28j60.h"
#include "sha1.h"
#include "base64.h"
#include "ipstack.h"

//Websocket protocol Globally Unique Identifier
const char* GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

//Websocket state
wsStates_t wsState = wsCLOSED;

void unmask(uint8_t* masking_key, uint8_t * data, uint16_t data_length,
		uint8_t* wsMsg) {
	uint16_t i;
	for (i = 0; i < data_length; i++) {
		wsMsg[i] = masking_key[i % 4] ^ data[i];
	}
}

void wsHandshake(uint8_t* packet, uint16_t port) {
	TCPhdr* tcp = (TCPhdr*) packet;
	uint16_t len = sizeof(TCPhdr);
	uint16_t totalLen;

	//Receive TCP request from browser
	do {
		GetPacket(TCPPROTOCOL, packet);
	} while (!(tcp->destPort == (uint16_t) HTONS(port) && tcp->SYN));
	tcpState = tcpCONNECTING;

	//Send ACK + SYN
	setupAckTcp(tcp, len, 1);
	enc28j60_send_packet(packet, len);

	//Wait for ACK
	do {
		GetPacket(TCPPROTOCOL, packet);
	} while (!(tcp->destPort == (uint16_t) HTONS(port)));
	tcpState = tcpOPEN;
	//TCP handshake completed

	usartSendString("TCP handshake completed\r");

	//Receive Websocket handshake request from browser
	do {
		GetPacket(TCPPROTOCOL, packet);
	} while (!(tcp->destPort == (uint16_t) HTONS(port)));
	wsState = wsCONNECTING;
	totalLen = HTONS(tcp->ip.len) + sizeof(EtherNetII);

	//Send ACK
	len = setupAckTcp(tcp, totalLen, 0);
	enc28j60_send_packet(packet, len);

	//Parse handshake fields
	uint8_t* inputPtr = packet + sizeof(TCPhdr);
	uint8_t* endPtr = packet + totalLen;
	uint8_t* secWebSocketKeyEnd;
	uint8_t* secWebSocketKeyStart;
	uint8_t sizeOfsecWebSocketKey;

	while (inputPtr < endPtr && inputPtr[0] != '\r' && inputPtr[1] != '\n') {
		if (memcmp(inputPtr, "GET / HTTP/1.1", 14) == 0) {
			//usartSendString("GET field OK.\r");
		} else if (memcmp(inputPtr, "Upgrade: websocket", 18) == 0) {
			//usartSendString("Upgrade field OK.\r");
		} else if (memcmp(inputPtr, "Sec-WebSocket-Key: ", 19) == 0) {

			//Extract the Sec-WebSocket-Key value
			secWebSocketKeyStart = inputPtr + 19;
			secWebSocketKeyEnd = strstr(inputPtr, "\r\n");
			sizeOfsecWebSocketKey = secWebSocketKeyEnd - (inputPtr + 19);
		}
		//Move pointer to next field
		inputPtr = strstr(inputPtr, "\r\n") + 2;
	}

	//Copy value to new pointer
	unsigned char secWebSocketKey[sizeOfsecWebSocketKey + strlen(GUID)];
	memcpy(secWebSocketKey, secWebSocketKeyStart, sizeOfsecWebSocketKey);

	//Concatenate with GUID
	memcpy(secWebSocketKey + sizeOfsecWebSocketKey, GUID, strlen(GUID));
	uint8_t output[20];
	sha1(secWebSocketKey, sizeOfsecWebSocketKey + strlen(GUID), output);

	//Encode to Base64
	unsigned char* dummy;
	size_t n = 0;
	base64_encode(dummy, &n, (unsigned char *) output, 20);
	unsigned char encodedOutput[n];
	base64_encode(encodedOutput, &n, (unsigned char *) output, 20);

	//Build response
	char * reponseHeader = "HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: WebSocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: ";
	char * rnrn = "\r\n\r\n";
	uint16_t responseLen = strlen(reponseHeader) + strlen(rnrn) + n;
	char response[responseLen];
	strcpy(response, reponseHeader);
	strcat(response, encodedOutput);
	strcat(response, rnrn);

	//Send response
	memcpy(packet + sizeof(TCPhdr), response, responseLen);
	len = sizeof(TCPhdr) + responseLen;
	tcp->ip.len = (uint16_t) HTONS((len-sizeof(EtherNetII)));
	tcp->chksum = 0;
	tcp->ip.chksum = 0;
	uint16_t chk1, chk2, pseudochksum;
	pseudochksum = chksum(TCPPROTOCOL + len - sizeof(IPhdr), tcp->ip.source,
			sizeof(deviceIP) * 2);
	chk1 = ~(chksum(pseudochksum, packet + sizeof(IPhdr), len - sizeof(IPhdr)));
	tcp->chksum = (uint16_t) HTONS(chk1);
	chk2 = ~(chksum(0, packet + sizeof(EtherNetII),
			sizeof(IPhdr) - sizeof(EtherNetII)));
	tcp->ip.chksum = (uint16_t) HTONS(chk2);
	enc28j60_send_packet(packet, len);

	//Wait for ACK
	do {
		GetPacket(TCPPROTOCOL, packet);
	} while (!(tcp->destPort == (uint16_t) HTONS(port)) && tcp->ACK == 1);

	//Handshake completed.
	wsState = wsOPEN;
	usartSendString("WS Handshake completed\r");
}

char* receiveWSmsg(uint8_t* packet, uint16_t port, uint8_t* msgLen) {
	uint16_t len;
	uint16_t totalLen;
	TCPhdr* tcp = (TCPhdr*) packet;

	if (wsState == wsOPEN) {
		//Receive websocket message
		tcp->destPort = 0;
		do {
			GetPacket(TCPPROTOCOL, packet);
		} while (!(tcp->destPort == (uint16_t) HTONS(port)));

		//Send ACK
		totalLen = HTONS(tcp->ip.len) + sizeof(EtherNetII);
		len = setupAckTcp(tcp, totalLen, 0);
		enc28j60_send_packet(packet, len);

		//Close TCP socket if FIN or RST flag is set
		if (tcp->FIN || tcp->RST) {
			tcpState = tcpCLOSED;
			*msgLen = 0;
			usartSendString("TCP connection closed\r");
			return NULL;
		}

		WShdr* ws = (WShdr*) packet;
		if (ws->payloadLen<126){
			*msgLen = ws->payloadLen;
		}else{
			//Only short payloads supported
			usartSendString("Payloads greater than 126 not supported\r");
			*msgLen = 0;
			return NULL;
		}


		//Close websocket connection if opcode is 8 (Close)
		if (ws->opcode == 8) {
			wsCloseHandshake(packet, port);
			*msgLen = 0;
			return NULL;
		}

		//Copy value to new pointer
		char wsMsg[*msgLen];
		memcpy(wsMsg, packet + sizeof(WShdr), *msgLen);

		//Unmask message
		char wsMsgUnmasked[*msgLen];
		unmask(ws->maskingKey, wsMsg, *msgLen, wsMsgUnmasked);

		return wsMsgUnmasked;
	} else {
		usartSendString("Error! Websocket is not open!");
		return NULL;
	}
}

void sendWSmsg(uint8_t *packet, uint16_t port, char *data, uint8_t msgLen) {
	//Setup TCP header
	TCPhdr* tcp = (TCPhdr*) packet;
	uint16_t len;
	len = sizeof(WShdr) + msgLen - 4;
	tcp->ip.len = (uint16_t) HTONS((len-sizeof(EtherNetII)));
	tcp->chksum = 0;
	tcp->ip.chksum = 0;

	//Setup WS header
	WShdr* ws = (WShdr*) packet;
	ws->MASK = 0;
	ws->opcode = 1;
	ws->payloadLen = msgLen;
	memcpy(packet + sizeof(WShdr) - 4, data, msgLen);

	//Calculate checksum
	uint16_t pseudochksum;
	pseudochksum = chksum(TCPPROTOCOL + len - sizeof(IPhdr), tcp->ip.source,
			sizeof(deviceIP) * 2);
	uint16_t chk1, chk2;
	chk1 = ~(chksum(pseudochksum, packet + sizeof(IPhdr), len - sizeof(IPhdr)));
	tcp->chksum = (uint16_t) HTONS(chk1);
	chk2 = ~(chksum(0, packet + sizeof(EtherNetII),
			sizeof(IPhdr) - sizeof(EtherNetII)));
	tcp->ip.chksum = (uint16_t) HTONS(chk2);

	//Send packet
	enc28j60_send_packet(packet, len);
	usartSendString("\rWS message sent\r");

	//Wait for ACK
	do {
		GetPacket(TCPPROTOCOL, packet);
	} while (!(tcp->destPort == (uint16_t) HTONS(port)));

	//Swap direction of packet for next send.
	swapDirection(tcp);
}

void wsCloseHandshake(uint8_t* packet, uint16_t port){
	//Setup close websocket message
	WShdr* ws = (WShdr*) packet;
	ws->FIN=1;
	ws->MASK=0;
	ws->opcode=8;
	ws->payloadLen=0;

	//Setup TCP packet
	TCPhdr* tcp = (TCPhdr*) packet;
	uint16_t len = sizeof(WShdr)-4;
	tcp->ip.len = (uint16_t) HTONS((len-sizeof(EtherNetII)));
	tcp->chksum = 0;
	tcp->ip.chksum = 0;
	uint16_t chk1, chk2, pseudochksum;
	pseudochksum = chksum(TCPPROTOCOL + len - sizeof(IPhdr), tcp->ip.source,
			sizeof(deviceIP) * 2);
	chk1 = ~(chksum(pseudochksum, packet + sizeof(IPhdr), len - sizeof(IPhdr)));
	tcp->chksum = (uint16_t) HTONS(chk1);
	chk2 = ~(chksum(0, packet + sizeof(EtherNetII),
			sizeof(IPhdr) - sizeof(EtherNetII)));
	tcp->ip.chksum = (uint16_t) HTONS(chk2);

	//Send packet
	enc28j60_send_packet(packet, len);
	wsState = wsCLOSED;
	usartSendString("Websocket connection closed\r");

	//Close connection if FIN or RST
	do {
		GetPacket(TCPPROTOCOL, packet);
	} while (!(tcp->destPort == (uint16_t) HTONS(port) && (tcp->FIN || tcp->RST)));

	//Send ACK
	len = sizeof(TCPhdr);
	setupAckTcp(tcp, len, 0);
	enc28j60_send_packet(packet, len);

	//TCP port is closed now
	tcpState = tcpCLOSED;
	wsState = wsCLOSED;
	usartSendString("TCP connection closed\r");
}
