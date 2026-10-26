#ifndef OLED_H
#define OLED_H

#include "stm32f10x.h"

void My_I2C_Init(void);
void OLED_Init(void);                 /* ???????????I2C + SSD1306???? + ???? */
void OLED_Clear(void);
void Show_Char(char a,uint8_t row,uint8_t col);
void Show_String(const char *p,uint8_t row,uint8_t col);
void Cursor_Location(uint8_t page,uint8_t col);
int My_I2C_SendBytes(I2C_TypeDef *I2Cx,uint8_t Addr,uint8_t *pData,uint16_t Size);

#endif
