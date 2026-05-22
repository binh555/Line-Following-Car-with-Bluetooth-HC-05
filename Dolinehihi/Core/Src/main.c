#include "main.h"
#include "motor.h"
#include "sensor.h"
#include "pid.h"
#include "encoder.h"
#include "bluetooth.h"

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

void SystemClock_Config(void){}
void MX_GPIO_Init(void){}
void MX_TIM2_Init(void){}
void MX_TIM3_Init(void){}
void MX_USART1_UART_Init(void){}
void MX_USART2_UART_Init(void){}

int main(void){
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    HAL_TIM_PWM_Start(&htim2,TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_2);
    tb6612_standby(0);
    stopMotor();
    HAL_Delay(START_DELAY_MS);

    bluetoothStartRx();

    while(1){
        bluetoothTask();
        lineControlTask();
        reportEncoderAndSensors();
    }
}
