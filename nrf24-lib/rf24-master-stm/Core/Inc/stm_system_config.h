/**
 * This configuration in general term
 * - Delay us second function
 * - General things
 */

#ifndef __STM_SYSTEM_CONFIG__
#define __STM_SYSTEM_CONFIG__

#include "stm32f1xx.h"

/** DATA WATCH POINT TRACE (DWT) CYCLE COUNTER (DWT->CYCCNT)
 * It not depend on timer/interrupt/compiler, it is cycle cpu clock with CPU freq 
 * 
 * data watch point is a part of ARM cortex design inside ARM chip. it's not peripheral like timer
 * or other stuff. So that make it have delay on 0s and dont slow down perf of system
 * 
 * SYSCLK → AHB Prescaler → HCLK → CPU core
*/

void DWT_Init(void);
void delay_us(uint32_t us);

#endif //__STM_SYSTEM_CONFIG__