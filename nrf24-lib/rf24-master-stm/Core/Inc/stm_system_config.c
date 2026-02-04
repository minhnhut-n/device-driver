/**
 * This configuration in general term
 * - Delay us second function
 * - General things
 */

#include "stm_system_config.h"

/** DATA WATCH POINT TRACE (DWT) CYCLE COUNTER (DWT->CYCCNT)
 * It not depend on timer/interrupt/compiler, it is cycle cpu clock with CPU freq 
 * 
 * data watch point is a part of ARM cortex design inside ARM chip. it's not peripheral like timer
 * or other stuff. So that make it have delay on 0s and dont slow down perf of system
 * 
 * SYSCLK → AHB Prescaler → HCLK → CPU core
*/

void DWT_Init(void) {
    
    //enable TRC
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    // Reset counter
    DWT->CYCCNT = 0;

    // Enable cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    //current system core clock is 8Mhz
    //cycle count = ticks
    uint32_t ticks = us * (SystemCoreClock / 10e6);

    while ((DWT->CYCCNT - start) < ticks);
    //meet cycle
}
