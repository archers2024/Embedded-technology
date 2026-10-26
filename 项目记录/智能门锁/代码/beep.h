#ifndef __BEEP_H
#define __BEEP_H
#include "stm32f10x.h"
void beep_init(void);
void beep_on(uint16_t freq_hz);
void beep_off(void);
void beep_key(void);
void beep_success(void);
void beep_error(void);
#endif
