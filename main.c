#include "stm32f0xx.h"
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_rcc.h"
#include "enc28j60.h"
#include "usart.h"
#include "ipstack.h"
#include "websocket.h"

int main(void) {

	usartInit();
	IPstackInit();

	uint8_t i;
	uint8_t packet[MAXPACKETLEN];
	uint8_t wsMsgLen = 0;
	char *wsMsg;

	while (1) {
		if (tcpState == tcpCLOSED) {
			wsHandshake(packet, 50000);
		} else if (wsState == wsOPEN) {
			//Receive websocket message
			wsMsg = receiveWSmsg(packet, 50000, &wsMsgLen);
			//Print received message
			for (i = 0; i < wsMsgLen; i++)usartSend8(wsMsg[i]);
			//Send message back
			if(wsMsgLen)sendWSmsg(packet, 50000, wsMsg, wsMsgLen);

		} else {
			IPstackIdle();
		}
	}
}
