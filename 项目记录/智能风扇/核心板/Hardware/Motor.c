#include "Motor.h"

#define MOTOR_AIN1_PORT GPIOA
#define MOTOR_AIN1_PIN  GPIO_Pin_4
#define MOTOR_AIN2_PORT GPIOA
#define MOTOR_AIN2_PIN  GPIO_Pin_5
#define MOTOR_PWM_PORT  GPIOA
#define MOTOR_PWM_PIN   GPIO_Pin_6
#define MOTOR_STBY_PORT GPIOA
#define MOTOR_STBY_PIN  GPIO_Pin_7

static uint8_t s_motor_running;

void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = MOTOR_AIN1_PIN | MOTOR_AIN2_PIN | MOTOR_STBY_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = MOTOR_PWM_PIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 100 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 36 - 1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_Cmd(TIM3, ENABLE);

    Motor_Stop();
}

void Motor_SetSpeed(uint8_t speed)
{
    if (speed > 100)
    {
        speed = 100;
    }
    TIM_SetCompare1(TIM3, speed);
}

void Motor_Start(uint8_t speed)
{
    GPIO_SetBits(MOTOR_STBY_PORT, MOTOR_STBY_PIN);
    GPIO_SetBits(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN);
    GPIO_ResetBits(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN);
    Motor_SetSpeed(speed);
    s_motor_running = 1;
}

void Motor_Stop(void)
{
    Motor_SetSpeed(0);
    GPIO_ResetBits(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN);
    GPIO_ResetBits(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN);
    GPIO_ResetBits(MOTOR_STBY_PORT, MOTOR_STBY_PIN);
    s_motor_running = 0;
}

uint8_t Motor_IsRunning(void)
{
    return s_motor_running;
}
