#include "stm32f0xx.h"
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_rcc.h"
#include "enc28j60.h"
#include "usart.h"
#include "ipstack.h"
#include "websocket.h"
#include "led.h"

int main(void) {

	usartInit();
	IPstackInit();
	ledInit();

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
			if(memcmp ( wsMsg, "SET", 3) == 0)GPIO_WriteBit(GPIOB, GPIO_Pin_3, Bit_SET);
			if(memcmp ( wsMsg, "RESET", 5) == 0)GPIO_WriteBit(GPIOB, GPIO_Pin_3, Bit_RESET);;
			//Print received message
			for (i = 0; i < wsMsgLen; i++)usartSend8(wsMsg[i]);
			//Send message back
			if(wsMsgLen)sendWSmsg(packet, 50000, wsMsg, wsMsgLen);

		} else {
			IPstackIdle();
		}

//		static int i;
//		static int led_state = 0;
//
//		GPIO_WriteBit(GPIOB, GPIO_Pin_3, Bit_SET);
//		for (i = 0; i < 100000; ++i)
//			;
//		GPIO_WriteBit(GPIOB, GPIO_Pin_3, Bit_RESET);
//		for (i = 0; i < 100000; ++i)
//			;
	}
}
