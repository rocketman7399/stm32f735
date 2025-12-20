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
#define HAL_GPIO_MODULE_ENABLED     /* 启用GPIO(通用输入输出)模块 */
#define HAL_RCC_MODULE_ENABLED      /* 启用RCC(复位和时钟控制)模块 */
#define HAL_PWR_MODULE_ENABLED      /* 启用PWR(电源控制)模块 */
#define HAL_CORTEX_MODULE_ENABLED   /* 启用Cortex-M内核相关模块 */
#define HAL_FLASH_MODULE_ENABLED    /* 启用FLASH(闪存)控制模块 */
#define HAL_UART_MODULE_ENABLED     /* 启用UART(串口通信)模块 */
#define HAL_DMA_MODULE_ENABLED      /* 启用DMA(直接内存访问)模块 */

/* ============================================================================ */
/* 2. 时钟源频率配置 */
/* ============================================================================ */
/* HSE - 高速外部时钟(外部晶振) */
#if !defined (HSE_VALUE) 
  #define HSE_VALUE    ((uint32_t)25000000)  /* 25MHz外部晶振频率 */
#endif

/* HSI - 高速内部时钟(芯片内部RC振荡器) */
#if !defined (HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)64000000)  /* 64MHz内部RC振荡器频率 */
#endif

/* CSI - 低功耗内部时钟源 */
#if !defined (CSI_VALUE)
  #define CSI_VALUE    ((uint32_t)4000000)   /* 4MHz低功耗时钟频率 */
#endif

/* LSI - 低速内部时钟(用于看门狗等) */
#define LSI_VALUE  ((uint32_t)32000)         /* 32KHz低速内部时钟 */

/* LSE - 低速外部时钟(通常用于RTC实时时钟) */
#define LSE_VALUE  ((uint32_t)32768)         /* 32.768KHz外部晶振(RTC专用) */

/* ============================================================================ */
/* 3. 时钟源启动超时时间配置 */
/* ============================================================================ */
#define HSE_STARTUP_TIMEOUT    ((uint32_t)100)   /* HSE启动超时时间: 100ms */
#define LSE_STARTUP_TIMEOUT    ((uint32_t)5000)  /* LSE启动超时时间: 5000ms */

/* 外部时钟值(用于I2S等音频接口) */
#define EXTERNAL_CLOCK_VALUE   ((uint32_t)12288000)  /* I2S外部时钟: 12.288MHz */

/* ============================================================================ */
/* 4. 系统滴答定时器(SysTick)中断优先级配置 */
/* ============================================================================ */
/* 优先级范围: 0x00(最高优先级) 到 0x0F(最低优先级) */
/* 0x0F 表示最低优先级(15),用于系统滴答定时器 SysTick */
#define TICK_INT_PRIORITY     ((uint32_t)0x0F)

/* ============================================================================ */
/* 5. 系统特性配置 */
/* ============================================================================ */
#define USE_RTOS                     0    /* 是否使用实时操作系统: 0=裸机, 1=使用RTOS */
#define PREFETCH_ENABLE              1    /* 启用预取缓冲: 1=启用, 0=禁用 */
#define INSTRUCTION_CACHE_ENABLE     1    /* 启用指令缓存: 1=启用, 0=禁用 */
#define DATA_CACHE_ENABLE            1    /* 启用数据缓存: 1=启用, 0=禁用 */

/* ============================================================================ */
/* 6. 参数检查宏定义(调试用) */
/* ============================================================================ */
/* 在发布版本中,为了提高运行速度,通常禁用参数检查 */
/* 在调试版本中,可以启用参数检查来发现错误 */
/* 当前配置: 禁用参数检查(不执行任何检查) */
#define assert_param(expr) ((void)0U)

/* 调试版本可以使用以下宏来启用参数检查:
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t* file, uint32_t line);
*/

/* ============================================================================ */
/* 7. 包含启用的HAL模块头文件 */
/* ============================================================================ */
/* 以下是必需的核心头文件(总是包含) */
#include "stm32h7xx_hal_rcc.h"      /* RCC(复位和时钟控制)模块 */
#include "stm32h7xx_hal_gpio.h"     /* GPIO(通用输入输出)模块 */
#include "stm32h7xx_hal_pwr.h"      /* PWR(电源管理)模块 */
#include "stm32h7xx_hal_cortex.h"   /* Cortex-M内核控制模块 */
#include "stm32h7xx_hal_flash.h"    /* FLASH(闪存控制)模块 */

/* 以下是可选的外设头文件(根据宏定义条件包含) */
#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32h7xx_hal_dma.h"    /* DMA(直接内存访问)模块 */
#endif

#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32h7xx_hal_uart.h"   /* UART(串口通信)模块 */
#endif

/* 可以根据需要添加更多外设模块头文件,例如:
#ifdef HAL_SPI_MODULE_ENABLED
  #include "stm32h7xx_hal_spi.h"    // SPI(串行外设接口)模块
#endif

#ifdef HAL_I2C_MODULE_ENABLED
  #include "stm32h7xx_hal_i2c.h"    // I2C(I2C总线)模块
#endif

#ifdef HAL_TIM_MODULE_ENABLED
  #include "stm32h7xx_hal_tim.h"    // TIM(定时器)模块
#endif

#ifdef HAL_ADC_MODULE_ENABLED
  #include "stm32h7xx_hal_adc.h"    // ADC(模数转换器)模块
#endif
*/

#ifdef __cplusplus
}
#endif

#endif /* __STM32H7xx_HAL_CONF_H */

/* ============================================================================ */
/* 文件说明:
 * 
 * 这个配置文件用于:
 * 1. 选择要使用的HAL库外设模块(通过定义相应的宏)
 * 2. 配置系统时钟源的频率
 * 3. 配置系统参数(如中断优先级、缓存开关等)
 * 
 * 使用方法:
 * - 如果需要使用某个外设,取消注释对应的 #define HAL_XXX_MODULE_ENABLED
 * - 如果不需要某个外设,注释掉对应的 #define 或删除该行
 * - 根据实际硬件调整时钟频率值(HSE_VALUE等)
 * 
 * 注意事项:
 * - 启用的模块越多,编译后的代码体积越大
 * - 只启用实际需要的模块可以节省存储空间
 * - 修改时钟配置前请确认硬件电路上的实际晶振频率
 * ============================================================================ */