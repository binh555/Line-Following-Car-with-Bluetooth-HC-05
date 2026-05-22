#ifndef SENSOR_H
#define SENSOR_H
#include <stdint.h>

uint8_t updateLinePosition(float *positionOut);
uint8_t isAllBlackStable(void);
uint8_t isAllBlack(void);

#endif
