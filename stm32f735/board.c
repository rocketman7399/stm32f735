/* board.c - STM32H7 DTCM 极简专用版 */
/* 适配：Keil Target IRAM1 Start: 0x20000000, Size: 0x20000 */

#include <rtthread.h>
#include <rthw.h>
#include "stm32h7xx_hal.h"

extern void SystemClock_Config(void);
extern int MX_USART3_UART_Init(void);

/* ============================================================= */
/* 内存堆配置 (Heap Config) - 关键修改点 */
/* ============================================================= */
/* * 既然都在 DTCM (0x20000000) 运行，堆也在 DTCM 里分配。
 * H735 的 DTCM 有 128KB，减去静态变量，剩下的给 malloc 用足够了。
 */
extern int Image$$RW_IRAM1$$ZI$$Limit; 
#define HEAP_BEGIN  ((void *)&Image$$RW_IRAM1$$ZI$$Limit)

/* * 这里的 0x20020000 是 DTCM 的结束地址 (Start 0x20000000 + Size 128KB)
 * 务必确认你的 Keil Target 里 IRAM1 Size 是 0x20000
 */
#define HEAP_END    (void *)(0x20020000) 

/* ============================================================= */
/* 板级初始化 */
/* ============================================================= */
void rt_hw_board_init(void)
{
    /* 1. 关闭非对齐访问陷阱 (即使在 DTCM 也要关，防止第三方库报错) */
    SCB->CCR &= ~SCB_CCR_UNALIGN_TRP_Msk;

    /* 2. MPU 配置：完全不需要！DTCM 不需要 MPU 就能全速运行 */
    // MPU_Config(); // <-- 删掉，DTCM 不需要这个累赘

    /* 3. 开启 FPU (浮点运算单元) */
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
        SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));
    #endif

    /* 4. HAL 库初始化 */
    HAL_Init();
    
    /* 5. 时钟初始化 (调用 main.c 里的那个 SMPS 版) */
    SystemClock_Config();

    /* 6. 系统滴答定时器 */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / RT_TICK_PER_SECOND);
    HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);

    /* 7. 初始化堆内存 (都在 DTCM 里，没有大峡谷了) */
    rt_system_heap_init(HEAP_BEGIN, HEAP_END);

    /* 8. 初始化调试串口 */
    MX_USART3_UART_Init();

#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
    
    rt_kprintf("\r\n[Memory] Running in DTCM (0x20000000)\r\n");
    rt_kprintf("[Heap]   From: 0x%08X To: 0x%08X\r\n", HEAP_BEGIN, HEAP_END);
}

/* SysTick 中断 */
void SysTick_Handler(void)
{
    rt_interrupt_enter();
    rt_tick_increase();
    rt_interrupt_leave();
    HAL_IncTick();
}