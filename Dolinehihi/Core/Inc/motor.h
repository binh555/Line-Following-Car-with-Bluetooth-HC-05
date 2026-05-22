#ifndef MOTOR_H
#define MOTOR_H

#include "pinmap.h"
#include "config.h"

void tb6612_standby(uint8_t enable);
void stopMotor(void);
void setMotorPercent(float leftPercent, float rightPercent);
void manualForward(void);
void manualBackward(void);
void manualLeft(void);
void manualRight(void);

#endif
