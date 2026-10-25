#include "Key.h"
#include "Delay.h"

#define KEY_DEBOUNCE_MS 20

/* 0=空闲 1=等待去抖稳定 2=已触发等待松开 */
static uint8_t  s_state;
static uint8_t  s_key;
static uint32_t s_press_ms;

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 注意：必须同时开启 GPIOA 和 GPIOB 的时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* 禁用 JTAG，保留 SWD（如果不需要释放 PA15/PB3/PB4，可以删掉这行） */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    /* 初始化 GPIOB：PB11（按键1 非定时开启/暂停）、PB1（按键2 定时档位），上拉输入 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 初始化 GPIOA：PA6（按键3 定时开启/暂停）、PA4（按键4 风速）、
       PA2（按键5 定时+1s）、PA0（按键6 定时-1s），上拉输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_4 | GPIO_Pin_2 | GPIO_Pin_0;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    s_state = 0;
    s_key = KEY_NONE;
}

static uint8_t Key_ScanRaw(void)
{
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0)
    {
        return KEY_START_PAUSE;
    }
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)
    {
        return KEY_TIME_SWITCH;
    }
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == 0)
    {
        return KEY_TIMER_START_PAUSE;
    }
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) == 0)
    {
        return KEY_SPEED;
    }
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 0)
    {
        return KEY_TIME_PLUS_1S;
    }
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0)
    {
        return KEY_TIME_MINUS_1S;
    }
    return KEY_NONE;
}

/* 非阻塞按键：每次按下只返回一次，不卡主循环 */
uint8_t Key_GetNum(void)
{
    uint8_t raw = Key_ScanRaw();

    switch (s_state)
    {
        case 0: /* 空闲：检测到按下 */
            if (raw != KEY_NONE)
            {
                s_key = raw;
                s_press_ms = Millis();
                s_state = 1;
            }
            break;

        case 1: /* 去抖：稳定 20ms 才确认触发 */
            if (raw == s_key)
            {
                if ((Millis() - s_press_ms) >= KEY_DEBOUNCE_MS)
                {
                    s_state = 2;
                    return s_key;
                }
            }
            else
            {
                s_state = 0;
            }
            break;

        case 2: /* 等待完全松开 */
            if (raw == KEY_NONE)
            {
                s_state = 0;
            }
            break;
    }

    return KEY_NONE;
}
