/* stm32h7xx_hal_conf.h - ??? */
#ifndef __STM32H7xx_HAL_CONF_H
#define __STM32H7xx_HAL_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* 1. ????? HAL ?? */
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED  
#define HAL_DMA_MODULE_ENABLED   

#if !defined  (HSE_VALUE) 
  #define HSE_VALUE    ((uint32_t)25000000) 
#endif
#if !defined  (HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)64000000) 
#endif
#if !defined  (CSI_VALUE)
  #define CSI_VALUE    ((uint32_t)4000000) 
#endif
#define LSI_VALUE  ((uint32_t)32000)      
#define LSE_VALUE  ((uint32_t)32768)     

/* 3. ???????????? (?? HSE_STARTUP_TIMEOUT ??) */
#define HSE_STARTUP_TIMEOUT    ((uint32_t)100)
#define LSE_STARTUP_TIMEOUT    ((uint32_t)5000)
#define EXTERNAL_CLOCK_VALUE   ((uint32_t)12288000) /* I2S??????? */

/* 4. ??????Tick ??? (?? TICK_INT_PRIORITY ??) */
/* 0x0F ?????? (15),??? SysTick */
#define  TICK_INT_PRIORITY            ((uint32_t)0x0F) 

/* 5. ???? */
#define  USE_RTOS                     0
#define  PREFETCH_ENABLE              1
#define  INSTRUCTION_CACHE_ENABLE     1
#define  DATA_CACHE_ENABLE            1

/* 6. ????????? (?? assert_param ??) */
/* ?????,???????????????,??????? */
#define assert_param(expr) ((void)0U)

/* 7. ????????? */
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
#ifdef __cplusplus
}
#endif
#endif /* __STM32H7xx_HAL_CONF_H */

