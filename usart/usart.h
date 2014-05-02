#include "stm32f0xx.h"
#include "stm32f0xx_rcc.h"
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_usart.h"

void usartInit();
void usartSendString(const char *s);
void usartSend8(uint16_t Data);
void usartSend16(uint16_t Data);
void usartSend32(uint32_t Data);
