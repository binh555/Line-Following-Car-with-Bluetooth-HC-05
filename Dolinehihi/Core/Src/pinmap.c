#include "pinmap.h"
#include "stm32f4xx_hal.h"

const SensorPin_t sensorPins[SENSOR_COUNT] = {
    {GPIOA, GPIO_PIN_0},
    {GPIOA, GPIO_PIN_1},
    {GPIOA, GPIO_PIN_4},
    {GPIOB, GPIO_PIN_0},
    {GPIOC, GPIO_PIN_1}
};
