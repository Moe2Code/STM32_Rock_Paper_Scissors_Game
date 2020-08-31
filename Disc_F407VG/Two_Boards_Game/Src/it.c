/**
  ******************************************************************************
  * @file    it.c
  * @author  Moe2Code
  * @brief   This source file calls the appropriate handler for each interrupt used in the
  * 		 rock paper scissors game.
  */

// Includes
#include "main.h"


// Global variables shared with other modules
extern CAN_HandleTypeDef hcan1;
extern TIM_HandleTypeDef htimer6;


/**
  * @brief This function handles system tick timer.
  */

void SysTick_Handler(void)
{
	HAL_IncTick();				// Increment system tick
	HAL_SYSTICK_IRQHandler();
}


/**
  * @brief This function handles interrupt request specifically for
  * message transmission via CAN1 peripheral
  */

void CAN1_TX_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan1);
}


/**
  * @brief This function handles interrupt request specifically for
  * messages received in FIFO0 during CAN communication
  */

void CAN1_RX0_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan1);
}


/**
  * @brief This function handles interrupt request specifically for
  * messages received in FIFO1 during CAN communication
  */

void CAN1_RX1_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan1);
}


/**
  * @brief This function handles interrupt request specifically for
  * status changes and errors (SCE) that occur during CAN communication
  */

void CAN1_SCE_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan1);
}


/**
  * @brief This function handles interrupt request specifically for
  * the basic timer, TIM6
  */

void TIM6_DAC_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&htimer6);
}

