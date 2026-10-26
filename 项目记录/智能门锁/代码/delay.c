#include "delay.h"

/*
 * 微秒/毫秒延时
 * 系统主频72MHz，SysTick时钟源为HCLK/8=9MHz
 * 即每微秒计数9次，采用轮询方式不开中断
 */
void delay_us(uint32_t nus)
{
    uint32_t temp;
    SysTick->LOAD = nus * 9U;     /* 重装载值：9MHz下nus微秒对应的计数 */
    SysTick->VAL  = 0x00;         /* 清空当前计数器 */
    SysTick->CTRL = 0x00000005;  /* 使能SysTick，时钟源HCLK/8 */
    do { temp = SysTick->CTRL; } while ((temp & 0x01) && !(temp & (1<<16)));
    SysTick->CTRL = 0x00000004;  /* 关闭定时器 */
    SysTick->VAL  = 0x00;
}

void delay_ms(uint32_t nms)
{
    while (nms--) delay_us(1000);
}
