#ifndef __ALARM_H
#define __ALARM_H

#include "stm32f10x.h"

void Alarm_Init(void);
void Alarm_RunLed(uint8_t on);
void Alarm_SafeLed(uint8_t on);
void Alarm_Buzzer(uint8_t on);
void Alarm_SafeBlinkTask(void);

#endif
