/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines and includes of the
  *                   rock paper scissors application.
  */

/* Define to prevent recursive inclusion */
#ifndef __MAIN_H
#define __MAIN_H


// Includes
#include <stdio.h>
#include "string.h"
#include <time.h>
#include <stdlib.h>
#include "stm32f4xx_hal.h"


// Defines
#define FALSE		0
#define TRUE		1
// Various system clock frequencies to possibly use
#define SYSCLK_FREQ_50MHZ		50
#define SYSCLK_FREQ_84MHZ		84
#define SYSCLK_FREQ_120MHZ		120
#define SYSCLK_FREQ_180MHZ		180


#endif /* __MAIN_H */
