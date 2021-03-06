/**
  ******************************************************************************
  * @file    msp.c
  * @author  Moe2Code
  * @brief   This file provides code for MSP initialization necessary for processor
  *          and peripherals relied on in the rock paper scissors application.
  */

// Includes
#include "main.h"


extern void Error_handler(void);
extern uint8_t UART_Msg_Tx(char msg[]);


char uart_msg[100] = {0};


/**
  * @brief  Initializes the HAL MSP. Low level processor specific initializations done here.
  * @param	None
  * @retval None
  */

void HAL_MspInit(void)
{
	// 1. Set up the priority grouping of the ARM Cortex Mx processor
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

	// 2. Enable the required system exceptions of the ARM Cortex Mx processor
	SCB->SHCSR |= 0x7 << 16; 	//Usage fault, memory fault, and bus fault system exceptions

	// 3. Configure the priority for the system exceptions
	HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0); 	// Set interrupt priority to 0
	HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0); 			// Set interrupt priority to 0
	HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0); 		// Set interrupt priority to 0

	// SysTick exception is configured in HAL_Init()
}


/**
  * @brief  Initializes the CAN MSP. Low level inits of CAN1 peripheral are done here.
  * @param  hcan pointer to a CAN_HandleTypeDef structure that contains
  *         the configuration information for the specified CAN.
  * @retval None
  */

void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)
{
	GPIO_InitTypeDef gpios_can1 = {0};

	// Enable the clock for CAN1 and GPIOA peripherals
	__HAL_RCC_CAN1_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	// Configure GPIO pins to act as CAN1 Tx and Rx
	gpios_can1.Pin = GPIO_PIN_8 | GPIO_PIN_9;  // PB8 --> CAN1_RX and PB9 --> CAN1_TX
	gpios_can1.Mode = GPIO_MODE_AF_PP;
	gpios_can1.Pull = GPIO_NOPULL;
	gpios_can1.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	gpios_can1.Alternate = GPIO_AF9_CAN1;

	HAL_GPIO_Init(GPIOB, &gpios_can1);

	// Enable the IRQ and set the priority (NVIC settings)
	HAL_NVIC_SetPriority(CAN1_TX_IRQn, 15, 0);
	HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 15, 0);
	HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 15, 0);
	HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 15, 0);

	HAL_NVIC_EnableIRQ(CAN1_TX_IRQn);
	HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
	HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
	HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
}


/**
  * @brief	Initializes the timer base MSP.
  * @param 	htim TIM Base handle
  * @retval	None
  */

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htimer)
{
	// 1. Enable the clock for the timer peripheral (TIM6)
	__HAL_RCC_TIM6_CLK_ENABLE();

	// 2. Enable the IRQ of TIM6
	HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);

	// 3. Setup the priority for TIM6_DAC_IRQn
	HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 15, 0);
}


/**
  * @brief  Initializes the RTC MSP.
  * @param  hrtc pointer to an RTC_HandleTypeDef structure that contains
  *         the configuration information for RTC.
  * @retval None
  */

void HAL_RTC_MspInit(RTC_HandleTypeDef* hrtc)
{
	RCC_OscInitTypeDef RTC_OscInitStruct = {0};
	RCC_PeriphCLKInitTypeDef RTC_PeriClkInit = {0};

	// Use LSI as clock source for RTC. Disc doesn't have an LSE.
	// 1. Turn on the LSI
	RTC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
	RTC_OscInitStruct.LSIState = RCC_LSI_ON;
	RTC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

	uint8_t osc_status = HAL_RCC_OscConfig(&RTC_OscInitStruct);
	if( osc_status!= HAL_OK)
	{
		sprintf(uart_msg, "LSI osc error: %d\r\n", osc_status);
		UART_Msg_Tx(uart_msg);

		Error_handler();
	}

	// 2. Select LSI as RTC Clock
	RTC_PeriClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
	RTC_PeriClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;

	if(HAL_RCCEx_PeriphCLKConfig(&RTC_PeriClkInit) != HAL_OK)
	{
		UART_Msg_Tx("RTC peri clock config in MSP error\r\n");

		Error_handler();
	}

	// 3. Enable the RTC Clock
	__HAL_RCC_RTC_ENABLE();
}


/**
  * @brief  Initializes the UART MSP.
  * @param  huart Pointer to a UART_HandleTypeDef structure that contains
  *         the configuration information for the specified UART module.
  * @retval None
  */

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
	// Here we are going to do the low level inits of the USART2 peripheral

	GPIO_InitTypeDef gpios_uart2 = {0};

	// 1. Enable the clock for the USART2 as well as GPIO port A peripheral
	__HAL_RCC_USART2_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	// 2. Do the pin muxing configurations

	gpios_uart2.Mode = GPIO_MODE_AF_PP;
	gpios_uart2.Pull = GPIO_PULLUP;
	gpios_uart2.Speed = GPIO_SPEED_FREQ_LOW;
	gpios_uart2.Alternate = GPIO_AF7_USART2;

	gpios_uart2.Pin = GPIO_PIN_2;
	HAL_GPIO_Init(GPIOA, &gpios_uart2);		// PA2 --> UART2_TX

	gpios_uart2.Pin = GPIO_PIN_3;
	HAL_GPIO_Init(GPIOA, &gpios_uart2);		// PA3 --> UART2_RX

	// 3. Enable the IRQ and set the priority (NVIC settings)
	HAL_NVIC_EnableIRQ(USART2_IRQn);
	HAL_NVIC_SetPriority(USART2_IRQn, 15, 0);   // Interrupt priority set to 15
}
