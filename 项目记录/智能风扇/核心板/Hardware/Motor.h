#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

void Motor_Init(void);
void Motor_SetSpeed(uint8_t speed);
void Motor_Start(uint8_t speed);
void Motor_Stop(void);
uint8_t Motor_IsRunning(void);

#endif
