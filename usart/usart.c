#include "usart.h"

void usartInit(){
	USART_InitTypeDef USART_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource14, GPIO_AF_1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource15, GPIO_AF_1);

	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);

	USART_Cmd(USART2,ENABLE);
}

void usartSendString(const char *s)
{
  while(*s)
  {
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, *s++);
  }
}

void usartSend8(uint16_t Data)
{
	while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	    USART_SendData(USART2, Data);
}

void usartSend16(uint16_t Data){
usartSend8(Data >> 8);
usartSend8(Data);
}

void usartSend32(uint32_t Data){
usartSend8(Data >> 24);
usartSend8(Data >> 16);
usartSend8(Data >> 8);
usartSend8(Data);
}
