#include "led.h"
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_rcc.h"

void ledInit() {
	GPIO_InitTypeDef gpio;
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE); //Enable GPIOB peripheral clock

	GPIO_StructInit(&gpio);
	gpio.GPIO_Pin = GPIO_Pin_3; // IO13
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_Init(GPIOB, &gpio);

	gpio.GPIO_Pin = GPIO_Pin_4; // IO12
	GPIO_Init(GPIOB, &gpio);
}
