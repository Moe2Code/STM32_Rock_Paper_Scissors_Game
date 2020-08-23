/*
 * main_.c
 *
 *  Created on: Dec 08, 2019
 *      Author: 426180
 */


#include "main.h"


void Error_handler(void);
void SysClockConfig_HSE(uint8_t ClkFreq);
void UART2_Init(void);
void CAN1_Init(void);
void GPIO_Init(void);
void CAN1_Tx(void);
void CAN_Filter_Config(void);
void Timer6_Init(void);
void manage_LED_output(uint8_t LED_ID);
uint8_t UART_Msg_Tx(char msg[]);
uint8_t Determine_Win(uint8_t player1, uint8_t player2);
void send_game_result(uint8_t winner);
void RTC_Init(void);
void RTC_CalendarConfig(void);
char* get_date_time(void);
void clear_sleep_flags(void);


UART_HandleTypeDef huart2 = {0};
CAN_HandleTypeDef hcan1 = {0};
TIM_HandleTypeDef htimer6 = {0};		// Timer 6 (TIM6) is a basic timer
RTC_HandleTypeDef hrtc = {0};
CAN_RxHeaderTypeDef RxHeader = {0};		// Stores the header of Rx frame
uint8_t debounce_cnt = 0;				// Counter to ensure we have a stable button input before we send a CAN message
char DateTime_Info[100] = {0};


/*
 * This app simulates a game of rock, paper, scissors played between two ST boards.
 * The game features timers, CAN messaging, RTC time/date keeping, game score keeping
 * in the backup SRAM, and low power management of the boards.
 */


int main(void)
{
	HAL_Init();

	SysClockConfig_HSE(SYSCLK_FREQ_50MHZ);	// PLL via HSE (8 MHz) is used to generate SYSCLK of 50 MHz
											// Use HSE since it is more accurate than HSI

	clear_sleep_flags();	// If woke up from Standby mode clear the associated flags

	Timer6_Init();

	UART2_Init();

	GPIO_Init();

	RTC_Init();

	RTC_CalendarConfig();

	CAN1_Init();							// Moves CAN from sleep to initialization state

	CAN_Filter_Config();					// Filter config for Rx must be done in initialization state

	uint32_t active_IT = CAN_IT_TX_MAILBOX_EMPTY | CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_BUSOFF;		// interrupts to activate
	if( HAL_CAN_ActivateNotification(&hcan1, active_IT) != HAL_OK)   // Activates one or more interrupts
	{
		Error_handler();
	}

	if(HAL_CAN_Start(&hcan1) != HAL_OK)		// Moves CAN from initialization to normal state
	{
		Error_handler();
	}

	srand(time(NULL));   	// Initialize random seed into rand(); should be called once only

	UART_Msg_Tx("Disc initialization successful\r\n");

	while(1);

	return 0;
}


/**
  * @brief	Clears the Standby and Wakeup flags if controller woke up from Standby mode
  * @param	None
  * @note	Power controller must be on in order to check/change SB and WU flags
  * @note 	If you don't clear the flags below you will then exit the next Standby mode
  * 		as soon as you enter it
  * @retval None
  */

void clear_sleep_flags(void)
{
	__HAL_RCC_PWR_CLK_ENABLE();		// Power controller must be on in order to check/change SB and WU flags

	if( __HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET)
	{
		__HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);	 // Clear the Standby occurred flag
		__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);	 // Clear the wakeup flag

		UART_Msg_Tx("Woke up from Standby mode\r\n");
	}
}


/**
  * @brief	Selects CAN1, configures its properties, and initializes it
  * @param	None
  * @retval None
  */

void CAN1_Init(void)
{
	// CAN1 is hanging on APB1. PCLK1 = SYSCLK/2 = 50M/2 = 25 MHz
	// Prescalar reduces the rate to 25M/5 = 5 MHz
	// CAN timing calc table shows a need of 10 time quanta (TQ) for 1 CAN bit
	// Thus the CAN bit rate is 5M/10 = 500 kbit/s
	// Check CAN timing calc table to see if a certain CAN bit rate can be achieved

	hcan1.Instance = CAN1;
	hcan1.Init.Mode = CAN_MODE_NORMAL;
	hcan1.Init.AutoBusOff = DISABLE;
	hcan1.Init.AutoRetransmission = ENABLE;		// Retransmit message until it is successfully received
	hcan1.Init.AutoWakeUp = DISABLE;			// During message reception, sleep mode is left on software request
	hcan1.Init.ReceiveFifoLocked = DISABLE;  	// Allow message overwrite if receive FIFO is full
	hcan1.Init.TimeTriggeredMode = DISABLE;
	hcan1.Init.TransmitFifoPriority = DISABLE;	// Priority configured to be driven by the identifier of the message

	// Settings related to CAN bit timing
	// Settings to Tx/Rx at 500 kbit/s
	hcan1.Init.Prescaler = 5;
	hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
	hcan1.Init.TimeSeg1 = CAN_BS1_8TQ;
	hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;

	if(HAL_CAN_Init(&hcan1) != HAL_OK)	 // CAN is in sleep state after reset. This API moves it from sleep to initialization state.
	{
		Error_handler();
	}
}


/**
  * @brief	Selects a filter bank (FB0) for hcan1 (CAN1) and sets its properties
  * @param	None
  * @note	No filter mask applied
  * @retval None
  */

void CAN_Filter_Config(void)
{
	CAN_FilterTypeDef can1_filter_init = {0};

	can1_filter_init.FilterActivation = ENABLE;
	can1_filter_init.FilterBank = 0;
	can1_filter_init.FilterFIFOAssignment = CAN_RX_FIFO0;
	can1_filter_init.FilterIdHigh = 0x0000;
	can1_filter_init.FilterIdLow = 0x0000;
	can1_filter_init.FilterMaskIdHigh = 0x0000;
	can1_filter_init.FilterMaskIdLow = 0x0000;
	can1_filter_init.FilterMode = CAN_FILTERMODE_IDMASK;
	can1_filter_init.FilterScale = CAN_FILTERSCALE_32BIT;

	if(HAL_CAN_ConfigFilter(&hcan1, &can1_filter_init) != HAL_OK)
	{
		Error_handler();
	}
}


/**
  * @brief	Disc sends a remote frame using CAN1 to request game statistics from Nucleo
  * @param	None
  * @retval None
  */

void CAN1_Tx(void)
{
	CAN_TxHeaderTypeDef TxHeader = {0};
	uint32_t TxMailbox;					// ID for the selected Tx mailbox will be stored in this variable.
	uint8_t can_msg;

	TxHeader.DLC = 4; 					// Length of message to request (4 bytes)
	TxHeader.StdId = 0x633; 			// Random ID for message is selected. StdId cannot be a value beyond 0x7FF
	TxHeader.IDE = CAN_ID_STD;			// Is ID for standard or extended CAN?
	TxHeader.RTR = CAN_RTR_REMOTE;  	// Request to transmit data frame or remote frame?

	if(HAL_CAN_AddTxMessage(&hcan1, &TxHeader, &can_msg, &TxMailbox) != HAL_OK)	 // Add a message to the first free Tx mailbox and activate the corresponding transmission request
	{
		Error_handler();
	}

	UART_Msg_Tx("Sent Remote Frame to ask for game stats\r\n");
}


/**
  * @brief  Determins the winner of rock, paper, scissors
  * @param  player1 unsigned integer (Nucleo)
  * @param 	player2 unsigned integer (Disc)
  * @retval Unsigned integer with game result: 1 = Nucleo wins, 2 = Disc wins, 3 = a tie
  * 		4 = error occurred
  */

uint8_t Determine_Win(uint8_t player1, uint8_t player2)
{
	if(player1 == 0 && player2 == 1)  		// P1: rock, P2: paper
	{
		return 2;   // P2 wins
	}else if(player1 == 0 && player2 == 2) 	// P1: rock, P2: scissors
	{
		return 1;   // P1 wins
	}else if(player1 == 1 && player2 == 0)	// P1: paper, P2: rock
	{
		return 1;	// P1 wins
	}else if(player1 == 1 && player2 == 2)	// P1: paper, P2: scissors
	{
		return 2;	// P2 wins
	}else if(player1 == 2 && player2 == 0)	// P1: scissors, P2: rock
	{
		return 2;	// P2 wins
	}else if(player1 == 2 && player2 == 1)	// P1: scissors, P2: paper
	{
		return 1;	// P1 wins
	}else if(player1 == player2)
	{
		return 3;	// A tie
	}

	return 4;		// Error indication
}

/**
  * @brief	Disc sends a data frame using CAN1 carrying game result after receiving
  * 		Nucleo's hand
  * @param	Unsigned integer with game result: 1 = Nucleo wins, 2 = Disc wins, 3 = a tie
  * 		4 = error occurred
  * @retval None
  */

void send_game_result(uint8_t winner)
{
	CAN_TxHeaderTypeDef TxHeader = {0};
	uint32_t TxMailbox;						// ID for selected mailbox will be stored in this variable.
	char *game_result[4] = {"Nucleo wins", "Disc wins", "A tie", "Error occurred"};
	char uart_msg[100];

	TxHeader.DLC = 2; 				// Length of message to transmit in bytes
	TxHeader.StdId = 0x111;
	TxHeader.IDE = CAN_ID_STD;		// Is ID for standard or extended CAN?
	TxHeader.RTR = CAN_RTR_DATA;  	// Request to transmit data frame or remote frame?

	if(HAL_CAN_AddTxMessage(&hcan1, &TxHeader, &winner, &TxMailbox) != HAL_OK)	// Add a message to the first free Tx mailbox and activate the corresponding transmission request
	{
		UART_Msg_Tx("send_game_result HAL_CAN_AddTxMessage Tx error\r\n");
		Error_handler();
	}

	sprintf(uart_msg, "Sent message with game result: %s\r\n", game_result[winner-1]);
	UART_Msg_Tx(uart_msg);
}


/**
  * @brief	Rx FIFO 0 message pending callback.
  * @param	hcan pointer to a CAN_HandleTypeDef structure that contains
  *         the configuration information for the specified CAN
  * @retval None
  */

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	uint8_t rcvd_msg[8] = {0};				// CAN frame can contain 8 bytes
	char uart_msg[100];
	char game_stats[100] = {0};
	char *playerspick[3] = {"Rock", "Paper", "Scissors"};
	uint8_t Disc_pick = 0;
	uint8_t winner = 0;

	// Release a message from Rx FIFO0
	if(HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, rcvd_msg) != HAL_OK)
	{
		Error_handler();
	}

	if(RxHeader.StdId == 0x49F && RxHeader.RTR == CAN_RTR_DATA)				// Nucleo sent its hand to Disc
	{
		sprintf(uart_msg, "Message received. Nucleo's hand is %s\r\n", playerspick[rcvd_msg[0]]);

		UART_Msg_Tx(uart_msg);

		Disc_pick = rand() % 3;				// To generate a random integer between 0 to 2 and act as Discovery's pick of rock, paper, scissors
		Disc_pick = rand() % 3;				// Called again to prevent the same hand as Nucleo's to be picked

		sprintf(uart_msg, "Disc's hand is %s\r\n", playerspick[Disc_pick]);

		UART_Msg_Tx(uart_msg);

		winner = Determine_Win(rcvd_msg[0], Disc_pick);		// To determine winner of Rock, Paper, Scissors

		manage_LED_output(winner);			// Turn on the appropriate LED to indicate game result

		send_game_result(winner);			// Disc to send game result to Nucleo

	// StdId cannot be a value beyond 0x7FF
	}else if(RxHeader.StdId == 0x633 && RxHeader.RTR == CAN_RTR_DATA)		// Game stats sent from Nucleo to Disc
	{
		sprintf(game_stats, "STATS: Nucleo Wins: %d, Disc Wins: %d, Ties: %d, Game Error: %d\r\n", rcvd_msg[0], rcvd_msg[1], rcvd_msg[2], rcvd_msg[3]);

		// get_date_time() uses RTC to get current time and return it as a pointer to a string
		UART_Msg_Tx(strcat(get_date_time(), game_stats));					// strcat() returns dest, the pointer to the destination string.

	}else if(RxHeader.StdId == 0x77B && RxHeader.RTR == CAN_RTR_DATA)		// Message from Nucleo to go to sleep
	{
		UART_Msg_Tx("Light lost; gone to sleep\r\n");

		// Configure WAKEUP_PIN1 (PA0) as a wakeup pin in order to exit Standby mode
		// PA0 will be pulled down to ground when configured as WAKEUP_PIN1
		// Thus a rising edge signal to PA0 will wake up the MCU from Standby mode
		HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

		HAL_PWR_EnterSTANDBYMode();		// Enters Standby mode using WFI
	}
}


/**
  * @brief	Returns current date and time from RTC
  * @param	None
  * @retval Pointer to the start of the string carrying date and time info
  */

char* get_date_time(void)
{
	RTC_TimeTypeDef RTC_TimeRead = {0};
	RTC_DateTypeDef RTC_DateRead = {0};
	char *TimeFormat[] = {"AM", "PM"};

	if( HAL_RTC_GetTime(&hrtc, &RTC_TimeRead, RTC_FORMAT_BIN) != HAL_OK)
	{
		Error_handler();
	}

	if( HAL_RTC_GetDate(&hrtc, &RTC_DateRead, RTC_FORMAT_BIN) != HAL_OK)
	{
		Error_handler();
	}

	sprintf(DateTime_Info, "20%02d-%02d-%02d %02d:%02d:%02d %s - ", RTC_DateRead.Year, RTC_DateRead.Month, RTC_DateRead.Date, \
			RTC_TimeRead.Hours, RTC_TimeRead.Minutes, RTC_TimeRead.Seconds, TimeFormat[RTC_TimeRead.TimeFormat/0x40]);

	return DateTime_Info;
}


/**
  * @brief  Error CAN callback.
  * @param  hcan pointer to a CAN_HandleTypeDef structure that contains
  *         the configuration information for the specified CAN.
  * @retval None
  */

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
	UART_Msg_Tx("CAN Error Occurred\r\n");
}


/**
  * @brief  Selects TIM6, configures its properties, and initializes it. This API also
  * 		starts time base generation in interrupt mode using the initialized TIM6
  * @param  None
  * @retval None
  */

void Timer6_Init(void)
{
	// Timer period will change if SYSCLK changes
	// TIM6_CLK =  PCLK1 * 2 = SYSCLK
	// To select TIM6 and configure its period for 1 ms
	htimer6.Instance = TIM6;
	htimer6.Init.Prescaler = 49;
	htimer6.Init.Period = 1000-1;  // Subtract one to ensure an update event is generated at exactly the time base needed

	// CounterMode is not configured b/c TIM6 (basic timer) can only count up which is the default mode

	if(HAL_TIM_Base_Init(&htimer6) != HAL_OK)
	{
		Error_handler();
	}

	// Start the timer base generation in interrupt mode
	HAL_TIM_Base_Start_IT(&htimer6);
}


/**
  * @brief  Period elapsed callback for TIM6 in non-blocking mode. This callback
  * 		will execute every 1ms to check if the user button is pressed. If
  * 		the new button state persists (pressed) then CAN1_Tx() is called.
  * 		This is a way to resolve button debouncing problem.
  * @param  htim pointer to a TIM_HandleTypeDef structure that contains
  *         the configuration information for the specified TIM (TIM6)
  * @retval None
  */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	uint8_t btn_state;		// State of user button that is connected to PA0

	btn_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

	if(btn_state == GPIO_PIN_SET)	// Button pressed; PA0 is high
	{
		debounce_cnt++;
	}else
	{
		debounce_cnt = 0;
	}

	// Once pin reading stabilizes at high then transmit CAN message

	if(debounce_cnt == 100)			// 100 consecutive milliseconds passed and btn_state is still pressed
	{
		debounce_cnt = 0;

		CAN1_Tx();
	}
}


/**
  * @brief  Selects RTC, configures its properties, and initializes it.
  * @param  None
  * @retval None
  */

void RTC_Init(void)
{
	char uart_msg[100] = {0};

	hrtc.Instance = RTC;
	hrtc.Init.HourFormat = RTC_HOURFORMAT_12;
	hrtc.Init.AsynchPrediv = 0x7F;  			// 127
	hrtc.Init.SynchPrediv = 0xFF;	    		// 255
	hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
	hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;		// Does not matter b/c output is disabled
	hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;			// Does not matter b/c output is disabled

	uint8_t rtc_status = HAL_RTC_Init(&hrtc);

	if( rtc_status != HAL_OK)
	{
		sprintf(uart_msg,"RTC init error: %d\r\n",hrtc.State);
		UART_Msg_Tx(uart_msg);

		Error_handler();
	}
}


/**
  * @brief  Configure the RTC calendar for the following: 4:02:00 PM, 01 Feb 2020 Saturday
  * @param  None
  * @retval None
  */

void RTC_CalendarConfig(void)
{
	RTC_TimeTypeDef	RTC_TimeInit = {0};
	RTC_DateTypeDef RTC_DateInit = {0};

	RTC_TimeInit.Hours = 4;
	RTC_TimeInit.Minutes = 02;
	RTC_TimeInit.Seconds = 00;
	RTC_TimeInit.TimeFormat = RTC_HOURFORMAT12_PM;

	if( HAL_RTC_SetTime(&hrtc, &RTC_TimeInit, RTC_FORMAT_BIN) != HAL_OK)
	{
		UART_Msg_Tx("RTC SetTime error\r\n");

		Error_handler();
	}

	RTC_DateInit.Date = 1;
	RTC_DateInit.Month = RTC_MONTH_FEBRUARY;
	RTC_DateInit.WeekDay = RTC_WEEKDAY_SATURDAY;
	RTC_DateInit.Year = 20;

	if( HAL_RTC_SetDate(&hrtc, &RTC_DateInit, RTC_FORMAT_BIN) != HAL_OK)
	{
		UART_Msg_Tx("RTC SetDate error\r\n");

		Error_handler();
	}
}


/**
  * @brief  Configures and initializes GPIOs for Disc's user button and 4x LEDs
  * @param  None
  * @retval None
  */

void GPIO_Init(void)
{
	GPIO_InitTypeDef btn_gpio = {0};
	GPIO_InitTypeDef led_gpio = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();			// GPIO clock must be enabled before configuring GPIO parameters
	__HAL_RCC_GPIOD_CLK_ENABLE();

	// User Button (Request game stats) --> PA0
	btn_gpio.Pin = GPIO_PIN_0;
	btn_gpio.Mode = GPIO_MODE_INPUT;	// PA0 is pulled down when button is not pressed
	btn_gpio.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &btn_gpio);

	// Configure GPIOs for the LEDs on Node 2
	// LED1 (G)--> PD12, LED2 (O) --> PD13, LED3 (R) --> PD14, LED4 (B) --> PD15
	led_gpio.Mode = GPIO_MODE_OUTPUT_PP;
	led_gpio.Pull = GPIO_NOPULL;

	led_gpio.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &led_gpio);
}


/**
  * @brief  Turn on the appropriate LED to indicate game result
  * @param  Unsigned integer with the LED ID for the appropriate switch case
  * @retval None
  */

void manage_LED_output(uint8_t LED_ID)
{
	switch(LED_ID)
	{
		case 1:	// Turn on LED1 (G); rest is off
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
			break;

		case 2:	// Turn on LED2 (O); rest is off
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
			break;

		case 3:	// Turn on LED3 (R); rest is off
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
			break;

		case 4:	// Turn on LED4 (B); rest is off
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
			break;

		default:
			break;
	}
}


/**
  * @brief  Selects UART2, configures its properties, and initializes it
  * @param  None
  * @retval None
  */

void UART2_Init(void)
{
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;

	if(HAL_UART_Init(&huart2) != HAL_OK)
	{
		//There is a problem. You can blink a red LED or trap in error handler
		Error_handler();
	}
}


/**
  * @brief  Sending UART message in blocking mode
  * @param  msg[] message string
  * @retval HAL status
  */

uint8_t UART_Msg_Tx(char msg[])
{
	uint8_t Tx_Status = 0;

	Tx_Status = HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)(strlen(msg)), HAL_MAX_DELAY);
	return Tx_Status;
}


/**
  * @brief  Enables HSE clock and PLL engine. Configures PLL clock source, multiplication and division factors.
  * 		Selects PLL as SYSCLK source, sets latency, and configures prescalars of HCLK, PCLK1, and PCLK2.
  * 		Re-configures SYSTICK because HCLK frequency changes
  * @param  ClkFreq unsigned integer with SYSCLK frequency desired
  * @retval None
  */

void SysClockConfig_HSE(uint8_t ClkFreq)
{
	RCC_OscInitTypeDef osc_init = {0};
	RCC_ClkInitTypeDef clk_init = {0};
	uint8_t flash_latency = 0;

	osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	osc_init.HSEState = RCC_HSE_BYPASS;					// New state for oscillator
	osc_init.PLL.PLLState = RCC_PLL_ON;					// New state for PLL
	osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;			// To source PLL via HSI


	clk_init.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | \
						 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;  		// To configure all these clocks

	clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;  						// To select system clock source

	// Configure PLL engine parameters; SYSCLK = ((HSI/M)*N)/P
	// PLLM, N, P, Q, and R have to be kept within their acceptable range
	// Check the acceptable range in PLL configuration register (RCC_PLLCFGR)

	switch(ClkFreq)
	{
		case SYSCLK_FREQ_50MHZ:
			osc_init.PLL.PLLM = 8;
			osc_init.PLL.PLLN = 100;
			osc_init.PLL.PLLP = 2;
			osc_init.PLL.PLLQ = 2;							// Default value; not needed for this exercise

			clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;		// Configuring prescalar for HCLK
			clk_init.APB1CLKDivider = RCC_HCLK_DIV2;		// Configuring prescalar for PCLK1
			clk_init.APB2CLKDivider = RCC_HCLK_DIV2;		// Configuring prescalar for PCLK2

			flash_latency = FLASH_ACR_LATENCY_1WS;

			break;

		case SYSCLK_FREQ_180MHZ:	// Not used in this exercise

			// To use 180 MHz, we have to set voltage scale to 1 and turn on over drive in power controller
			__HAL_RCC_PWR_CLK_ENABLE();										// Turn on power controller clock
			__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

			osc_init.PLL.PLLM = 8;
			osc_init.PLL.PLLN = 360;
			osc_init.PLL.PLLP = 2;
			osc_init.PLL.PLLQ = 2;

			clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;		// Configuring prescalar for HCLK
			clk_init.APB1CLKDivider = RCC_HCLK_DIV4;		// Configuring prescalar for PCLK1
			clk_init.APB2CLKDivider = RCC_HCLK_DIV2;		// Configuring prescalar for PCLK2

			flash_latency = FLASH_ACR_LATENCY_5WS;

			break;

		default:
			return;
	}

	if(HAL_RCC_OscConfig(&osc_init) != HAL_OK)				// Enables HSE clock and PLL engine. Configures PLL clock source, multiplication and division factors
	{
		Error_handler();
	}

	if(HAL_RCC_ClockConfig(&clk_init, flash_latency) != HAL_OK)		// Selects PLL as SYSCLK source, sets latency, and configures prescalars of HCLK, PCLK1, and PCLK2
	{
		Error_handler();
	}

	// Redo the SYSTICK configuration because HCLK frequency has changed
	// HCLK feeds to Cortex System timer. Note there is a prescalar in between which can be configured

	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);		// Configuring the prescalar between the HCLK and SYSTICK timer
}


/**
  * @brief  Trap for all error occurred in this app. Disc's blue LED will also  illuminate
  * 		periodically whenever this API is entered.
  * @param  None
  * @retval None
  */

void Error_handler(void)
{
	while(1)
	{
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
		HAL_Delay(500);		// in milliseconds
	}
}
