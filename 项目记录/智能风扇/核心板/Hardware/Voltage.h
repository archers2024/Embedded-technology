#ifndef __VOLTAGE_H
#define __VOLTAGE_H

#include "stm32f10x.h"

void Voltage_Init(void);
uint16_t Voltage_GetMilliVolt(void);

#endif
