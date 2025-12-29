/*
 * Project: STM32H7 Crypto Diagnostic (Debug Version)
 * BaudRate: 921600 bps
 * Features: Debug Logs via USART3
 */

#include "stm32h7xx_hal.h"
#include "rtthread.h"
#include "board.h"
#include <string.h>
#include <rthw.h>
#include "stm32h7xx_hal_cryp.h"

#ifdef __FPU_PRESENT
#undef __FPU_PRESENT
#endif

/* =================================================================================
 * 1. 资源定义 (强制 32字节对齐)
 * ================================================================================= */

UART_HandleTypeDef huart3; /* Debug Console */
UART_HandleTypeDef huart1; /* Decrypt Port (D0/D1) */
UART_HandleTypeDef huart7; /* Encrypt Port (D10/D13) */
CRYP_HandleTypeDef hcryp;  /* Hardware Crypto */

/* 临时字节 */
uint8_t rx_byte_u1, rx_byte_u7;

/* 对齐缓冲区 (解决潜在的总线访问问题) */
ALIGN(32) volatile uint8_t buf_u7[16];
ALIGN(32) volatile uint8_t buf_u1[16];

volatile uint8_t cnt_u7 = 0;
volatile uint32_t last_time_u7 = 0;

volatile uint8_t cnt_u1 = 0;
volatile uint32_t last_time_u1 = 0;

rt_sem_t sem_u7 = RT_NULL;
rt_sem_t sem_u1 = RT_NULL;

/* AES Key */
ALIGN(32) static const uint32_t pKeyAES[4] = {
    0x2B7E1516, 0x28AED2A6, 0xABF71588, 0x09CF4F3C
};

/* =================================================================================
 * 2. 硬件初始化 (修复密钥配置)
 * ================================================================================= */

void MX_CRYP_Init(void) {
    __HAL_RCC_CRYP_CLK_ENABLE();

    hcryp.Instance = CRYP;
    hcryp.Init.DataType = CRYP_DATATYPE_8B;
    hcryp.Init.KeySize = CRYP_KEYSIZE_128B;
    hcryp.Init.Algorithm = CRYP_AES_ECB;
    hcryp.Init.pKey = (uint32_t *)pKeyAES; 
    hcryp.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
    
    /* 关键修改：不要设置为 ALWAYS SKIP，否则解密时的密钥准备步骤会被跳过 */
    /* 注释掉下面这行，或者使用默认值 */
    // hcryp.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ALWAYS; 

    if (HAL_CRYP_Init(&hcryp) != HAL_OK) {
        rt_kprintf("[ERR] CRYP Init Failed!\n");
        while(1);
    }
}

/* =================================================================================
 * 3. 线程逻辑 (带调试打印)
 * ================================================================================= */

/* 打印 Hex 辅助函数 */
void print_debug_hex(const char* tag, uint8_t* d, int len) {
    rt_kprintf("[%s] ", tag);
    for(int i=0; i<len; i++) rt_kprintf("%02X ", d[i]);
    rt_kprintf("\n");
}

/* 加密线程 (UART7) */
void thread_u7_entry(void *parameter) {
    ALIGN(32) uint8_t aes_in[16];
    ALIGN(32) uint8_t aes_out[16];
    
    while(1) {
        if (rt_sem_take(sem_u7, RT_WAITING_FOREVER) == RT_EOK) {
            // 拷贝数据
            register rt_base_t level = rt_hw_interrupt_disable();
            memcpy(aes_in, (void*)buf_u7, 16);
            rt_hw_interrupt_enable(level);

            // 调试信息
            // rt_kprintf("[U7] Recv Data, Encrypting...\n");

            // 硬件加密
            if (HAL_CRYP_Encrypt(&hcryp, (uint32_t*)aes_in, 16, (uint32_t*)aes_out, 100) == HAL_OK) {
                HAL_UART_Transmit(&huart7, aes_out, 16, 100);
            } else {
                rt_kprintf("[U7] Hardware Encrypt Error!\n");
                HAL_CRYP_DeInit(&hcryp); // 尝试复位
                MX_CRYP_Init();
            }
        }
    }
}

/* 解密线程 (USART1) - 极速版 */
void thread_u1_entry(void *parameter) {
    ALIGN(32) uint8_t aes_in[16];
    ALIGN(32) uint8_t aes_out[16];
    
    while(1) {
        if (rt_sem_take(sem_u1, RT_WAITING_FOREVER) == RT_EOK) {
            
            // 1. 注释掉调试打印
            // rt_kprintf("[U1] RX OK! Starting Decrypt...\n");
            // print_debug_hex("CIPHER", (uint8_t*)buf_u1, 16);

            register rt_base_t level = rt_hw_interrupt_disable();
            memcpy(aes_in, (void*)buf_u1, 16);
            rt_hw_interrupt_enable(level);

            // 2. 纯硬件解密 (耗时忽略不计)
            if (HAL_CRYP_Decrypt(&hcryp, (uint32_t*)aes_in, 16, (uint32_t*)aes_out, 100) == HAL_OK) {
                
                // 3. 注释掉结果打印
                // rt_kprintf("[U1] Decrypt OK! Sending...\n");
                // print_debug_hex("PLAIN", aes_out, 16);
                
                // 4. 只保留数据回传
                HAL_UART_Transmit(&huart1, aes_out, 16, 100);
            } else {
                // 出错时再打印，平时不打印
                rt_kprintf("ERR\n");
                HAL_CRYP_DeInit(&hcryp);
                MX_CRYP_Init();
            }
        }
    }
}

/* =================================================================================
 * 4. 中断回调
 * ================================================================================= */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint32_t now = HAL_GetTick();

    if (huart->Instance == UART7) {
        if (now - last_time_u7 > 5) cnt_u7 = 0;
        last_time_u7 = now;
        if (cnt_u7 < 16) buf_u7[cnt_u7++] = rx_byte_u7;
        if (cnt_u7 == 16) {
            cnt_u7 = 0; 
            rt_sem_release(sem_u7);
        }
        HAL_UART_Receive_IT(&huart7, &rx_byte_u7, 1);
    }
    else if (huart->Instance == USART1) {
        if (now - last_time_u1 > 5) cnt_u1 = 0;
        last_time_u1 = now;
        if (cnt_u1 < 16) buf_u1[cnt_u1++] = rx_byte_u1;
        if (cnt_u1 == 16) {
            cnt_u1 = 0;
            rt_sem_release(sem_u1);
        }
        HAL_UART_Receive_IT(&huart1, &rx_byte_u1, 1);
    }
}

/* =================================================================================
 * 5. 基础初始化
 * ================================================================================= */

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_DIV1; RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT; 
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE; HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1; RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1; 
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1; RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1; RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1);
}

void UART_Init_Base(UART_HandleTypeDef *huart, USART_TypeDef *instance, uint32_t baud) {
    huart->Instance = instance;
    huart->Init.BaudRate = baud; 
    huart->Init.WordLength = UART_WORDLENGTH_8B;
    huart->Init.StopBits = UART_STOPBITS_1;
    huart->Init.Parity = UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(huart);
}

void MX_USART1_Init(void) {
    __HAL_RCC_USART1_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g={0}; g.Pin=GPIO_PIN_14|GPIO_PIN_15; g.Mode=GPIO_MODE_AF_PP; g.Alternate=GPIO_AF4_USART1;
    HAL_GPIO_Init(GPIOB, &g);
    UART_Init_Base(&huart1, USART1, 921600); 
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0); HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void MX_UART7_Init(void) {
    __HAL_RCC_UART7_CLK_ENABLE(); __HAL_RCC_GPIOF_CLK_ENABLE(); 
    GPIO_InitTypeDef g={0}; g.Pin=GPIO_PIN_7|GPIO_PIN_6; g.Mode=GPIO_MODE_AF_PP; g.Pull=GPIO_PULLUP; g.Alternate=GPIO_AF7_UART7; 
    HAL_GPIO_Init(GPIOF, &g);
    UART_Init_Base(&huart7, UART7, 921600); 
    HAL_NVIC_SetPriority(UART7_IRQn, 1, 0); HAL_NVIC_EnableIRQ(UART7_IRQn);
}

int MX_USART3_UART_Init(void) { 
    __HAL_RCC_USART3_CLK_ENABLE(); __HAL_RCC_GPIOD_CLK_ENABLE(); 
    GPIO_InitTypeDef g={0}; g.Pin=GPIO_PIN_8|GPIO_PIN_9; g.Mode=GPIO_MODE_AF_PP; g.Alternate=GPIO_AF7_USART3; 
    HAL_GPIO_Init(GPIOD,&g); 
    UART_Init_Base(&huart3, USART3, 115200); 
    return 0; 
}
INIT_BOARD_EXPORT(MX_USART3_UART_Init);

int main(void)
{
    HAL_Init(); 
    SystemClock_Config();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g={0}; g.Pin=GPIO_PIN_2|GPIO_PIN_3; g.Mode=GPIO_MODE_OUTPUT_PP; HAL_GPIO_Init(GPIOC, &g);
    
    MX_USART1_Init(); 
    MX_UART7_Init(); 
    MX_USART3_UART_Init();
    MX_CRYP_Init();

    sem_u7 = rt_sem_create("s7", 0, RT_IPC_FLAG_FIFO);
    sem_u1 = rt_sem_create("s1", 0, RT_IPC_FLAG_FIFO);

    rt_thread_t t7 = rt_thread_create("t7", thread_u7_entry, RT_NULL, 2048, 15, 5);
    if(t7) rt_thread_startup(t7);

    rt_thread_t t1 = rt_thread_create("t1", thread_u1_entry, RT_NULL, 2048, 15, 5);
    if(t1) rt_thread_startup(t1);

    HAL_UART_Receive_IT(&huart1, &rx_byte_u1, 1);
    HAL_UART_Receive_IT(&huart7, &rx_byte_u7, 1);

    rt_kprintf("\n=== DEBUG MODE: H7 Crypto Test ===\n");

    while (1) { 
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_2|GPIO_PIN_3);
        rt_thread_mdelay(1000); 
    }
}

void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }
void UART7_IRQHandler(void)  { HAL_UART_IRQHandler(&huart7); }
