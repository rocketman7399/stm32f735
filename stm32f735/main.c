/*
 * 文件名: main.c
 * 硬件平台: STM32H735G-DK
 *
 * [系统交互逻辑]
 * -------------------------------------------------------------------------
 * 1. 上行 (SSCOM -> Console): 加密流程
 * - 用户在 SSCOM (UART1 或 UART7) 发送明文 (Hex 格式, 如 11 22 33)
 * - 板子接收数据，自动补全为 16 字节，执行 AES 加密
 * - 板子在 SecureCRT (控制台) 打印出密文，格式: [UARTx Cipher]: AA BB ...
 *
 * 2. 下行 (Console -> SSCOM): 解密流程
 * - 用户在 SecureCRT 输入命令: send_dec <端口号> <密文数据...>
 * - 例如: send_dec 1 AA BB CC ... (将密文解密后发回给 UART1)
 * - 板子执行 AES 解密，并将还原的明文通过指定串口发回 SSCOM
 * -------------------------------------------------------------------------
 */

#include "stm32h7xx_hal.h"
#include "rtthread.h"
#include "board.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 消除 Keil 编译器关于 FPU 的重复定义警告 */
#ifdef __FPU_PRESENT
#undef __FPU_PRESENT
#endif

/* =================================================================================
 * 1. 全局配置与变量
 * ================================================================================= */

/* --- 硬件句柄 --- */
UART_HandleTypeDef huart3; /* Console (SecureCRT) */
UART_HandleTypeDef huart1; /* 通道 1 (SSCOM #1 - D0/D1) */
UART_HandleTypeDef huart7; /* 通道 7 (SSCOM #2 - D13/D10) */
UART_HandleTypeDef huart2; /* 备用 (STMod+) */

/* --- 接收缓存 (ISR用) --- */
uint8_t rx_buf_u1;
uint8_t rx_buf_u7;
uint8_t rx_buf_u2;

/* --- RTOS 消息队列 --- */
rt_mq_t uart_rx_mq = RT_NULL;

/* 消息包结构 */
struct RxMsg {
    uint8_t source_id; /* 1=UART1, 7=UART7 */
    uint8_t data;
};

/* --- AES 参数 --- */
#define AES_BLOCK_SIZE 16
#define Nb 4
#define Nk 4
#define Nr 10

/* 固定密钥 (128-bit) */
const uint8_t AES_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

/* =================================================================================
 * 2. AES-128 核心算法 (已完全展开排版)
 * ================================================================================= */

static const uint8_t sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16 
};

static const uint8_t rsbox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d 
};

static const uint8_t Rcon[11] = { 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };

/* --- AES 辅助函数 (Expanded for readability) --- */

void KeyExpansion(uint8_t* RoundKey, const uint8_t* Key) 
{
    unsigned i, j, k; 
    uint8_t tempa[4]; 
    
    for (i = 0; i < Nk; ++i) { 
        RoundKey[(i * 4) + 0] = Key[(i * 4) + 0]; 
        RoundKey[(i * 4) + 1] = Key[(i * 4) + 1]; 
        RoundKey[(i * 4) + 2] = Key[(i * 4) + 2]; 
        RoundKey[(i * 4) + 3] = Key[(i * 4) + 3]; 
    }
    
    for (i = Nk; i < Nb * (Nr + 1); ++i) {
        k = (i - 1) * 4; 
        tempa[0] = RoundKey[k + 0]; 
        tempa[1] = RoundKey[k + 1]; 
        tempa[2] = RoundKey[k + 2]; 
        tempa[3] = RoundKey[k + 3]; 
        
        if (i % Nk == 0) {
            const uint8_t u8tmp = tempa[0]; 
            tempa[0] = tempa[1]; 
            tempa[1] = tempa[2]; 
            tempa[2] = tempa[3]; 
            tempa[3] = u8tmp; 
            
            tempa[0] = sbox[tempa[0]]; 
            tempa[1] = sbox[tempa[1]]; 
            tempa[2] = sbox[tempa[2]]; 
            tempa[3] = sbox[tempa[3]]; 
            
            tempa[0] = tempa[0] ^ Rcon[i/Nk];
        }
        
        j = i * 4; 
        k = (i - Nk) * 4;
        RoundKey[j + 0] = RoundKey[k + 0] ^ tempa[0]; 
        RoundKey[j + 1] = RoundKey[k + 1] ^ tempa[1];
        RoundKey[j + 2] = RoundKey[k + 2] ^ tempa[2]; 
        RoundKey[j + 3] = RoundKey[k + 3] ^ tempa[3];
    }
}

void AddRoundKey(uint8_t round, uint8_t* state, const uint8_t* RoundKey) 
{
    for (uint8_t i = 0; i < 4; ++i) {
        for (uint8_t j = 0; j < 4; ++j) {
            state[i + (j * 4)] ^= RoundKey[(round * Nb * 4) + (i * Nb) + j];
        }
    }
}

void SubBytes(uint8_t* state) 
{
    for (uint8_t i = 0; i < 16; ++i) {
        state[i] = sbox[state[i]];
    }
}

void ShiftRows(uint8_t* state) 
{
    uint8_t temp;
    
    /* Row 1 */
    temp = state[1]; 
    state[1] = state[5]; 
    state[5] = state[9]; 
    state[9] = state[13]; 
    state[13] = temp;
    
    /* Row 2 */
    temp = state[2]; 
    state[2] = state[10]; 
    state[10] = temp;
    temp = state[6]; 
    state[6] = state[14]; 
    state[14] = temp;
    
    /* Row 3 */
    temp = state[3]; 
    state[3] = state[15]; 
    state[15] = state[11]; 
    state[11] = state[7]; 
    state[7] = temp;
}

uint8_t xtime(uint8_t x) 
{
    return ((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

void MixColumns(uint8_t* state) 
{
    uint8_t i, Tmp, Tm, t;
    for (i = 0; i < 4; ++i) {
        t = state[i * 4]; 
        Tmp = state[i * 4] ^ state[i * 4 + 1] ^ state[i * 4 + 2] ^ state[i * 4 + 3];
        
        Tm = state[i * 4] ^ state[i * 4 + 1]; 
        Tm = xtime(Tm); 
        state[i * 4] ^= Tm ^ Tmp;
        
        Tm = state[i * 4 + 1] ^ state[i * 4 + 2]; 
        Tm = xtime(Tm); 
        state[i * 4 + 1] ^= Tm ^ Tmp;
        
        Tm = state[i * 4 + 2] ^ state[i * 4 + 3]; 
        Tm = xtime(Tm); 
        state[i * 4 + 2] ^= Tm ^ Tmp;
        
        Tm = state[i * 4 + 3] ^ t; 
        Tm = xtime(Tm); 
        state[i * 4 + 3] ^= Tm ^ Tmp;
    }
}

void Cipher(uint8_t* state, const uint8_t* RoundKey) 
{
    uint8_t round = 0; 
    AddRoundKey(0, state, RoundKey); 
    
    for (round = 1; round < Nr; ++round) { 
        SubBytes(state); 
        ShiftRows(state); 
        MixColumns(state); 
        AddRoundKey(round, state, RoundKey); 
    }
    
    SubBytes(state); 
    ShiftRows(state); 
    AddRoundKey(Nr, state, RoundKey);
}

void InvShiftRows(uint8_t* state) 
{
    uint8_t temp;
    
    /* Row 1 */
    temp = state[13]; state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = temp;
    /* Row 2 */
    temp = state[2]; state[2] = state[10]; state[10] = temp;
    temp = state[6]; state[6] = state[14]; state[14] = temp;
    /* Row 3 */
    temp = state[3]; state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = temp;
}

void InvSubBytes(uint8_t* state) 
{
    for (uint8_t i = 0; i < 16; ++i) {
        state[i] = rsbox[state[i]];
    }
}

uint8_t Multiply(uint8_t x, uint8_t y) 
{
    return (((y & 1) * x) ^ ((y>>1 & 1) * xtime(x)) ^ ((y>>2 & 1) * xtime(xtime(x))) ^ ((y>>3 & 1) * xtime(xtime(xtime(x)))) ^ ((y>>4 & 1) * xtime(xtime(xtime(xtime(x)))))); 
}

void InvMixColumns(uint8_t* state) 
{
    int i; 
    uint8_t a, b, c, d;
    for (i = 0; i < 4; ++i) { 
        a = state[i * 4]; b = state[i * 4 + 1]; c = state[i * 4 + 2]; d = state[i * 4 + 3];
        
        state[i * 4] = Multiply(a, 0x0e) ^ Multiply(b, 0x0b) ^ Multiply(c, 0x0d) ^ Multiply(d, 0x09);
        state[i * 4 + 1] = Multiply(a, 0x09) ^ Multiply(b, 0x0e) ^ Multiply(c, 0x0b) ^ Multiply(d, 0x0d);
        state[i * 4 + 2] = Multiply(a, 0x0d) ^ Multiply(b, 0x09) ^ Multiply(c, 0x0e) ^ Multiply(d, 0x0b);
        state[i * 4 + 3] = Multiply(a, 0x0b) ^ Multiply(b, 0x0d) ^ Multiply(c, 0x09) ^ Multiply(d, 0x0e);
    }
}

void InvCipher(uint8_t* state, const uint8_t* RoundKey) 
{
    uint8_t round = 0; 
    AddRoundKey(Nr, state, RoundKey); 
    
    for (round = (Nr - 1); round > 0; --round) { 
        InvShiftRows(state); 
        InvSubBytes(state); 
        AddRoundKey(round, state, RoundKey); 
        InvMixColumns(state); 
    }
    
    InvShiftRows(state); 
    InvSubBytes(state); 
    AddRoundKey(0, state, RoundKey);
}

/* --- AES 对外接口 --- */

void AES128_ECB_encrypt(uint8_t* input, const uint8_t* key, uint8_t *output) {
    uint8_t RoundKey[176];
    KeyExpansion(RoundKey, key);
    memcpy(output, input, 16);
    Cipher(output, RoundKey);
}

void AES128_ECB_decrypt(uint8_t* input, const uint8_t* key, uint8_t *output) {
    uint8_t RoundKey[176];
    KeyExpansion(RoundKey, key);
    memcpy(output, input, 16);
    InvCipher(output, RoundKey);
}

/* =================================================================================
 * 3. 硬件驱动层 (初始化)
 * ================================================================================= */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY); 
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE; 
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1);
}

void UART_Init_Common(UART_HandleTypeDef *huart, USART_TypeDef *instance) {
    huart->Instance = instance;
    huart->Init.BaudRate = 115200;
    huart->Init.WordLength = UART_WORDLENGTH_8B;
    huart->Init.StopBits = UART_STOPBITS_1;
    huart->Init.Parity = UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(huart);
}

/* 控制台串口 (USART3) */
int MX_USART3_UART_Init(void) {
    __HAL_RCC_USART3_CLK_ENABLE(); 
    __HAL_RCC_GPIOD_CLK_ENABLE(); 
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    UART_Init_Common(&huart3, USART3);
    return 0;
}
INIT_BOARD_EXPORT(MX_USART3_UART_Init);

/* 通道 1 (USART1) */
void MX_USART1_Init(void) {
    __HAL_RCC_USART1_CLK_ENABLE(); 
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    UART_Init_Common(&huart1, USART1);
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* 通道 7 (UART7) */
void MX_UART7_Init(void) {
    __HAL_RCC_UART7_CLK_ENABLE(); 
    __HAL_RCC_GPIOF_CLK_ENABLE(); 
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Alternate = GPIO_AF7_UART7; 
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
    
    UART_Init_Common(&huart7, UART7);
    HAL_NVIC_SetPriority(UART7_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(UART7_IRQn);
}

/* 通道 2 (USART2) */
void MX_USART2_UART_Init(void) {
    __HAL_RCC_USART2_CLK_ENABLE(); 
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    UART_Init_Common(&huart2, USART2);
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* =================================================================================
 * 4. 业务逻辑层 (上行: 接收 -> 加密 -> 打印)
 * ================================================================================= */

/* 中断回调: 数据入队 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    struct RxMsg msg;
    if(huart->Instance == USART1) { msg.source_id = 1; msg.data = rx_buf_u1; HAL_UART_Receive_IT(&huart1, &rx_buf_u1, 1); }
    else if(huart->Instance == UART7) { msg.source_id = 7; msg.data = rx_buf_u7; HAL_UART_Receive_IT(&huart7, &rx_buf_u7, 1); }
    else if(huart->Instance == USART2) { msg.source_id = 2; msg.data = rx_buf_u2; HAL_UART_Receive_IT(&huart2, &rx_buf_u2, 1); }
    else return;
    
    if (uart_rx_mq != RT_NULL) rt_mq_send(uart_rx_mq, &msg, sizeof(struct RxMsg));
}

/* 通用函数: 处理接收包并加密打印 */
void Process_And_Print_Cipher(uint8_t* buf, uint8_t len, char* port_name)
{
    uint8_t aes_in[16];
    uint8_t aes_out[16];
    
    if (len > 0) {
        /* 1. 打印收到的明文 */
        rt_kprintf("\n[%s Plain]: ", port_name);
        for(int i=0; i<len; i++) rt_kprintf("%02X ", buf[i]);
        
        /* 2. 补全并加密 */
        memset(aes_in, 0, 16);
        memcpy(aes_in, buf, len);
        AES128_ECB_encrypt(aes_in, AES_KEY, aes_out);
        
        /* 3. 打印密文 (供用户复制) */
        rt_kprintf("\n[%s Cipher]: ", port_name);
        for(int i=0; i<16; i++) rt_kprintf("%02X ", aes_out[i]);
        rt_kprintf("\n");
    }
}

/* 主线程 */
void uart_process_thread_entry(void *parameter)
{
    struct RxMsg msg;
    uint8_t buf_7[128], len_7 = 0;
    uint8_t buf_1[128], len_1 = 0;

    while (1) {
        if (rt_mq_recv(uart_rx_mq, &msg, sizeof(struct RxMsg), RT_WAITING_FOREVER) == RT_EOK) 
        {
            /* 处理 UART7 (SSCOM #2) */
            if (msg.source_id == 7) {
                buf_7[len_7++] = msg.data;
                while (rt_mq_recv(uart_rx_mq, &msg, sizeof(struct RxMsg), 10) == RT_EOK) {
                    if (msg.source_id == 7 && len_7 < 16) buf_7[len_7++] = msg.data;
                }
                Process_And_Print_Cipher(buf_7, len_7, "UART7");
                len_7 = 0;
            }
            /* 处理 UART1 (SSCOM #1) */
            else if (msg.source_id == 1) {
                buf_1[len_1++] = msg.data;
                while (rt_mq_recv(uart_rx_mq, &msg, sizeof(struct RxMsg), 10) == RT_EOK) {
                    if (msg.source_id == 1 && len_1 < 16) buf_1[len_1++] = msg.data;
                }
                Process_And_Print_Cipher(buf_1, len_1, "UART1");
                len_1 = 0;
            }
        }
    }
}

/* =================================================================================
 * 5. 业务逻辑层 (下行: 命令行解密 -> 回传)
 * ================================================================================= */

/* 命令: send_dec <port> <cipher_hex...> */
void send_dec(int argc, char** argv)
{
    if (argc < 3) {
        rt_kprintf("Usage: send_dec <port: 1/7> <hex1> <hex2> ...\n");
        return;
    }

    int port = atoi(argv[1]);
    UART_HandleTypeDef *huart_target = (port == 7) ? &huart7 : &huart1;

    uint8_t cipher_in[16] = {0};
    int data_len = argc - 2;
    if (data_len > 16) data_len = 16;

    for (int i = 0; i < data_len; i++) {
        cipher_in[i] = (uint8_t)strtol(argv[2 + i], NULL, 16);
    }

    /* 执行解密 */
    uint8_t plain_out[16] = {0};
    AES128_ECB_decrypt(cipher_in, AES_KEY, plain_out);

    /* 打印并回传 */
    rt_kprintf("[Manual Decrypt]: Sending Plaintext to UART%d...\n", port);
    HAL_UART_Transmit(huart_target, plain_out, 16, 100);
}
MSH_CMD_EXPORT(send_dec, Decrypt and send via UART);

/* =================================================================================
 * 6. 主函数
 * ================================================================================= */
int main(void)
{
    HAL_Init(); 
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    MX_USART1_Init(); 
    MX_UART7_Init(); 
    MX_USART2_UART_Init(); 

    uart_rx_mq = rt_mq_create("u_mq", sizeof(struct RxMsg), 256, RT_IPC_FLAG_FIFO);
    rt_thread_t tid = rt_thread_create("u_task", uart_process_thread_entry, RT_NULL, 2048, 20, 10);
    if (tid != RT_NULL) rt_thread_startup(tid);

    HAL_UART_Receive_IT(&huart1, &rx_buf_u1, 1);
    HAL_UART_Receive_IT(&huart7, &rx_buf_u7, 1); 
    HAL_UART_Receive_IT(&huart2, &rx_buf_u2, 1);

    rt_kprintf("\n=== STM32H7 Crypto Console Ready ===\n");
    rt_kprintf("1. [Encrypt]: Send HEX to UART1/7 -> Console shows Cipher\n");
    rt_kprintf("2. [Decrypt]: CMD 'send_dec <port> <cipher...>' -> UART shows Plain\n");

    while (1) { 
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_2 | GPIO_PIN_3); 
        rt_thread_mdelay(1000); 
    }
}

/* =================================================================================
 * 7. 中断服务函数
 * ================================================================================= */
void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }
void UART7_IRQHandler(void)  { HAL_UART_IRQHandler(&huart7); }
void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }

