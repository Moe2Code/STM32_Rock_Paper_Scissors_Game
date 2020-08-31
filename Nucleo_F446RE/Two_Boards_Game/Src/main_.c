/**
  ******************************************************************************
  * @file    main_.c
  * @author  Moe2Code
  * @brief   Main source file for a game of rock, paper, scissors played between two ST boards.
  *          The game features timers, CAN messaging, RTC time/date keeping, game score keeping
  *          in the backup SRAM, and low power management of the boards. The following is conducted
  *          in source file:
  *          + Initialization and configuration for all the peripherals used
  *          + Transmission of Nucleo's hand (rock, paper, or scissors) to Discovery board via CAN
  *          + Reception of message determining the winner of each round via CAN from Discovery
  *          + Transmission of the rolling game score to Discovery via CAN when requested
  *          + Low power management of both boards
  *          + Preservation of rolling game score in backup SRAM
  */

// Includes
#include "main.h"


// Global variables
UART_HandleTypeDef huart2;				// UART2 peripheral handle
CAN_HandleTypeDef hcan1;				// CAN1 peripheral handle
TIM_HandleTypeDef htimer6;				// Timer 6 (TIM6) peripheral handle. TIM6 is a basic timer
CAN_RxHeaderTypeDef RxHeader;			// Stores the header of CAN Rx frame
uint8_t nucleo_wins = 0;				// To store the number of wins for Nucleo so far
uint8_t disc_wins = 0;					// To store the number of wins for Discovery so far
uint8_t tie_count = 0;					// To store the number of tie games occurred so far
uint8_t game_err= 0;					// To store the number of errors occurred for game result
uint8_t *pBKPSRAMbase = (uint8_t*)BKPSRAM_BASE;	  // Pointing to the base address of the backup SRAM


// Function prototypes
void Error_handler(void);
void SysClockConfig_HSE(uint8_t ClkFreq);
void UART2_Init(void);
void CAN1_Init(void);
void GPIO_Init(void);
void CAN1_Tx(void);
void CAN_Filter_Config(void);
void Timer6_Init(void);
void send_game_stats(uint32_t StdId);
uint8_t UART_Msg_Tx(char msg[]);
void store_score_in_bSRAM(uint8_t p1_wins, uint8_t p2_wins, uint8_t game_ties, uint8_t game_err);
void load_bSRAM_score(void);
void send_sleep_msg(void);
void wakeup_disc(void);


/**
  * @brief	Conducts initialization and configuration for all of the peripherals used.
  *         After exiting the low power standby mode, loading score from backup SRAM and
  *         waking up Discovery (Disc) board is also conducted by this function.
  * @param	None
  * @note	None
  * @retval 0
  */

int main(void)
{
	HAL_Init();   // HAL layer initialization

	// PLL via HSE (8 MHz) is used to generate SYSCLK of 50 MHz
	// Used HSE since it is more accurate than HSI
	SysClockConfig_HSE(SYSCLK_FREQ_50MHZ);

	// Initialization and configuration to the used peripherals
	Timer6_Init();

	UART2_Init();

	GPIO_Init();

	// Load the score from the backup SRAM if woke up from standby mode
	load_bSRAM_score();

	// Wake up Disc board if woke up from standby mode
	wakeup_disc();

	CAN1_Init();	// Moves CAN peripheral from sleep to initialization state

	CAN_Filter_Config();	// Filter config for CAN Rx must be done in initialization state

	uint32_t active_IT = CAN_IT_TX_MAILBOX_EMPTY | CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_BUSOFF;	  // Interrupts to activate for CAN

	if(HAL_CAN_ActivateNotification(&hcan1, active_IT) != HAL_OK)   // Activates the CAN interrupts needed
	{
		UART_Msg_Tx("HAL_CAN_ActivateNotification error\r\n");

		Error_handler();   // Go to error handler if the activation of interrupts was not successful
	}

	if(HAL_CAN_Start(&hcan1) != HAL_OK)		// Moves CAN from initialization to normal state
	{
		UART_Msg_Tx("HAL_CAN_Start error\r\n");

		Error_handler();  // Go to error handler if the transfer to normal state was not successful
	}

	srand(time(NULL));   // Initialize random seed into rand(); should be called once only

	UART_Msg_Tx("Nucleo initialization successful\r\n");

	while(1);

	return 0;
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

	hcan1.Init.Prescaler = 5;
	hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
	hcan1.Init.TimeSeg1 = CAN_BS1_8TQ;
	hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;

	if(HAL_CAN_Init(&hcan1) != HAL_OK)	 		// CAN is in sleep state after reset. This API moves it from sleep to initialization state.
	{
		UART_Msg_Tx("HAL_CAN_Init error\r\n");

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
	CAN_FilterTypeDef can1_filter_init;
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
		UART_Msg_Tx("HAL_CAN_ConfigFilter error\r\n");
		Error_handler();
	}
}


/**
  * @brief	Nucleo sends a data frame using CAN1 providing its pick of rock, paper, scissors
  * @param	None
  * @retval None
  */

void CAN1_Tx(void)
{
	CAN_TxHeaderTypeDef TxHeader;
	uint32_t TxMailbox;						// ID for the selected Tx mailbox will be stored in this variable.
	uint8_t can_msg;
	char *playerspick[3] = {"Rock", "Paper", "Scissors"};
	char uart_msg[75];

	can_msg = rand() % 3;					// To generate a random integer between 0 to 2 and act as Nucleo's pick of rock, paper, scissors

	TxHeader.DLC = 1; 						// Length of message to transmit in bytes
	TxHeader.StdId = 0x49F; 				// Random ID for message is selected
	TxHeader.IDE = CAN_ID_STD;				// Is ID for standard or extended CAN?
	TxHeader.RTR = CAN_RTR_DATA;  			// Request to transmit data frame or remote frame?

	if(HAL_CAN_AddTxMessage(&hcan1, &TxHeader, &can_msg, &TxMailbox) != HAL_OK)	 // Add a message to the first free Tx mailbox and activate the corresponding transmission request
	{
		Error_handler();
	}

	sprintf(uart_msg, "Sent message containing Nucleo's hand (%s)\r\n", playerspick[can_msg]);
	UART_Msg_Tx(uart_msg);
}


/**
  * @brief	Nucleo sends a data frame using CAN1 providing game stats to Disc after receiving
  * 		a remote frame from Disc requesting the game stats
  * @param	StdId unsigned integer holding the ID of the frame to send
  * @retval None
  */

void send_game_stats(uint32_t StdId)
{
	CAN_TxHeaderTypeDef TxHeader;
	uint32_t TxMailbox;						// ID for selected mailbox will be stored in this variable.

	// increase to uint16_t later
	uint8_t can_msg[4] = {nucleo_wins, disc_wins, tie_count, game_err};  	// Random values

	TxHeader.DLC = 4; 						// Length of message to transmit in bytes
	TxHeader.StdId = StdId;
	TxHeader.IDE = CAN_ID_STD;				// Is ID for standard or extended CAN?
	TxHeader.RTR = CAN_RTR_DATA;  			// Request to transmit data frame or remote frame?

	if(HAL_CAN_AddTxMessage(&hcan1, &TxHeader, can_msg, &TxMailbox) != HAL_OK)	// Add a message to the first free Tx mailbox and activate the corresponding transmission request
	{
		UART_Msg_Tx("send_response HAL_CAN_AddTxMessage Tx error\r\n");
		Error_handler();
	}

	UART_Msg_Tx("Nucleo sent game stats to Disc\r\n");
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
	char uart_msg[75] = {0};
	char *game_result[4] = {"Nucleo wins", "Disc wins", "A tie", "Error occurred"};


	// Release a message from Rx FIFO0
	if(HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, rcvd_msg) != HAL_OK)
	{
		Error_handler();
	}

	if(RxHeader.StdId == 0x111 && RxHeader.RTR == CAN_RTR_DATA)				// Disc sent game result to Nucleo
	{
		sprintf(uart_msg, "Received message with game result: %s\r\n", game_result[rcvd_msg[0]-1]);

		// Increment score counter
		switch(rcvd_msg[0])
		{
			case 1:
				nucleo_wins++;
				break;
			case 2:
				disc_wins++;
				break;
			case 3:
				tie_count++;
				break;
			case 4:
				game_err++;
				break;
		}

		// Store score in the backup SRAM
		store_score_in_bSRAM(nucleo_wins, disc_wins, tie_count, game_err);

		// StdId cannot be a value beyond 0x7FF
	}else if(RxHeader.StdId == 0x633 && RxHeader.RTR == CAN_RTR_REMOTE) 	// Disc requests game stats from Nucleo
	{
		send_game_stats(RxHeader.StdId);
	}
}


/**
  * @brief  CAN error callback. Error message will be sent via UART to be printed on PC terminal
  * @param  hcan pointer to a CAN_HandleTypeDef structure that contains
  *         the configuration information for the specified CAN.
  * @retval None
  */

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
	UART_Msg_Tx("CAN Error Occurred\r\n");
}


/**
  * @brief  Stores game stats in the backup SRAM
  * @param  p1_wins counter for Nucleo's wins
  * @param  p2_wins counter for Disc's wins
  * @param  game_ties counter for game times
  * @param  game_err counter for game errors
  * @retval None
  */

void store_score_in_bSRAM(uint8_t p1_wins, uint8_t p2_wins, uint8_t game_ties, uint8_t game_errs)
{
	char write_buff[75];

	sprintf(write_buff, "Nucleo Wins: %d, Disc Wins: %d, Ties: %d, Game Error: %d\r\n", p1_wins, p2_wins, game_ties, game_errs);
	UART_Msg_Tx(write_buff);

	// 1. Turn on the clock for the backup SRAM
	__HAL_RCC_BKPSRAM_CLK_ENABLE();

	// 2. Enable write access to the backup domain. The backup SRAM is part of the backup domain and is write-protected by default
	__HAL_RCC_PWR_CLK_ENABLE();   // Power controller configures the write access of the backup domain. Thus, its clock must be on.
	HAL_PWR_EnableBkUpAccess();	  // Enables write access to the backup domain

	// 3. Write the score to the backup SRAM. You can write data at the SRAM's base address and onward
	for(uint8_t i = 0; i < strlen(write_buff)+1; i++)
	{
		*(pBKPSRAMbase + i) = write_buff[i];
	}
}


/**
  * @brief  Loads game stats from the backup SRAM if woke up from Standby mode
  * @param  None
  * @retval None
  */

void load_bSRAM_score(void)
{
	__HAL_RCC_PWR_CLK_ENABLE();  // Power controller must be on in order to check/change SB and WU flags

	if( __HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET)	// PWR_FLAG_SB and PWR_FLAG_WU are cleared in wakeup_disc()
	{
		UART_Msg_Tx("Woke up from Standby mode\r\n");

		// Check to see if the bSRAM data has stored stats

		// Clock to bSRAM must be turned on before attempting to read the stored stats
		__HAL_RCC_BKPSRAM_CLK_ENABLE();

		char text_end = 'a';		// Initialized to a random value
		uint8_t text_length = 0;
		char uart_msg[100];

		// Read length of data stored in the bSRAM
		while(text_end != '\n' && text_length <=255)	// If no stats stored text_length will roll back to 0
		{
			text_end = (char)*(pBKPSRAMbase + text_length);
			text_length++;
		}

		char c = 'a';								// Initialized to a random value
		uint32_t digit = 0;							// Digit extract from string
		uint32_t num = 0;							// Number generated the extracted digits
		uint8_t j =0;								// Counter for the multiple numbers generated (stats)
		uint8_t stats[4] = {0};						// Array to hold the multiple numbers generated (stats)

		for(uint8_t i = 0; i < text_length; i++)
		{
			if(c != ',')
			{
				c = *(pBKPSRAMbase + i);

				if(c >= '0' && c <= '9')
				{
					digit = c - '0';
					num = num*10 + digit;
				}
			}
			else
			{
				stats[j] = num;
				num = 0;
				j++;
				c = *(pBKPSRAMbase + i);
			}
		}

		nucleo_wins = stats[0];
		disc_wins = stats[1];
		tie_count = stats[2];
		game_err = stats[3];

		sprintf(uart_msg, "Loaded Stats - Nucleo Wins: %d, Disc Wins: %d, Ties: %d, Game Error: %d\r\n", nucleo_wins, disc_wins, tie_count, game_err);
		UART_Msg_Tx(uart_msg);
	}
	else
	{
		UART_Msg_Tx("Fresh start; no scores available\r\n");

		// Stat counters already set to zero in global domain
	}
}


/**
  * @brief  Wake up Disc from Standby mode by sending a signal to it via PC5
  * @Note	If you don't clear the SB and WU flags you will then exit the next
  * 		Standby mode as soon as you enter it
  * @param  None
  * @retval None
  */

void wakeup_disc(void)
{
	// Power controller must be on in order to check/change SB and WU flags
	// Turned on already in load_bSRAM_score();

	if( __HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET)
		{
			__HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);	 // Clear the Standby occurred flag
			__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);	 // Clear the wakeup flag

			// Send a rising edge signal using Nucleo's PC5 to Disc's wakeup pin (PA0)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
			HAL_Delay(50);										// Wait for a second
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
		}

	// De-initialize the wakeup signal in order to prevent interference with Disc's user button
	// Disc's user button is used to request game stats. It is connected to PA0 which is also Disc's wakeup pin
	// The API below reconfigures the IO Direction in Input Floating Mode
	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_5);
}


/**
  * @brief  Selects TIM6, configures its properties, and initializes it.
  * @param  None
  * @retval None
  */

void Timer6_Init(void)
{
	// Timer period will change if SYSCLK changes
	// TIM6_CLK =  PCLK1 * 2 = SYSCLK
	// To select TIM6 and configure its period for 4 seconds
	htimer6.Instance = TIM6;
	htimer6.Init.Prescaler = 4999;
	htimer6.Init.Period = 40000-1;  // Subtract one to ensure an update event is generated at exactly the time base needed

	// CounterMode is not configured b/c TIM6 (basic timer) can only count up which is the default mode

	if(HAL_TIM_Base_Init(&htimer6) != HAL_OK)
	{
		Error_handler();
	}
}


/**
  * @brief  Transmits Nucleo's hand to Disc once every 4 seconds
  * @param  htim pointer to a TIM_HandleTypeDef structure that contains
  *         the configuration information for the specified TIM (TIM6)
  * @retval None
  */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	CAN1_Tx();
}


/**
  * @brief  Configures and initializes GPIOs for Nucleo's user button, sleep pin, and
  * 		wakeup pin (to wake up Disc).
  * @param  None
  * @retval None
  */

void GPIO_Init(void)
{
	GPIO_InitTypeDef btn_gpio = {0};
	GPIO_InitTypeDef sleep_gpio = {0};
	GPIO_InitTypeDef wakeup_gpio = {0};

	__HAL_RCC_GPIOC_CLK_ENABLE();			  // GPIO clock must be enabled before configuring GPIO parameters

	// User Button (Start) --> PC13
	btn_gpio.Pin = GPIO_PIN_13;
	btn_gpio.Mode = GPIO_MODE_IT_FALLING;	  // PC13 is pulled up when button is not pressed
	btn_gpio.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &btn_gpio);

	// Set priority and enable interrupt delivery of GPIO PC13
	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 15, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

	/*
	// Simulating input from sensor (aka photoresistor)
	sleep_gpio.Pin = GPIO_PIN_4;
	sleep_gpio.Mode = GPIO_MODE_IT_FALLING;
	sleep_gpio.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOC, &sleep_gpio);
	 */

	// Sleep Input --> PC4
	sleep_gpio.Pin = GPIO_PIN_4;
	sleep_gpio.Mode = GPIO_MODE_IT_RISING;	  // Photoresistor will be connected to PC4
	sleep_gpio.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &sleep_gpio);

	// Set priority and enable interrupt delivery of GPIO PC4
	HAL_NVIC_SetPriority(EXTI4_IRQn, 15, 0);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);

	// Disc Wakeup Signal --> PC5
	wakeup_gpio.Pin = GPIO_PIN_5;
	wakeup_gpio.Mode = GPIO_MODE_OUTPUT_PP;
	wakeup_gpio.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &wakeup_gpio);
}


/**
  * @brief  EXTI line detection callback. If user button pressed (PC13) then start time
  * 		generation using TIM6. If PC4 pulled down then Nucleo will send sleep message
  * 		to Disc and itself will go to sleep (Standby mode)
  * @param  GPIO_Pin Specifies the pins connected EXTI line
  * @retval None
  */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == GPIO_PIN_13)
	{
		UART_Msg_Tx("User button pressed; timer started\r\n");

		// Start the timer base generation in interrupt mode
		HAL_TIM_Base_Start_IT(&htimer6);

	}
	else if(GPIO_Pin == GPIO_PIN_4)
	{
		UART_Msg_Tx("Light lost; gone to sleep\r\n");

		send_sleep_msg();				// Send Disc a CAN message to go to sleep

		__HAL_RCC_PWR_CLK_ENABLE();   	// Enable the power controller in order to configure the backup regulator

		HAL_PWREx_EnableBkUpReg();		// Enables the backup regulator so that the SRAM won't lose power when the processor enters Standby mode

		HAL_PWR_EnterSTANDBYMode();		// Enters Standby mode using WFI

		// MCU will not resume here when waking up. Rather a reset will occur. The code resumes back from the start of main function
	}
}


/**
  * @brief	Nucleo sends a data frame using CAN1 telling Disc to go to sleep (Standby mode)
  * @param	None
  * @retval None
  */

void send_sleep_msg(void)
{
	CAN_TxHeaderTypeDef TxHeader;
	uint32_t TxMailbox;						// ID for selected mailbox will be stored in this variable.
	uint8_t can_msg = 0;					// Message content is irrelevant

	TxHeader.DLC = 1; 						// Length of message to transmit in bytes
	TxHeader.StdId = 0x77B;
	TxHeader.IDE = CAN_ID_STD;				// Is ID for standard or extended CAN?
	TxHeader.RTR = CAN_RTR_DATA;  			// Request to transmit data frame or remote frame?

	if(HAL_CAN_AddTxMessage(&hcan1, &TxHeader, &can_msg, &TxMailbox) != HAL_OK)	// Add a message to the first free Tx mailbox and activate the corresponding transmission request
	{
		UART_Msg_Tx("send_response HAL_CAN_AddTxMessage Tx error\r\n");
		Error_handler();
	}

	UART_Msg_Tx("Nucleo sent sleep message to Disc\r\n");
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
	RCC_OscInitTypeDef osc_init;
	RCC_ClkInitTypeDef clk_init;
	uint8_t flash_latency = 0;

	memset(&osc_init,0, sizeof(osc_init));  // Clearing members of the struct (i.e. clearing garbage values)

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
			osc_init.PLL.PLLQ = 2;						// Default value; not needed for this exercise
			osc_init.PLL.PLLR = 2;						// Default value; not needed for this exercise

			clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;		// Configuring prescalar for HCLK
			clk_init.APB1CLKDivider = RCC_HCLK_DIV2;		// Configuring prescalar for PCLK1
			clk_init.APB2CLKDivider = RCC_HCLK_DIV2;		// Configuring prescalar for PCLK2

			flash_latency = FLASH_ACR_LATENCY_1WS;

			break;

		case SYSCLK_FREQ_180MHZ:	// Not used in this exercise

			// To use 180 MHz, we have to set voltage scale to 1 and turn on over drive in power controller
			__HAL_RCC_PWR_CLK_ENABLE();										// Turn on power controller clock
			__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
			__HAL_PWR_OVERDRIVE_ENABLE();

			osc_init.PLL.PLLM = 8;
			osc_init.PLL.PLLN = 360;
			osc_init.PLL.PLLP = 2;
			osc_init.PLL.PLLQ = 2;
			osc_init.PLL.PLLR = 2;

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
  * @brief  Trap for all error occurred in this app.
  * @param  None
  * @retval None
  */

void Error_handler(void)
{
	while(1);
}
