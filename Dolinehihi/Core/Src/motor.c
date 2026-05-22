#include "stm32f4xx_hal.h"
#include "main.h"
#include "config.h"
#include "pinmap.h"
#include "motor.h"

static int16_t lastLeftPwm = 0;
static int16_t lastRightPwm = 0;

static float clamp_f32(float x, float mn, float mx){
    if(x<mn) return mn;
    if(x>mx) return mx;
    return x;
}

static int16_t clamp_i16(int32_t x,int16_t mn,int16_t mx){
    if(x<mn) return mn;
    if(x>mx) return mx;
    return (int16_t)x;
}

static int16_t percentToSignedPwm(float percent,int16_t bias){
    float mag; int32_t pwm;
    percent = clamp_f32(percent,-100.0f,100.0f);
    if(percent>-0.5f && percent<0.5f) return 0;
    mag = (percent>=0.0f)?percent:-percent;
    pwm = (int32_t)(mag*PWM_MAX/100.0f);
    if(pwm>0){ pwm+=bias; if(pwm<PWM_MIN_RUN) pwm=PWM_MIN_RUN; if(pwm>PWM_MAX) pwm=PWM_MAX;}
    return (percent>=0.0f)?(int16_t)pwm:(int16_t)(-pwm);
}

static void setLeftMotorPwmSigned(int16_t pwm){
#if LEFT_MOTOR_INVERT
    pwm=-pwm;
#endif
    pwm=clamp_i16(pwm,-PWM_MAX,PWM_MAX);
    if(pwm>=0){
        HAL_GPIO_WritePin(AIN1_PORT,AIN1_PIN,GPIO_PIN_SET);
        HAL_GPIO_WritePin(AIN2_PORT,AIN2_PIN,GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(PWMA_TIM,PWMA_CHANNEL,pwm);
    }else{
        HAL_GPIO_WritePin(AIN1_PORT,AIN1_PIN,GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AIN2_PORT,AIN2_PIN,GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(PWMA_TIM,PWMA_CHANNEL,-pwm);
    }
    lastLeftPwm=pwm;
}

static void setRightMotorPwmSigned(int16_t pwm){
#if RIGHT_MOTOR_INVERT
    pwm=-pwm;
#endif
    pwm=clamp_i16(pwm,-PWM_MAX,PWM_MAX);
    if(pwm>=0){
        HAL_GPIO_WritePin(BIN1_PORT,BIN1_PIN,GPIO_PIN_SET);
        HAL_GPIO_WritePin(BIN2_PORT,BIN2_PIN,GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(PWMB_TIM,PWMB_CHANNEL,pwm);
    }else{
        HAL_GPIO_WritePin(BIN1_PORT,BIN1_PIN,GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BIN2_PORT,BIN2_PIN,GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(PWMB_TIM,PWMB_CHANNEL,-pwm);
    }
    lastRightPwm=pwm;
}

void tb6612_standby(uint8_t enable){ HAL_GPIO_WritePin(STBY_PORT,STBY_PIN,enable?GPIO_PIN_SET:GPIO_PIN_RESET); }
void stopMotor(void){ setLeftMotorPwmSigned(0); setRightMotorPwmSigned(0); }
void setMotorPercent(float l,float r){ setLeftMotorPwmSigned(percentToSignedPwm(l,LEFT_PWM_BIAS)); setRightMotorPwmSigned(percentToSignedPwm(r,RIGHT_PWM_BIAS)); }
void manualForward(void){ setMotorPercent(100.0f,100.0f);}
void manualBackward(void){ setMotorPercent(-80.0f,-80.0f);}
void manualLeft(void){ setMotorPercent(-35.0f,100.0f);}
void manualRight(void){ setMotorPercent(100.0f,-35.0f);}
