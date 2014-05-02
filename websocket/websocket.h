#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "ipstack.h"

typedef struct {
	TCPhdr tcp;
	uint8_t opcode :4;
	uint8_t reserved : 3;
	uint8_t FIN :1;
	uint8_t payloadLen :7;
	uint8_t MASK :1;
	uint8_t maskingKey[4];
} WShdr;

typedef enum {wsCONNECTING, wsOPEN, wsCLOSED} wsStates_t;
extern wsStates_t wsState;

void unmask(uint8_t* masking_key, uint8_t * data, uint16_t data_length,
		uint8_t* payload);
void wsHandshake(uint8_t* packet, uint16_t port);
char* receiveWSmsg(uint8_t* packet, uint16_t port, uint8_t* msgLen);
void sendWSmsg(uint8_t *packet, uint16_t port, char *data, uint8_t msgLen);
void wsCloseHandshake(uint8_t* packet, uint16_t port);

#endif
