#include "encoder.h"
#include "main.h"
#include <stdio.h>
#include <string.h>   // fix strlen warning

volatile uint32_t leftPulseCount=0;
volatile uint32_t rightPulseCount=0;
static uint32_t lastLeftCount=0;
static uint32_t lastRightCount=0;

void reportEncoderAndSensors(void){
    uint32_t leftPPS=leftPulseCount-lastLeftCount;
    uint32_t rightPPS=rightPulseCount-lastRightCount;
    lastLeftCount=leftPulseCount;
    lastRightCount=rightPulseCount;

    char msg[180];
    snprintf(msg,sizeof(msg),"[AUTO REPORT] LeftPPS=%lu RightPPS=%lu\r\n",leftPPS,rightPPS);
    HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),10);
}
