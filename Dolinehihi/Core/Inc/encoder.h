#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

extern volatile uint32_t leftPulseCount;
extern volatile uint32_t rightPulseCount;

void reportEncoderAndSensors(void);

#endif
