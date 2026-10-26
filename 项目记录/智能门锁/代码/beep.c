#include "beep.h"
#include "delay.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"

/*
 * 无源蜂鸣器：PA8 / TIM1_CH1（高级定时器TIM1的通道1）
 * 计数时钟计算：PSC=71 -> 计数时钟 = 72MHz/(71+1) = 1MHz（即每个计数1微秒）
 * 发声原理：无源蜂鸣器需要外部方波驱动，通过改变自动重装载值ARR改变PWM频率，占空比固定50%
 */
void beep_init(void)
{
	GPIO_InitTypeDef g;              /* GPIO初始化结构体 */
	TIM_TimeBaseInitTypeDef t;        /* 定时器时基初始化结构体 */
	TIM_OCInitTypeDef oc;             /* 定时器输出比较(PWM)初始化结构体 */

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);  /* 使能GPIOA端口时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);    /* 使能高级定时器TIM1时钟 */

	/* 配置PA8为复用推挽输出（TIM1_CH1映射到PA8） */
	g.GPIO_Pin=GPIO_Pin_8; g.GPIO_Mode=GPIO_Mode_AF_PP; g.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&g);

	/* 定时器时基配置：计数时钟1MHz */
	t.TIM_Prescaler=71; t.TIM_Period=1000;                 /* 预分频71分频得1MHz；周期ARR初值1000（后续动态修改） */
	t.TIM_CounterMode=TIM_CounterMode_Up;                  /* 向上计数模式 */
	t.TIM_ClockDivision=TIM_CKD_DIV1; t.TIM_RepetitionCounter=0;  /* 时钟不分频；重复计数器0（每次更新都生效） */
	TIM_TimeBaseInit(TIM1,&t);

	/* PWM输出模式1，初始占空比50%（Pulse=500，配合ARR=1000） */
	oc.TIM_OCMode=TIM_OCMode_PWM1; oc.TIM_OutputState=TIM_OutputState_Enable;
	oc.TIM_Pulse=500; oc.TIM_OCPolarity=TIM_OCPolarity_High;   /* 比较值500；有效电平为高电平 */
	TIM_OC1Init(TIM1,&oc); TIM_OC1PreloadConfig(TIM1,TIM_OCPreload_Enable);  /* 初始化通道1并使能比较值预装载 */

	TIM_CtrlPWMOutputs(TIM1,ENABLE);  /* 高级定时器TIM1必须开启主输出(MOE)，否则无PWM波形 */
	TIM_Cmd(TIM1,ENABLE);             /* 启动TIM1计数器 */
	TIM_SetCompare1(TIM1,0);          /* 比较值清零，默认不发声（静音） */
}

/*
 * 函数：beep_on
 * 功能：以指定频率驱动无源蜂鸣器发声
 * 参数：freq_hz —— 目标发声频率(Hz)；freq_hz=0 表示停止发声
 * 说明：计数时钟1MHz，故 ARR = 1000000/频率 - 1；比较值取ARR一半实现50%方波占空比
 */
void beep_on(uint16_t freq_hz)
{
	uint16_t arr;                     /* 临时保存计算出的自动重装载值 */
	/* 判断：频率为0时直接把比较值清0并返回，即关闭声音 */
	if(freq_hz==0){ TIM_SetCompare1(TIM1,0); return; }
	arr = 1000000U/freq_hz - 1;       /* 由1MHz计数时钟反推一个周期需要的计数值 */
	TIM_SetAutoreload(TIM1, arr);     /* 更新ARR，从而改变PWM输出频率 */
	TIM_SetCompare1(TIM1, arr/2);     /* 比较值取ARR一半，得到50%占空比方波 */
}

/* 函数：beep_off —— 关闭蜂鸣器（比较值清0，输出恒低） */
void beep_off(void){ TIM_SetCompare1(TIM1,0); }

/* 函数：beep_key —— 按键短滴声：2000Hz响50ms后停止，作为按键反馈音 */
void beep_key(void){ beep_on(2000); delay_ms(50); beep_off(); }

/*
 * 函数：beep_success —— 开锁成功提示：连续两短声(1500Hz)，每声100ms，间隔50ms
 */
void beep_success(void){
	beep_on(1500); delay_ms(100); beep_off(); delay_ms(50);   /* 第一声短滴，停顿50ms */
	beep_on(1500); delay_ms(100); beep_off();                 /* 第二声短滴 */
}

/* 函数：beep_error —— 密码错误报警：500Hz低频长鸣400ms */
void beep_error(void){ beep_on(500); delay_ms(400); beep_off(); }
