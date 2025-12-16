/* main.c - STM32H735G-DK 全功能修复版 */
/* 运行环境：DTCM (IRAM1 Start: 0x20000000, Size: 0x20000) */

#include "stm32h7xx_hal.h"
#include "rtthread.h"
#include "board.h"

/* =========================================================== */
/* 全局串口句柄定义 */
/* =========================================================== */
UART_HandleTypeDef huart3; /* Console (ST-Link USB) */
UART_HandleTypeDef huart7; /* Arduino D0/D1 */
UART_HandleTypeDef huart1; /* Pmod 接口 */

/* =========================================================== */
/* 1. 时钟与电源配置 (救砖关键点) */
/* =========================================================== */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    /* STM32H735G-DK 必须使用 SMPS 供电！*/
    HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY); 
    
    /* 电压调节配置 */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* 使用 HSI (内部高速时钟 64MHz)，最稳，不依赖外部晶振 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE; /* 关闭 PLL，先求稳 */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        while(1); /* 时钟错误死循环 */
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
/* 2. 串口初始化函数 */
/* =========================================================== */

/* Console: USART3 (PD8/PD9) -> 对应 ST-Link 虚拟串口 COM12 */
int MX_USART3_UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE(); 

    /* PD8=TX, PD9=RX */
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
/* 注册自动初始化，RT-Thread 启动时会自动调用 */
INIT_BOARD_EXPORT(MX_USART3_UART_Init);


/* Arduino 接口: UART7 (PF7/PF6) -> 对应 D0/D1 */
void MX_UART7_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_UART7_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /* 注意：根据原理图，PF7=TX, PF6=RX */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_UART7;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    huart7.Instance = UART7;
    huart7.Init.BaudRate = 9600; /* Arduino 常用 9600 */
    huart7.Init.WordLength = UART_WORDLENGTH_8B;
    huart7.Init.StopBits = UART_STOPBITS_1;
    huart7.Init.Parity = UART_PARITY_NONE;
    huart7.Init.Mode = UART_MODE_TX_RX;
    huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    if (HAL_UART_Init(&huart7) != HAL_OK)
    {
        rt_kprintf("UART7 Init Error!\n");
    }
}

/* Pmod 接口: USART1 (PB14/PB15) */
void MX_USART1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB14=TX, PB15=RX */
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        rt_kprintf("USART1 Init Error!\n");
    }
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

    /* 2. 手动初始化剩下的两个功能串口 */
    /* USART3 已经在 board.c 阶段通过 INIT_BOARD_EXPORT 自动初始化了 */
    MX_UART7_Init();
    MX_USART1_Init();

    /* 3. 打印启动信息 */
    rt_kprintf("\n============================================\n");
    rt_kprintf(" STM32H735 Full Functionality Restored! \n");
    rt_kprintf(" Power Mode: SMPS (Safe for DK Board)   \n");
    rt_kprintf(" Memory:     DTCM (0x20000000)          \n");
    rt_kprintf(" UARTs:      USART3(Console), UART7, USART1\n");
    rt_kprintf("============================================\n");
    rt_kprintf("type 'uart_test' to verify ports.\n");

    /* 4. 主循环：闪灯 */
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_2 | GPIO_PIN_3);
        rt_thread_mdelay(500); /* 延时 500ms */
    }
}

/* =========================================================== */
/* 4. 测试命令：一键测试所有串口 */
/* =========================================================== */
void uart_test(void)
{
    char msg_console[] = "Test from Console (USART3)\r\n";
    char msg_arduino[] = "Test from Arduino (UART7 9600)\r\n";
    char msg_pmod[]    = "Test from Pmod (USART1 115200)\r\n";

    rt_kprintf("Sending data to all ports...\n");

    /* 1. 发送给 Console */
    HAL_UART_Transmit(&huart3, (uint8_t*)msg_console, sizeof(msg_console)-1, 100);

    /* 2. 发送给 Arduino (D1) */
    if(HAL_UART_Transmit(&huart7, (uint8_t*)msg_arduino, sizeof(msg_arduino)-1, 100) != HAL_OK)
        rt_kprintf("UART7 (Arduino) Send Failed!\n");
    else
        rt_kprintf("UART7 (Arduino) Send OK!\n");

    /* 3. 发送给 Pmod */
    if(HAL_UART_Transmit(&huart1, (uint8_t*)msg_pmod, sizeof(msg_pmod)-1, 100) != HAL_OK)
        rt_kprintf("USART1 (Pmod) Send Failed!\n");
    else
        rt_kprintf("USART1 (Pmod) Send OK!\n");
}
/* 导出命令到 msh */
MSH_CMD_EXPORT(uart_test, Send string to all UARTs);

