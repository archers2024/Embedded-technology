#include "servo.h"
#include "delay.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"

/*
 * 舵机（SG90类）：PB0 / TIM3_CH3，固定50Hz（周期20ms）标准舵机信号
 * PSC=72-1 -> APB1定时器倍频后为72MHz，72分频得到计数时钟1MHz（每计数1微秒）
 * ARR=20000-1 -> 20000个计数 = 20ms = 50Hz 周期
 * 角度与脉宽对应：0度 -> 比较值500(0.5ms)；180度 -> 比较值2500(2.5ms)
 * 舵机靠一个周期内高电平脉宽决定转角，与占空比绝对值无关
 */
void servo_init(void)
{
	GPIO_InitTypeDef g;              /* GPIO初始化结构体 */
	TIM_TimeBaseInitTypeDef t;        /* 定时器时基结构体 */
	TIM_OCInitTypeDef oc;             /* 输出比较PWM结构体 */

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);  /* 使能GPIOB端口时钟 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);    /* 使能TIM3时钟（TIM3挂在APB1总线） */

	/* PB0复用推挽输出，作为TIM3_CH3舵机信号脚 */
	g.GPIO_Pin=GPIO_Pin_0; g.GPIO_Mode=GPIO_Mode_AF_PP; g.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&g);

	/* 时基：1MHz计数，ARR=20000 得到20ms周期(50Hz) */
	t.TIM_Prescaler=72-1; t.TIM_Period=20000-1;
	t.TIM_CounterMode=TIM_CounterMode_Up; t.TIM_ClockDivision=TIM_CKD_DIV1;  /* 向上计数；时钟不分频 */
	TIM_TimeBaseInit(TIM3,&t);

	/* PWM模式1，初始比较值500对应0度位置，高电平有效 */
	oc.TIM_OCMode=TIM_OCMode_PWM1; oc.TIM_OutputState=TIM_OutputState_Enable;
	oc.TIM_Pulse=500; oc.TIM_OCPolarity=TIM_OCPolarity_High;
	TIM_OC3Init(TIM3,&oc); TIM_OC3PreloadConfig(TIM3,TIM_OCPreload_Enable);  /* 初始化通道3并使能比较值预装载 */

	TIM_Cmd(TIM3,ENABLE);             /* 启动TIM3计数器，开始输出PWM */
	servo_lock();   /* 上电先回到关锁位置，保证初始状态为锁定 */
}

/*
 * 函数：servo_set_angle
 * 功能：把舵机转到指定角度
 * 参数：angle —— 目标角度，范围0~180度
 */
void servo_set_angle(uint8_t angle)
{
	uint16_t pulse;                   /* 换算后的比较值（脉宽计数值） */
	/* 判断：角度超过180则截断为180，防止越界映射出错误脉宽 */
	if(angle>180) angle=180;
	pulse = 500 + (uint16_t)angle*2000U/180U;   /* 线性映射：0度=500，180度=2500(0.5~2.5ms) */
	TIM_SetCompare3(TIM3,pulse);     /* 写入通道3比较值，改变高电平脉宽从而改变转角 */
}

/* 函数：servo_unlock —— 开锁：转到90度位置，保持500ms让机械结构到位（角度可按实际机械微调） */
void servo_unlock(void){ servo_set_angle(90); delay_ms(500); }
/* 函数：servo_lock —— 关锁：回到0度位置，保持500ms让机械结构到位 */
void servo_lock(void){ servo_set_angle(0); delay_ms(500); }
