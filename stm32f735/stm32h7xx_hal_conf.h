/* stm32h7xx_hal_conf.h - STM32H7系列HAL库配置文件 */
#ifndef __STM32H7xx_HAL_CONF_H
#define __STM32H7xx_HAL_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* ============================================================================ */
/* 1. 启用的HAL模块配置 */
/* ============================================================================ */
#define HAL_MODULE_ENABLED          /* 启用HAL库总开关 */
#define HAL_CRYP_MODULE_ENABLED     /* 启用CRYP(加密)模块 */
#define HAL_GPIO_MODULE_ENABLED     /* 启用GPIO(通用输入输出)模块 */
#define HAL_RCC_MODULE_ENABLED      /* 启用RCC(复位和时钟控制)模块 */
#define HAL_PWR_MODULE_ENABLED      /* 启用PWR(电源控制)模块 */
#define HAL_CORTEX_MODULE_ENABLED   /* 启用Cortex-M内核相关模块 */
#define HAL_FLASH_MODULE_ENABLED    /* 启用FLASH(闪存)控制模块 */
#define HAL_UART_MODULE_ENABLED     /* 启用UART(串口通信)模块 */
#define HAL_DMA_MODULE_ENABLED      /* 启用DMA(直接内存访问)模块 */

/* ============================================================================ */
/* 2. 回调函数注册功能配置 */
/* ============================================================================ */
/* 必须显式定义 CRYP 模块的回调注册开关 */
#define USE_HAL_CRYP_REGISTER_CALLBACKS   1U  /* <--- 【关键】必须加这行，单独开启CRYP的回调 */
#define USE_HAL_DRIVER_REGISTER_CALLBACKS 1U  /* 全局开关(保留着也没事) */

/* ============================================================================ */
/* 3. 时钟源频率配置 */
/* ============================================================================ */
#if !defined (HSE_VALUE) 
  #define HSE_VALUE    ((uint32_t)25000000)
#endif

#if !defined (HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)64000000)
#endif

#if !defined (CSI_VALUE)
  #define CSI_VALUE    ((uint32_t)4000000)
#endif

#define LSI_VALUE  ((uint32_t)32000)
#define LSE_VALUE  ((uint32_t)32768)

/* ============================================================================ */
/* 4. 时钟源启动超时时间配置 */
/* ============================================================================ */
#define HSE_STARTUP_TIMEOUT    ((uint32_t)100)
#define LSE_STARTUP_TIMEOUT    ((uint32_t)5000)
#define EXTERNAL_CLOCK_VALUE   ((uint32_t)12288000)

/* ============================================================================ */
/* 5. 其他系统配置 */
/* ============================================================================ */
#define TICK_INT_PRIORITY            ((uint32_t)0x0F)
#define USE_RTOS                     0
#define PREFETCH_ENABLE              1
#define INSTRUCTION_CACHE_ENABLE     1
#define DATA_CACHE_ENABLE            1
#define assert_param(expr)           ((void)0U)

/* ============================================================================ */
/* 6. 包含头文件 */
/* ============================================================================ */
#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_pwr.h"
#include "stm32h7xx_hal_cortex.h"
#include "stm32h7xx_hal_flash.h"

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32h7xx_hal_dma.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32h7xx_hal_uart.h"
#endif

#ifdef HAL_CRYP_MODULE_ENABLED
  #include "stm32h7xx_hal_cryp.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STM32H7xx_HAL_CONF_H */
