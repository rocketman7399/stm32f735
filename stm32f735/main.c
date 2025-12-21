/* main.c - STM32H735G-DK 终极完整版 */
/* 方案：UART4 (D14/D15) + RTOS + AES-128 */
/* 运行环境：DTCM (0x20000000) */

#include "stm32h7xx_hal.h"
#include "rtthread.h"
#include "board.h"
#include <string.h>
#include <stdio.h>

/* =========================================================== */
/* 0. 全局变量与对象 */
/* =========================================================== */
UART_HandleTypeDef huart3; /* Console (USB) */
UART_HandleTypeDef huart1; /* D0/D1 */
UART_HandleTypeDef huart4; /* [核心] D14/D15 (通用串口) */
UART_HandleTypeDef huart2; /* STMod+ */

/* 接收缓存 (单字节中断用) */
uint8_t rx_buf_u1;
uint8_t rx_buf_u4;
uint8_t rx_buf_u2;

/* 消息队列句柄 */
rt_mq_t uart_rx_mq = RT_NULL;

/* 消息包结构体 */
struct RxMsg {
    uint8_t source_id; /* 1=D0/D1, 2=STMod+, 4=D14/D15 */
    uint8_t data;      /* 数据内容 */
};

/* --- AES 配置 --- */
#define AES_BLOCK_SIZE 16
#define AES_MAX_BUF_SIZE 128 
#define Nb 4
#define Nk 4
#define Nr 10

/* =========================================================== */
/* 1. AES-128 完整算法实现 */
/* =========================================================== */

/* S-Box (替换表) */
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

/* Inverse S-Box (解密替换表) */
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

/* AES 数学核心函数 */
void KeyExpansion(uint8_t* RoundKey, const uint8_t* Key) {
  unsigned i, j, k;
  uint8_t tempa[4]; 
  for (i = 0; i < Nk; ++i) {
    RoundKey[(i * 4) + 0] = Key[(i * 4) + 0]; RoundKey[(i * 4) + 1] = Key[(i * 4) + 1];
    RoundKey[(i * 4) + 2] = Key[(i * 4) + 2]; RoundKey[(i * 4) + 3] = Key[(i * 4) + 3];
  }
  for (i = Nk; i < Nb * (Nr + 1); ++i) {
    {
      k = (i - 1) * 4;
      tempa[0]=RoundKey[k + 0]; tempa[1]=RoundKey[k + 1];
      tempa[2]=RoundKey[k + 2]; tempa[3]=RoundKey[k + 3];
    }
    if (i % Nk == 0) {
      {
        const uint8_t u8tmp = tempa[0];
        tempa[0] = tempa[1]; tempa[1] = tempa[2]; tempa[2] = tempa[3]; tempa[3] = u8tmp;
      }
      {
        tempa[0] = sbox[tempa[0]]; tempa[1] = sbox[tempa[1]];
        tempa[2] = sbox[tempa[2]]; tempa[3] = sbox[tempa[3]];
      }
      tempa[0] = tempa[0] ^ Rcon[i/Nk];
    }
    j = i * 4; k=(i - Nk) * 4;
    RoundKey[j + 0] = RoundKey[k + 0] ^ tempa[0]; RoundKey[j + 1] = RoundKey[k + 1] ^ tempa[1];
    RoundKey[j + 2] = RoundKey[k + 2] ^ tempa[2]; RoundKey[j + 3] = RoundKey[k + 3] ^ tempa[3];
  }
}

void AddRoundKey(uint8_t round, uint8_t* state, const uint8_t* RoundKey) {
  for (uint8_t i = 0; i < 4; ++i) {
    for (uint8_t j = 0; j < 4; ++j) {
      state[i + (j * 4)] ^= RoundKey[(round * Nb * 4) + (i * Nb) + j];
    }
  }
}

void SubBytes(uint8_t* state) {
  for (uint8_t i = 0; i < 16; ++i) state[i] = sbox[state[i]];
}

void ShiftRows(uint8_t* state) {
  uint8_t temp;
  temp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = temp;
  temp = state[2]; state[2] = state[10]; state[10] = temp;
  temp = state[6]; state[6] = state[14]; state[14] = temp;
  temp = state[3]; state[3] = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = temp;
}

uint8_t xtime(uint8_t x) { return ((x<<1) ^ (((x>>7) & 1) * 0x1b)); }

void MixColumns(uint8_t* state) {
  uint8_t i, Tmp, Tm, t;
  for (i = 0; i < 4; ++i) {  
    t   = state[i * 4];
    Tmp = state[i * 4] ^ state[i * 4 + 1] ^ state[i * 4 + 2] ^ state[i * 4 + 3];
    Tm  = state[i * 4] ^ state[i * 4 + 1]; Tm = xtime(Tm); state[i * 4] ^= Tm ^ Tmp;
    Tm  = state[i * 4 + 1] ^ state[i * 4 + 2]; Tm = xtime(Tm); state[i * 4 + 1] ^= Tm ^ Tmp;
    Tm  = state[i * 4 + 2] ^ state[i * 4 + 3]; Tm = xtime(Tm); state[i * 4 + 2] ^= Tm ^ Tmp;
    Tm  = state[i * 4 + 3] ^ t; Tm = xtime(Tm); state[i * 4 + 3] ^= Tm ^ Tmp;
  }
}

void Cipher(uint8_t* state, const uint8_t* RoundKey) {
  uint8_t round = 0;
  AddRoundKey(0, state, RoundKey); 
  for (round = 1; round < Nr; ++round) {
    SubBytes(state); ShiftRows(state); MixColumns(state); AddRoundKey(round, state, RoundKey);
  }
  SubBytes(state); ShiftRows(state); AddRoundKey(Nr, state, RoundKey);
}

void InvShiftRows(uint8_t* state) {
  uint8_t temp;
  temp = state[13]; state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = temp;
  temp = state[2]; state[2] = state[10]; state[10] = temp;
  temp = state[6]; state[6] = state[14]; state[14] = temp;
  temp = state[3]; state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = temp;
}

void InvSubBytes(uint8_t* state) {
  for (uint8_t i = 0; i < 16; ++i) state[i] = rsbox[state[i]];
}

uint8_t Multiply(uint8_t x, uint8_t y) {
  return (((y & 1) * x) ^
       ((y>>1 & 1) * xtime(x)) ^
       ((y>>2 & 1) * xtime(xtime(x))) ^
       ((y>>3 & 1) * xtime(xtime(xtime(x)))) ^
       ((y>>4 & 1) * xtime(xtime(xtime(xtime(x)))))); 
}

void InvMixColumns(uint8_t* state) {
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

void InvCipher(uint8_t* state, const uint8_t* RoundKey) {
  uint8_t round = 0;
  AddRoundKey(Nr, state, RoundKey); 
  for (round = (Nr - 1); round > 0; --round) {
    InvShiftRows(state); InvSubBytes(state); AddRoundKey(round, state, RoundKey); InvMixColumns(state);
  }
  InvShiftRows(state); InvSubBytes(state); AddRoundKey(0, state, RoundKey);
}

void AES128_ECB_encrypt(uint8_t* input, const uint8_t* key, uint8_t *output){
  uint8_t RoundKey[176];
  KeyExpansion(RoundKey, key);
  memcpy(output, input, 16);
  Cipher(output, RoundKey);
}

void AES128_ECB_decrypt(uint8_t* input, const uint8_t* key, uint8_t *output){
  uint8_t RoundKey[176];
  KeyExpansion(RoundKey, key);
  memcpy(output, input, 16);
  InvCipher(output, RoundKey);
}

/* =========================================================== */
/* 2. AES 字符串安全封装 (PKCS#7) */
/* =========================================================== */
void AES128_ECB_encrypt_string(char* input_str, const uint8_t* key, uint8_t* output_buf, uint32_t* out_len)
{
    uint32_t input_len = strlen(input_str);
    uint32_t padded_len = (input_len / AES_BLOCK_SIZE + 1) * AES_BLOCK_SIZE;
    uint8_t temp_buf[AES_MAX_BUF_SIZE] = {0};
    
    if (padded_len > AES_MAX_BUF_SIZE) {
        rt_kprintf("[AES Error] Input string too long!\n");
        return;
    }
    memcpy(temp_buf, input_str, input_len);
    uint8_t pad_value = padded_len - input_len;
    for (uint32_t i = input_len; i < padded_len; i++) temp_buf[i] = pad_value;

    for (uint32_t i = 0; i < padded_len; i += AES_BLOCK_SIZE) {
        AES128_ECB_encrypt(&temp_buf[i], key, &output_buf[i]);
    }
    *out_len = padded_len;
}

void AES128_ECB_decrypt_string(uint8_t* input_buf, uint32_t input_len, const uint8_t* key, char* output_str)
{
    uint8_t temp_buf[AES_MAX_BUF_SIZE] = {0};
    if (input_len % AES_BLOCK_SIZE != 0) {
        rt_kprintf("[AES Error] Invalid cipher length!\n");
        return;
    }
    for (uint32_t i = 0; i < input_len; i += AES_BLOCK_SIZE) {
        AES128_ECB_decrypt(&input_buf[i], key, &temp_buf[i]);
    }
    uint8_t pad_value = temp_buf[input_len - 1];
    if (pad_value > AES_BLOCK_SIZE || pad_value == 0) {
         rt_kprintf("[AES Error] Padding check failed: %02X\n", pad_value);
         return; 
    }
    uint32_t real_len = input_len - pad_value;
    memcpy(output_str, temp_buf, real_len);
    output_str[real_len] = '\0'; 
}

/* =========================================================== */
/* 3. 硬件初始化 */
/* =========================================================== */
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
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) while(1);

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
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) while(1);
}

void UART_Init_Common(UART_HandleTypeDef *huart, USART_TypeDef *instance)
{
    huart->Instance = instance;
    huart->Init.BaudRate = 115200;
    huart->Init.WordLength = UART_WORDLENGTH_8B;
    huart->Init.StopBits = UART_STOPBITS_1;
    huart->Init.Parity = UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(huart);
}

/* 1. Console (USART3) */
int MX_USART3_UART_Init(void) {
    __HAL_RCC_USART3_CLK_ENABLE(); __HAL_RCC_GPIOD_CLK_ENABLE(); 
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    UART_Init_Common(&huart3, USART3);
    return 0;
}
INIT_BOARD_EXPORT(MX_USART3_UART_Init);

/* 2. D0/D1 (USART1) */
void MX_USART1_Init(void) {
    __HAL_RCC_USART1_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    UART_Init_Common(&huart1, USART1);
}

/* 3. [关键] D14/D15 (UART4) */
void MX_UART4_Init(void) {
    __HAL_RCC_UART4_CLK_ENABLE(); 
    __HAL_RCC_GPIOB_CLK_ENABLE(); 
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* [硬件接线] D14(TX) -> PB9, D15(RX) -> PB8 */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART4; /* 复用为 UART4 */
    
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    UART_Init_Common(&huart4, UART4);
}

/* 4. STMod+ (USART2) */
void MX_USART2_UART_Init(void) {
    __HAL_RCC_USART2_CLK_ENABLE(); __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    UART_Init_Common(&huart2, USART2);
}

/* =========================================================== */
/* 4. 中断逻辑 (Producer) */
/* =========================================================== */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    struct RxMsg msg;

    if(huart->Instance == USART1) { // D0/D1
        msg.source_id = 1; msg.data = rx_buf_u1;
        HAL_UART_Receive_IT(&huart1, &rx_buf_u1, 1);
    }
    else if(huart->Instance == UART4) { // D14/D15
        msg.source_id = 4; msg.data = rx_buf_u4;
        HAL_UART_Receive_IT(&huart4, &rx_buf_u4, 1);
    }
    else if(huart->Instance == USART2) { // STMod+
        msg.source_id = 2; msg.data = rx_buf_u2;
        HAL_UART_Receive_IT(&huart2, &rx_buf_u2, 1);
    }
    else { return; }

    if (uart_rx_mq != RT_NULL) {
        rt_mq_send(uart_rx_mq, &msg, sizeof(struct RxMsg));
    }
}

/* =========================================================== */
/* 5. 线程逻辑 (Consumer) */
/* =========================================================== */
void uart_process_thread_entry(void *parameter)
{
    struct RxMsg msg;
    char line_buf[64];
    uint8_t line_idx = 0;

    while (1)
    {
        if (rt_mq_recv(uart_rx_mq, &msg, sizeof(struct RxMsg), RT_WAITING_FOREVER) == RT_EOK)
        {
            /* D14/D15 数据处理 */
            if (msg.source_id == 4) {
                rt_kprintf("[UART4/D14D15]: %02X (%c)\n", msg.data, msg.data);
            }
            /* STMod+ 数据 */
            else if (msg.source_id == 2) {
                if (msg.data == '\n' || msg.data == '\r') {
                    line_buf[line_idx] = '\0';
                    if (line_idx > 0) rt_kprintf("[STMod+]: %s\n", line_buf);
                    line_idx = 0;
                } else {
                    if (line_idx < 63) line_buf[line_idx++] = msg.data;
                }
            }
            /* D0/D1 数据 */
            else if (msg.source_id == 1) {
                rt_kprintf("[D0/D1]: %02X\n", msg.data);
            }
        }
    }
}

/* =========================================================== */
/* 6. Main 主函数 */
/* =========================================================== */
int main(void)
{
    HAL_Init(); 
    
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* 串口初始化 */
    MX_USART1_Init(); 
    MX_UART4_Init();  /* D14/D15 (重点) */
    MX_USART2_UART_Init(); 

    uart_rx_mq = rt_mq_create("u_mq", sizeof(struct RxMsg), 256, RT_IPC_FLAG_FIFO);

    rt_thread_t tid = rt_thread_create("u_task", uart_process_thread_entry, RT_NULL, 2048, 20, 10);
    if (tid != RT_NULL) rt_thread_startup(tid);

    HAL_UART_Receive_IT(&huart1, &rx_buf_u1, 1);
    HAL_UART_Receive_IT(&huart4, &rx_buf_u4, 1); /* 开启 D14/D15 接收 */
    HAL_UART_Receive_IT(&huart2, &rx_buf_u2, 1);

    rt_kprintf("\n=== STM32H7 System Ready (UART4 Mode) ===\n");
    rt_kprintf("1. D14/D15 (UART4) - User Port\n");
    rt_kprintf("2. D0/D1 (USART1)\n");
    rt_kprintf("3. STMod+ (USART2)\n");

    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_2 | GPIO_PIN_3);
        rt_thread_mdelay(1000); 
    }
}

/* =========================================================== */
/* 7. 测试命令 (MSH) */
/* =========================================================== */
void aes_test_cmd(void) {
    const uint8_t key[16] = {
        0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
        0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
    };
    char plain_text[] = "AES on STM32H7!"; 
    uint8_t encrypted_buf[64] = {0};
    char decrypted_text[64] = {0};
    uint32_t cipher_len = 0;

    rt_kprintf("\n--- AES Test ---\n");
    AES128_ECB_encrypt_string(plain_text, key, encrypted_buf, &cipher_len);
    rt_kprintf("Encrypted: ");
    for(int i=0; i<cipher_len; i++) rt_kprintf("%02X ", encrypted_buf[i]);
    rt_kprintf("\n");
    AES128_ECB_decrypt_string(encrypted_buf, cipher_len, key, decrypted_text);
    rt_kprintf("Decrypted: %s\n", decrypted_text);
}
MSH_CMD_EXPORT(aes_test_cmd, AES Test);

void uart_test(void)
{
    char msg[] = "Ping!\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
    HAL_UART_Transmit(&huart4, (uint8_t*)msg, strlen(msg), 100);
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
    rt_kprintf("Ping sent to D14/D15, D0/D1, STMod+.\n");
}
MSH_CMD_EXPORT(uart_test, Ping all uarts);
