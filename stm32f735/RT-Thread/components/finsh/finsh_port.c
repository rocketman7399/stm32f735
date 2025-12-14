/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-17     bx           the first version
 * 2025-12-14     Gemini       Fixed for STM32H7 & Keil Linker Error
 */

#include <rtthread.h>
#include <rthw.h>
/* * 1. ?? stm32h7xx_hal.h ?? main.h 
 * ?????????? UART_HandleTypeDef ??? 
 */
#include "stm32h7xx_hal.h" 

/* * 2. ????????
 * ?????? UART3,????? huart1 ?? huart3 
 */
extern UART_HandleTypeDef huart1; 

/* * 3. ??????
 * ????:??? RT_WEAK,???? kservice.c ?????
 * ?? Error: L6200E: Symbol rt_hw_console_output multiply defined
 */
void rt_hw_console_output(const char *str)
{
    rt_size_t i = 0, size = 0;
    char a = '\r';

    /* ????? HAL ?????,????? */
    __HAL_UNLOCK(&huart1);

    size = rt_strlen(str);
    
    for (i = 0; i < size; i++)
    {
        if (*(str + i) == '\n')
        {
            /* ????? \n,???????? \r */
            HAL_UART_Transmit(&huart1, (uint8_t *)&a, 1, 1000);
        }
        /* ?????? */
        HAL_UART_Transmit(&huart1, (uint8_t *)(str + i), 1, 1000);
    }
}

/* * 4. ??????
 * ????:??? RT_WEAK
 */
char rt_hw_console_getchar(void)
{
    /* ch ??????? 0 */
    int ch = -1;

    /* ????:?? RXNE ??? (?????????) */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE))
    {
        /* ????????? (RDR) */
        ch = (int)(huart1.Instance->RDR & 0xFF);
    }
    else
    {
        /* ?????? (Overrun Error),???? */
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE))
        {
            __HAL_UART_CLEAR_OREFLAG(&huart1);
        }
    }

    return ch;
}

/* * 5. ????????? (???) 
 */
