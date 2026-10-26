#ifndef __SERVO_H
#define __SERVO_H
#include "stm32f10x.h"
void servo_init(void);
void servo_set_angle(uint8_t angle);
void servo_unlock(void);
void servo_lock(void);
#endif
