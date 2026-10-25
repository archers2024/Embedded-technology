#ifndef __SAFETY_H
#define __SAFETY_H

#include "stm32f10x.h"

#define TCRT_ACTIVE_LEVEL 0

void Safety_Init(void);
uint8_t Safety_IsBlocked(void);

#endif
