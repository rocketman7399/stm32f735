/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-17     bx           the first version
 * 2025-12-14     Gemini       Fixed for STM32H7 & UART3 (COM12)
 */

#include <rtthread.h>
#include <rthw.h>
/* 包含 HAL 库头文件，确保能识别 UART_HandleTypeDef */
#include "stm32h7xx_hal.h" 

/* * ⚠️ 关键修改点 1：声明 huart3
 * COM12 (ST-Link) 物理连接的是 USART3
 */
extern UART_HandleTypeDef huart3; 

/* * 函数名：rt_hw_console_output
 * 功  能：重定向 rt_kprintf 的输出到串口3
 */
void rt_hw_console_output(const char *str)
{
    rt_size_t i = 0, size = 0;
    char a = '\r';

    /* ⚠️ 关键修改点 2：解锁 huart3 */
    __HAL_UNLOCK(&huart3);

    size = rt_strlen(str);
    
    for (i = 0; i < size; i++)
    {
        if (*(str + i) == '\n')
        {
            /* 遇到换行符，先发一个回车 \r */
            /* ⚠️ 关键修改点 3：发送给 huart3 */
            HAL_UART_Transmit(&huart3, (uint8_t *)&a, 1, 1000);
        }
        /* 发送当前字符给 huart3 */
        HAL_UART_Transmit(&huart3, (uint8_t *)(str + i), 1, 1000);
    }
}

/* * 函数名：rt_hw_console_getchar
 * 功  能：从串口3获取字符（用于 Shell 输入）
 */
char rt_hw_console_getchar(void)
{
    /* ch 初始值必须小于 0，表示没有读到数据 */
    int ch = -1;

    /* ⚠️ 关键修改点 4：查询 huart3 的 RXNE 标志位 */
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
    {
        /* 读取 huart3 的 RDR 寄存器 */
        ch = (int)(huart3.Instance->RDR & 0xFF);
    }
    else
    {
        /* ⚠️ 关键修改点 5：清除 huart3 的溢出错误 */
        if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE))
        {
            __HAL_UART_CLEAR_OREFLAG(&huart3);
        }
    }

    return ch;
}




