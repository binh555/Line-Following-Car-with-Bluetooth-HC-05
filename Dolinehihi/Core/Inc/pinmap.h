#ifndef PINMAP_H
#define PINMAP_H

#include "config.h"    // cần SENSOR_COUNT
#include "main.h"
#include "stm32f4xx_hal.h"

/* MOTOR PINS */
#define STBY_PORT GPIOA
#define STBY_PIN  GPIO_PIN_6

#define AIN1_PORT GPIOB
#define AIN1_PIN  GPIO_PIN_1
#define AIN2_PORT GPIOA
#define AIN2_PIN  GPIO_PIN_8
#define BIN1_PORT GPIOC
#define BIN1_PIN  GPIO_PIN_7
#define BIN2_PORT GPIOB
#define BIN2_PIN  GPIO_PIN_6

/* PWM macros */
#define PWMA_TIM (&htim2)
#define PWMA_CHANNEL TIM_CHANNEL_3
#define PWMB_TIM (&htim3)
#define PWMB_CHANNEL TIM_CHANNEL_2

/* ENCODER PINS */
#define ENC_L_C1_PORT GPIOA
#define ENC_L_C1_PIN  GPIO_PIN_15
#define ENC_R_C1_PORT GPIOB
#define ENC_R_C1_PIN  GPIO_PIN_5

/* SENSOR PINS STRUCT */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} SensorPin_t;

/* EXTERN SENSOR PINS ARRAY */
extern const SensorPin_t sensorPins[SENSOR_COUNT];

#endif
