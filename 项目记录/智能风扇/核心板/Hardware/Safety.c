#include "Safety.h"
#include "Delay.h"

static uint8_t  s_blocked;      /* 去抖后的稳定状态 */
static uint8_t  s_last_raw;     /* 上一次原始电平 */
static uint32_t s_debounce_ms;  /* 电平变化时刻 */

void Safety_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 上电时直接采用当前电平，避免误报/漏报 */
    s_last_raw = ((uint8_t)GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == TCRT_ACTIVE_LEVEL) ? 1 : 0;
    s_blocked  = s_last_raw;
    s_debounce_ms = Millis();
}

uint8_t Safety_IsBlocked(void)
{
    uint8_t raw = ((uint8_t)GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == TCRT_ACTIVE_LEVEL) ? 1 : 0;

    /* 电平跳变：记录时刻，等待稳定 */
    if (raw != s_last_raw)
    {
        s_last_raw = raw;
        s_debounce_ms = Millis();
    }
    /* 电平保持 50ms 以上才更新状态（去抖） */
    else if ((Millis() - s_debounce_ms) >= 50)
    {
        s_blocked = raw;
    }

    return s_blocked;
}
