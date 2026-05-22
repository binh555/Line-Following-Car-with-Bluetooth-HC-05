#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* PWM handles */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

/* UART handles */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* Pin defines */
#define USART_TX_Pin GPIO_PIN_2
#define USART_RX_Pin GPIO_PIN_3

/* Function prototypes */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_TIM2_Init(void);
void MX_TIM3_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void Error_Handler(void);

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

#ifdef __cplusplus
}
#endif

#endif
