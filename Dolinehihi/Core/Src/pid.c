#include "pid.h"
#include "sensor.h"
#include "motor.h"
#include "config.h"

static float lastError = 0.0f;
static uint8_t finishLatched = 0;
static uint8_t runEnabled = 0;
static uint32_t allBlackStable = 0;

static float clamp_f32(float x, float mn, float mx) {
    if (x < mn) return mn;
    if (x > mx) return mx;
    return x;
}

void searchLine(void) {
    if (lastError < 0) {
        setMotorPercent(-SEARCH_REVERSE_SPEED, SEARCH_FORWARD_SPEED);
    } else if (lastError > 0) {
        setMotorPercent(SEARCH_FORWARD_SPEED, -SEARCH_REVERSE_SPEED);
    } else {
        setMotorPercent(30.0f, 30.0f);
    }
}

void lineControlTask(void) {
    float pos;
    uint8_t activeCount;
    float error, dError, correction;
    float leftCmd, rightCmd;

    if (!runEnabled || finishLatched) {
        stopMotor();
        return;
    }

    activeCount = updateLinePosition(&pos);
    if (activeCount == 0U) {
        searchLine();
        return;
    }

    // PD Filter
    float filteredPosition = 0.45f * pos + 0.55f * pos;

#if STOP_ON_ALL_BLACK
    if (isAllBlackStable()) allBlackStable++;
    else allBlackStable = 0;

    if (allBlackStable >= ALL_BLACK_CONFIRM_CYCLES) {
        finishLatched = 1;
        stopMotor();
        return;
    }
#endif

    error = (filteredPosition - LINE_CENTER_POS)/1000.0f;
    dError = error - lastError;
    correction = (KP*error) + (KD*dError);
    correction = clamp_f32(correction, -MAX_CORRECTION, MAX_CORRECTION);

    leftCmd = clamp_f32(BASE_SPEED + correction, MIN_AUTO_SPEED, MAX_SPEED);
    rightCmd = clamp_f32(BASE_SPEED - correction, MIN_AUTO_SPEED, MAX_SPEED);

    setMotorPercent(leftCmd, rightCmd);

    lastError = error;
}
