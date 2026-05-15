/* USER CODE BEGIN Header */
/**
  ****************************************************************************
  * @file : main.c
  * @brief : Line Follower Tự Động - Tốc Độ Cao (Không Bluetooth)
  *
  * TỐC ĐỘ TỐI ĐA:
  * - BASE_SPEED = 92%
  * - MAX_SPEED = 100%
  * - Sharp turn: 25% + 92%
  *
  * Boot: Delay 3 giây → Tự động chạy
  * Đã tắt tính năng STOP khi gặp đen để tránh dừng đột ngột
  ****************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);
void Error_Handler(void);

/* ========================== USER CONFIG ========================== */
#define SENSOR_COUNT 5
#define SENSOR_ACTIVE_BLACK 0

#define PWM_MAX 999
#define PWM_MIN_RUN 180
#define LEFT_PWM_BIAS 20
#define RIGHT_PWM_BIAS 22
#define LEFT_MOTOR_INVERT 0
#define RIGHT_MOTOR_INVERT 1

#define CONTROL_INTERVAL_MS 5U
#define DEBUG_INTERVAL_MS 500U
#define START_DELAY_MS 3000U

#define LINE_CENTER_POS 2000.0f

#define BASE_SPEED 92.0f
#define MAX_SPEED 100.0f
#define SEARCH_FORWARD_SPEED 88.0f
#define SEARCH_REVERSE_SPEED 30.0f

#define KP 18.0f
#define KD 8.0f
#define MAX_CORRECTION 42.0f

#define STOP_ON_ALL_BLACK 0          // TẮT để tránh dừng đột ngột

/* ========================== PIN MAP ========================== */
#define STBY_PORT GPIOA
#define STBY_PIN GPIO_PIN_6

#define AIN1_PORT GPIOB
#define AIN1_PIN GPIO_PIN_1

#define AIN2_PORT GPIOA
#define AIN2_PIN GPIO_PIN_8

#define BIN1_PORT GPIOC
#define BIN1_PIN GPIO_PIN_7

#define BIN2_PORT GPIOB
#define BIN2_PIN GPIO_PIN_6

#define PWMA_TIM (&htim2)
#define PWMA_CHANNEL TIM_CHANNEL_3

#define PWMB_TIM (&htim3)
#define PWMB_CHANNEL TIM_CHANNEL_2

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} SensorPin_t;

static const SensorPin_t sensorPins[SENSOR_COUNT] = {
    {GPIOA, GPIO_PIN_0}, {GPIOA, GPIO_PIN_1}, {GPIOA, GPIO_PIN_4},
    {GPIOB, GPIO_PIN_0}, {GPIOC, GPIO_PIN_1}
};

/* ========================== GLOBAL STATE ========================== */
static uint8_t sensorState[SENSOR_COUNT] = {0};
static float currentPosition = LINE_CENTER_POS;
static float filteredPosition = LINE_CENTER_POS;
static float lastError = 0.0f;
static int8_t lastSeenSide = 0;
static uint8_t runEnabled = 1;
static uint8_t finishLatched = 0;
static uint32_t allBlackCount = 0;
static int16_t lastLeftPwm = 0;
static int16_t lastRightPwm = 0;

typedef enum {
    CAR_MODE_STOP = 0,
    CAR_MODE_AUTO
} CarMode_t;

static CarMode_t carMode = CAR_MODE_AUTO;

/* ========================== HELPERS ========================== */
static float clamp_f32(float x, float mn, float mx) {
    if (x < mn) return mn;
    if (x > mx) return mx;
    return x;
}

static int16_t clamp_i16(int32_t x, int16_t mn, int16_t mx) {
    if (x < mn) return mn;
    if (x > mx) return mx;
    return (int16_t)x;
}

static const char *getModeName(void) {
    return (carMode == CAR_MODE_AUTO) ? "AUTO" : "STOP";
}

static void uartPrint(const char *s) {
    HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

static void uartPrintSensors(void) {
    char msg[140];
    snprintf(msg, sizeof(msg),
             "MODE=%s S=[%u %u %u %u %u] pos=%.0f pwmL=%d pwmR=%d\r\n",
             getModeName(),
             sensorState[0], sensorState[1], sensorState[2], sensorState[3], sensorState[4],
             currentPosition, lastLeftPwm, lastRightPwm);
    uartPrint(msg);
}

/* ========================== MOTOR ========================== */
static void tb6612_standby(uint8_t enable) {
    HAL_GPIO_WritePin(STBY_PORT, STBY_PIN, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void stopMotor(void) {
    __HAL_TIM_SET_COMPARE(PWMA_TIM, PWMA_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(PWMB_TIM, PWMB_CHANNEL, 0);
    HAL_GPIO_WritePin(AIN1_PORT, AIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AIN2_PORT, AIN2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BIN1_PORT, BIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BIN2_PORT, BIN2_PIN, GPIO_PIN_RESET);
    lastLeftPwm = 0;
    lastRightPwm = 0;
}

static int16_t percentToSignedPwm(float percent, int16_t bias) {
    float mag;
    int32_t pwm;
    percent = clamp_f32(percent, -100.0f, 100.0f);
    if (percent > -0.5f && percent < 0.5f) return 0;
    mag = (percent >= 0.0f) ? percent : -percent;
    pwm = (int32_t)((mag * (float)PWM_MAX) / 100.0f);
    if (pwm > 0) {
        pwm += bias;
        if (pwm < PWM_MIN_RUN) pwm = PWM_MIN_RUN;
        if (pwm > PWM_MAX) pwm = PWM_MAX;
    }
    return (percent >= 0.0f) ? (int16_t)pwm : (int16_t)(-pwm);
}

static void setLeftMotorPwmSigned(int16_t pwm) {
#if LEFT_MOTOR_INVERT
    pwm = -pwm;
#endif
    pwm = clamp_i16(pwm, -PWM_MAX, PWM_MAX);
    if (pwm >= 0) {
        HAL_GPIO_WritePin(AIN1_PORT, AIN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AIN2_PORT, AIN2_PIN, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(PWMA_TIM, PWMA_CHANNEL, (uint16_t)pwm);
    } else {
        HAL_GPIO_WritePin(AIN1_PORT, AIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AIN2_PORT, AIN2_PIN, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(PWMA_TIM, PWMA_CHANNEL, (uint16_t)(-pwm));
    }
    lastLeftPwm = pwm;
}

static void setRightMotorPwmSigned(int16_t pwm) {
#if RIGHT_MOTOR_INVERT
    pwm = -pwm;
#endif
    pwm = clamp_i16(pwm, -PWM_MAX, PWM_MAX);
    if (pwm >= 0) {
        HAL_GPIO_WritePin(BIN1_PORT, BIN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(BIN2_PORT, BIN2_PIN, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(PWMB_TIM, PWMB_CHANNEL, (uint16_t)pwm);
    } else {
        HAL_GPIO_WritePin(BIN1_PORT, BIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BIN2_PORT, BIN2_PIN, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(PWMB_TIM, PWMB_CHANNEL, (uint16_t)(-pwm));
    }
    lastRightPwm = pwm;
}

static void setMotorPercent(float leftPercent, float rightPercent) {
    setLeftMotorPwmSigned(percentToSignedPwm(leftPercent, LEFT_PWM_BIAS));
    setRightMotorPwmSigned(percentToSignedPwm(rightPercent, RIGHT_PWM_BIAS));
}

/* ========================== SENSORS ========================== */
static void readSensors(uint8_t *s) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        GPIO_PinState raw = HAL_GPIO_ReadPin(sensorPins[i].port, sensorPins[i].pin);
#if SENSOR_ACTIVE_BLACK
        s[i] = (raw == GPIO_PIN_SET) ? 1U : 0U;
#else
        s[i] = (raw == GPIO_PIN_RESET) ? 1U : 0U;
#endif
        sensorState[i] = s[i];
    }
}

static uint8_t updateLinePosition(float *positionOut) {
    uint8_t s[SENSOR_COUNT];
    uint32_t weightedSum = 0, activeCount = 0;
    readSensors(s);
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (s[i]) {
            weightedSum += (uint32_t)(i * 1000U);
            activeCount++;
        }
    }
    if (activeCount == 0) {
        if (positionOut) {
            *positionOut = (lastSeenSide < 0) ? 0.0f : (lastSeenSide > 0) ? 4000.0f : LINE_CENTER_POS;
        }
        return 0;
    }
    *positionOut = (float)weightedSum / (float)activeCount;
    currentPosition = *positionOut;

    if (sensorState[0]) lastSeenSide = -1;
    else if (sensorState[4]) lastSeenSide = +1;
    else if (*positionOut < LINE_CENTER_POS) lastSeenSide = -1;
    else if (*positionOut > LINE_CENTER_POS) lastSeenSide = +1;

    return activeCount;
}

/* ========================== CONTROL ========================== */
static void searchLine(void) {
    if (lastSeenSide < 0)
        setMotorPercent(-SEARCH_REVERSE_SPEED, SEARCH_FORWARD_SPEED);
    else if (lastSeenSide > 0)
        setMotorPercent(SEARCH_FORWARD_SPEED, -SEARCH_REVERSE_SPEED);
    else
        setMotorPercent(30.0f, 30.0f);
}

static void lineControlTask(void) {
    float pos;
    uint8_t activeCount;
    float error, dError, correction, leftCmd, rightCmd;

    if (carMode == CAR_MODE_STOP || finishLatched) {
        stopMotor();
        return;
    }

    activeCount = updateLinePosition(&pos);
    if (activeCount == 0) {
        searchLine();
        return;
    }

    filteredPosition = 0.72f * filteredPosition + 0.28f * pos;

    if (filteredPosition <= 500.0f) {
        setMotorPercent(25.0f, 92.0f);
        return;
    }
    if (filteredPosition >= 3500.0f) {
        setMotorPercent(92.0f, 25.0f);
        return;
    }

    error = (filteredPosition - LINE_CENTER_POS) / 1000.0f;
    dError = error - lastError;
    correction = (KP * error) + (KD * dError);
    correction = clamp_f32(correction, -MAX_CORRECTION, MAX_CORRECTION);

    leftCmd = BASE_SPEED + correction;
    rightCmd = BASE_SPEED - correction;
    leftCmd = clamp_f32(leftCmd, -40.0f, MAX_SPEED);
    rightCmd = clamp_f32(rightCmd, -40.0f, MAX_SPEED);

    setMotorPercent(leftCmd, rightCmd);
    lastError = error;
}

/* ========================== MAIN ========================== */
int main(void) {
    uint32_t lastControlTick, lastDebugTick;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_USART2_UART_Init();

    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

    stopMotor();
    tb6612_standby(1);

    uartPrint("\r\n=== LINE FOLLOWER AUTO - TỐC ĐỘ CAO ===\r\n");
    uartPrint("Delay 3 giay truoc khi chay...\r\n");

    HAL_Delay(START_DELAY_MS);
    uartPrint("Bat dau chay!\r\n");

    carMode = CAR_MODE_AUTO;
    runEnabled = 1;
    finishLatched = 0;
    lastError = 0.0f;

    lastControlTick = HAL_GetTick();
    lastDebugTick = HAL_GetTick();

    while (1) {
        if ((HAL_GetTick() - lastControlTick) >= CONTROL_INTERVAL_MS) {
            lastControlTick = HAL_GetTick();
            lineControlTask();
        }
        if ((HAL_GetTick() - lastDebugTick) >= DEBUG_INTERVAL_MS) {
            lastDebugTick = HAL_GetTick();
            uartPrintSensors();
        }
    }
}

/* ========================== CLOCK ========================== */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK
                                | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1
                                | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/* ========================== TIM2 PWM ========================== */
static void MX_TIM2_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 3;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 999;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
        Error_Handler();
    }
}

/* ========================== TIM3 PWM ========================== */
static void MX_TIM3_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 3;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 999;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }
}

/* ========================== UART2 Debug ========================== */
static void MX_USART2_UART_Init(void) {
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

/* ========================== GPIO ========================== */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 | GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* ========================== ERROR ========================== */
void Error_Handler(void) {
    __disable_irq();
    while (1) {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
    (void)file;
    (void)line;
}
#endif
