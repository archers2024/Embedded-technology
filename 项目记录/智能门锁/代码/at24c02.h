#ifndef __AT24C02_H
#define __AT24C02_H
#include "stm32f10x.h"
void at24c02_init(void);
void at24c02_write(uint8_t addr, uint8_t data);
uint8_t at24c02_read(uint8_t addr);
#endif
