#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

#define KEY_NONE              0
#define KEY_START_PAUSE       1   /* PB11：非定时模式 开启/暂停 */
#define KEY_TIME_SWITCH       2   /* PB1 ：定时档位切换 10/30/60s */
#define KEY_TIMER_START_PAUSE 3   /* PA6 ：定时模式 开启/暂停 */
#define KEY_SPEED             4   /* PA4 ：三档风速切换 */
#define KEY_TIME_PLUS_1S      5   /* PA2 ：定时 +1s */
#define KEY_TIME_MINUS_1S     6   /* PA0 ：定时 -1s */

void Key_Init(void);
uint8_t Key_GetNum(void);

#endif
