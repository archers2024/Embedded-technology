#ifndef KEY_H
#define KEY_H


#include "stm32f10x.h"

//键值宏定义
#define KEY_0 0
#define KEY_1 1
#define KEY_2 2
#define KEY_3 3
#define KEY_4 4
#define KEY_5 5
#define KEY_6 6
#define KEY_7 7
#define KEY_8 8
#define KEY_9 9
#define KEY_DELETE 10
#define KEY_CONFIRM 11
#define KEY_NONE 0xff

void key_init(void);//初始化key
uint8_t key_scan(void);//扫描取键值

#endif
