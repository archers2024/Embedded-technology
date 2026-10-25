#include "Alarm.h"
#include "Delay.h"

#define RUN_LED_PORT  GPIOC
#define RUN_LED_PIN   GPIO_Pin_13
#define SAFE_LED_PORT GPIOB
#define SAFE_LED_PIN  GPIO_Pin_0
#define BUZZER_PORT   GPIOB
#define BUZZER_PIN    GPIO_Pin_1

static uint8_t s_alarm_enable;
static uint32_t s_last_blink;
static uint8_t s_blink_state;

void Alarm_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = SAFE_LED_PIN | BUZZER_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = RUN_LED_PIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    Alarm_RunLed(0);
    Alarm_SafeLed(0);
    Alarm_Buzzer(0);
}

void Alarm_RunLed(uint8_t on)
{
    if (on)
    {
        GPIO_ResetBits(RUN_LED_PORT, RUN_LED_PIN);
    }
    else
    {
        GPIO_SetBits(RUN_LED_PORT, RUN_LED_PIN);
    }
}

void Alarm_SafeLed(uint8_t on)
{
    /* 模块为低电平点亮：亮=拉低，灭=拉高（上电默认输出高，保持不亮） */
    if (on)
    {
        GPIO_ResetBits(SAFE_LED_PORT, SAFE_LED_PIN);
    }
    else
    {
        GPIO_SetBits(SAFE_LED_PORT, SAFE_LED_PIN);
    }
}

void Alarm_Buzzer(uint8_t on)
{
    s_alarm_enable = on;
    if (on)
    {
        /* 低电平触发：响 */
        GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
    }
    else
    {
        /* 不响：拉高 */
        GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
    }
}

void Alarm_SafeBlinkTask(void)
{
    if (!s_alarm_enable)
    {
        return;
    }

    if ((Millis() - s_last_blink) >= 250)
    {
        s_last_blink = Millis();
        s_blink_state = !s_blink_state;
        Alarm_SafeLed(s_blink_state);
        /* 低电平触发：blink_state=1 时响 */
        GPIO_WriteBit(BUZZER_PORT, BUZZER_PIN, s_blink_state ? Bit_RESET : Bit_SET);
    }
}
