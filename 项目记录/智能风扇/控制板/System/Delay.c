#include "Delay.h"

static volatile uint32_t s_ms_tick;

void Delay_Init(void)
{
    SysTick_Config(SystemCoreClock / 1000);
}

void SysTick_Handler(void)
{
    s_ms_tick++;
}

uint32_t Millis(void)
{
    return s_ms_tick;
}

void Delay_us(uint32_t us)
{
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    uint32_t start = SysTick->VAL;
    uint32_t reload = SysTick->LOAD + 1;
    uint32_t elapsed = 0;

    while (elapsed < ticks)
    {
        uint32_t now = SysTick->VAL;
        if (start >= now)
        {
            elapsed += start - now;
        }
        else
        {
            elapsed += start + reload - now;
        }
        start = now;
    }
}

void Delay_ms(uint32_t ms)
{
    uint32_t start = Millis();
    while ((Millis() - start) < ms)
    {
    }
}
