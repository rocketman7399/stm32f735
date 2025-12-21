/* board.c - STM32H7 Nano 适配版 */
#include <rtthread.h>
#include <rthw.h>
#include "stm32h7xx_hal.h"

/* 引用 main.c 定义的串口句柄 */
extern UART_HandleTypeDef huart3; 
extern void SystemClock_Config(void);
extern int MX_USART3_UART_Init(void);

/* 堆内存定义 */
extern int Image$$RW_IRAM1$$ZI$$Limit; 
#define HEAP_BEGIN  ((void *)&Image$$RW_IRAM1$$ZI$$Limit)
#define HEAP_END    (void *)(0x20020000) 

/* 1. 板级初始化 */
void rt_hw_board_init(void)
{
    /* 关闭非对齐陷阱 */
    SCB->CCR &= ~SCB_CCR_UNALIGN_TRP_Msk;

    /* 开启 FPU */
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
        SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));
    #endif

    HAL_Init();
    SystemClock_Config();

    /* SysTick 配置 */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / RT_TICK_PER_SECOND);
    HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);

    /* 堆初始化 */
    rt_system_heap_init(HEAP_BEGIN, HEAP_END);

    /* 初始化调试串口 (UART3) */
    MX_USART3_UART_Init();

#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
    
    rt_kprintf("\r\n[Board] Nano Mode Init OK (No device.c)\r\n");
}

/* 2. SysTick 中断 */
void SysTick_Handler(void)
{
    rt_interrupt_enter();
    rt_tick_increase();
    rt_interrupt_leave();
    HAL_IncTick();
}

/* 3. [关键] 手动实现控制台输出 (替代 Serial 驱动) */
void rt_hw_console_output(const char *str)
{
    rt_size_t i = 0, size = 0;
    char a = '\r';

    size = rt_strlen(str);
    for (i = 0; i < size; i++)
    {
        if (*(str + i) == '\n')
        {
            HAL_UART_Transmit(&huart3, (uint8_t *)&a, 1, 10);
        }
        HAL_UART_Transmit(&huart3, (uint8_t *)(str + i), 1, 10);
    }
}

/* 4. [关键] 手动实现控制台输入 (给 Shell 用) */
char rt_hw_console_getchar(void)
{
    int ch = -1;
    uint8_t data;
    
    /* 检查是否有数据，不阻塞 */
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
    {
        HAL_UART_Receive(&huart3, &data, 1, 0);
        ch = data;
    }
    else
    {
        rt_thread_mdelay(10); /* 释放CPU */
    }
    return ch;
}
