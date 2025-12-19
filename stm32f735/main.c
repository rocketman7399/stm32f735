/* main.c - STM32H735G-DK 手动测试版 (SMPS供电 + 全串口开启) */
/* 运行环境：DTCM (IRAM1 Start: 0x20000000, Size: 0x20000) */

#include "stm32h7xx_hal.h"
#include "rtthread.h"
#include "board.h"

/* =========================================================== */
/* 全局串口句柄定义 */
/* =========================================================== */
UART_HandleTypeDef huart3; /* Console (ST-Link USB) */
UART_HandleTypeDef huart1; /* Arduino 接口 (D0/D1) */
UART_HandleTypeDef huart7; /* PMOD 接口 (左上角) */
UART_HandleTypeDef huart2; /* STMod+ 接口 (Fan-out小板子) */

/* =========================================================== */
/* 1. 时钟与电源配置 (保持 SMPS 供电，防止变砖) */
/* =========================================================== */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 【关键】STM32H735G-DK 必须使用 SMPS 供电！ */
    HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY); 
    
    /* 电压调节配置 */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* 使用 HSI (内部高速时钟 64MHz) */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE; 
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        while(1); 
    }

    /* 总线时钟分频 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                                |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        while(1);
    }
}

/* =========================================================== */
/* 2. 串口初始化函数 (开启所有物理接口) */
/* =========================================================== */

/* 1. Console: USART3 (PD8/PD9) -> ST-Link 虚拟串口 */
int MX_USART3_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE(); 

    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    if (HAL_UART_Init(&huart3) != HAL_OK) return -1;
    return 0;
}
INIT_BOARD_EXPORT(MX_USART3_UART_Init);


/* 2. Arduino接口: USART1 (PB14/PB15) -> 对应 D0/D1 */
void MX_USART1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB14=TX (D1), PB15=RX (D0) */
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200; /* 统一使用 115200 */
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(&huart1);
}

/* 3. Pmod接口: UART7 (PF7/PF6) -> 左上角黑座 */
void MX_UART7_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_UART7_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /* PF7=TX, PF6=RX */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_UART7;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    huart7.Instance = UART7;
    huart7.Init.BaudRate = 115200; /* 统一使用 115200，之前是9600 */
    huart7.Init.WordLength = UART_WORDLENGTH_8B;
    huart7.Init.StopBits = UART_STOPBITS_1;
    huart7.Init.Parity = UART_PARITY_NONE;
    huart7.Init.Mode = UART_MODE_TX_RX;
    huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(&huart7);
}

/* 4. STMod+接口: USART2 (PD5/PD6) -> Fan-out小板子 */
void MX_USART2_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* PD5=TX, PD6=RX */
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200; /* 统一使用 115200 */
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(&huart2);
}

/* =========================================================== */
/* 3. Main 主函数 */
/* =========================================================== */
int main(void)
{
    /* 1. 初始化 LED (PC2/PC3) */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* 2. 初始化所有物理串口 */
    MX_USART1_Init(); // D0/D1
    MX_UART7_Init();  // PMOD
    MX_USART2_UART_Init(); // STMod+ (小板子)

    /* 3. 打印启动信息 */
    rt_kprintf("\n==============================================\n");
    rt_kprintf(" STM32H735 All Ports Manual Test Mode\n");
    rt_kprintf(" Power: SMPS | Baud: 115200 (ALL)\n");
    rt_kprintf(" Active Ports:\n");
    rt_kprintf(" 1. USART3 (Console)\n");
    rt_kprintf(" 2. USART1 (D0/D1)\n");
    rt_kprintf(" 3. UART7  (PMOD)\n");
    rt_kprintf(" 4. USART2 (STMod+ Fan-out)\n");
    rt_kprintf("==============================================\n");
    rt_kprintf("Enter 'uart_test' to trigger manual send.\n");

    /* 4. 主循环：只闪灯，静默等待命令 */
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_2 | GPIO_PIN_3);
        rt_thread_mdelay(500); 
    }
}

/* =========================================================== */
/* 4. 测试命令：手动触发发送 */
/* =========================================================== */
void uart_test(void)
{
    rt_kprintf(">>> Firing on ALL ports... <<<\n");

    /* 消息定义 */
    char msg_console[] = "1. Hello from Console (USART3)\r\n";
    char msg_d0d1[]    = "2. Hello from D0/D1 (USART1)\r\n";
    char msg_pmod[]    = "3. Hello from PMOD (UART7)\r\n";
    char msg_stmod[]   = "4. Hello from STMod+ (USART2)\r\n";

    /* 1. 发送给 Console (USART3) */
    HAL_UART_Transmit(&huart3, (uint8_t*)msg_console, sizeof(msg_console)-1, 100);

    /* 2. 发送给 D0/D1 (USART1) */
    HAL_UART_Transmit(&huart1, (uint8_t*)msg_d0d1, sizeof(msg_d0d1)-1, 100);

    /* 3. 发送给 PMOD (UART7) */
    HAL_UART_Transmit(&huart7, (uint8_t*)msg_pmod, sizeof(msg_pmod)-1, 100);
    
    /* 4. 发送给 STMod+ Fan-out (USART2) */
    HAL_UART_Transmit(&huart2, (uint8_t*)msg_stmod, sizeof(msg_stmod)-1, 100);
    
    rt_kprintf(">>> Send Complete. Check your SSCOM! <<<\n");
}
/* 导出命令 */
MSH_CMD_EXPORT(uart_test, Send string to all UARTs);
