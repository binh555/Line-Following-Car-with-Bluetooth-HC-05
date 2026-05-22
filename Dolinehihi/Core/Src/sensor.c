#include "sensor.h"
#include "pinmap.h"
#include "stm32f4xx_hal.h"
#include "config.h"

static uint8_t sensorState[SENSOR_COUNT]={0};
static int8_t lastSeenSide=0;

static void readSensors(uint8_t *s){
    for(int i=0;i<SENSOR_COUNT;i++){
        GPIO_PinState raw=HAL_GPIO_ReadPin(sensorPins[i].port,sensorPins[i].pin);
#if SENSOR_ACTIVE_BLACK
        s[i]=(raw==GPIO_PIN_SET)?1:0;
#else
        s[i]=(raw==GPIO_PIN_RESET)?1:0;
#endif
        sensorState[i]=s[i];
    }
}

uint8_t updateLinePosition(float *positionOut){
    uint8_t s[SENSOR_COUNT]; uint32_t sum=0; uint32_t cnt=0;
    readSensors(s);
    for(int i=0;i<SENSOR_COUNT;i++) if(s[i]){sum+=i*1000U; cnt++;}
    if(cnt==0){
        if(positionOut) *positionOut = (lastSeenSide<0)?0.0f:(lastSeenSide>0?4000.0f:LINE_CENTER_POS);
        return 0;
    }
    if(positionOut) *positionOut = (float)sum/cnt;
    if(sensorState[0]) lastSeenSide=-1;
    else if(sensorState[4]) lastSeenSide=1;
    else{
        if(positionOut && *positionOut<LINE_CENTER_POS) lastSeenSide=-1;
        else lastSeenSide=1;
    }
    return (uint8_t)cnt;
}

uint8_t isAllBlackStable(void){ for(int i=0;i<SENSOR_COUNT;i++) if(!sensorState[i]) return 0; return 1; }
uint8_t isAllBlack(void){ for(int i=0;i<SENSOR_COUNT;i++) if(sensorState[i]!=0) return 0; return 1; }
