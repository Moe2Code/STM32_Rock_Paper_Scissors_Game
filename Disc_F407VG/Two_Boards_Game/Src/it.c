/*
 * it.c
 *
 *  Created on: Dec 08, 2019
 *      Author: 426180
 */

#include "main.h"

extern CAN_HandleTypeDef hcan1;
extern TIM_HandleTypeDef htimer6;

void SysTick_Handler(void)
{
	HAL_IncTick();				// Increment system tick
	HAL_SYSTICK_IRQHandler();
}


void CAN1_TX_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan1);
}


void CAN1_RX0_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan1);
}


void CAN1_RX1_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan1);
}


void CAN1_SCE_IRQHandler(void)		// Status Change and Error (SCE) interrupt
{
	HAL_CAN_IRQHandler(&hcan1);
}


void TIM6_DAC_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&htimer6);
}

