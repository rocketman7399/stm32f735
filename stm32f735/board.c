/* board.c - STM32H7 DTCM 适配版 */

#include <rtthread.h>
#include <rthw.h>
#include "stm32h7xx_hal.h"

extern void SystemClock_Config(void);
extern int MX_USART3_UART_Init(void);

/* 堆内存定义 (使用 DTCM 剩余空间) */
extern int Image$$RW_IRAM1$$ZI$$Limit; 
#define HEAP_BEGIN  ((void *)&Image$$RW_IRAM1$$ZI$$Limit)
#define HEAP_END    (void *)(0x20020000) 

void rt_hw_board_init(void)
{
    /* 1. 关闭非对齐访问陷阱 */
    SCB->CCR &= ~SCB_CCR_UNALIGN_TRP_Msk;

    /* 2. 开启 FPU (重要) */
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
        SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));
    #endif

    /* 3. HAL 初始化与时钟配置 */
    HAL_Init();
    SystemClock_Config();

    /* 4. SysTick 配置 */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / RT_TICK_PER_SECOND);
    HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);

    /* 5. 堆初始化 */
    rt_system_heap_init(HEAP_BEGIN, HEAP_END);

    /* 6. Console 初始化 */
    MX_USART3_UART_Init();

#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
    
    rt_kprintf("\r\n[System] Board Init OK (DTCM)\r\n");
}

void SysTick_Handler(void)
{
    rt_interrupt_enter();
    rt_tick_increase();
    rt_interrupt_leave();
    HAL_IncTick();
}