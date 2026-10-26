#include "key.h"
#include "delay.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"

/* 4x4矩阵键盘引脚分配：行线 PA0~PA3（输出），列线 PA4~PA7（上拉输入） */
static const uint16_t row_pin[4]={GPIO_Pin_0,GPIO_Pin_1,GPIO_Pin_2,GPIO_Pin_3};   /* 4根行线对应的引脚号 */
static const uint16_t col_pin[4]={GPIO_Pin_4,GPIO_Pin_5,GPIO_Pin_6,GPIO_Pin_7};   /* 4根列线对应的引脚号 */

/*
 * 键值映射表（按实际贴膜布局，行号row、列号col与物理按键一一对应）
 * 第一行 S1~S4 = 0 1 2 3
 * 第二行 S5~S8 = 4 5 6 7
 * 第三行 S9~S12 = 8 9 删除(KEY_DELETE) 确认(KEY_CONFIRM)
 * 第四行未使用，全部填 KEY_NONE
 */
static const uint16_t key_value[4][4]={
	{KEY_0,KEY_1,KEY_2,KEY_3},
	{KEY_4,KEY_5,KEY_6,KEY_7},
	{KEY_8,KEY_9,KEY_DELETE,KEY_CONFIRM},
	{KEY_NONE,KEY_NONE,KEY_NONE,KEY_NONE}
};

/*
 * 函数：key_init
 * 功能：初始化矩阵键盘GPIO
 * 配置：行线PA0~PA3为推挽输出（主动拉低进行扫描）；列线PA4~PA7为上拉输入（默认高电平，按下被行线拉低）
 */
void key_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;                 /* GPIO初始化结构体 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);  /* 使能GPIOA端口时钟 */

	/* 配置 PA0~PA3 行线为推挽输出 */
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStruct);

	/* 配置 PA4~PA7 列线为上拉输入（无按键时读到高电平） */
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7;
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_Init(GPIOA,&GPIO_InitStruct);

	/* 初始化时把所有行线先拉高，避免悬空误触发 */
	GPIO_WriteBit(GPIOA,GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3,Bit_SET);
}

/*
 * 函数：key_scan
 * 功能：扫描一次矩阵键盘，带机械消抖和松手检测
 * 原理：依次把每一行单独拉低，读取4根列线电平；某列读到低电平说明该行该列交叉处按键被按下
 * 返回：按下按键对应的键值；无按键返回 KEY_NONE(0xFF)
 */
uint8_t key_scan(void)
{
	uint8_t row,col;                  /* row=当前扫描行号(0~3)，col=当前检测列号(0~3) */
	/* 外层循环：逐行扫描，共4行 */
	for(row=0;row<4;row++)
	{
		/* 先把所有行线拉高，再单独把当前row行拉低，形成"单行低、其余高"的扫描电平 */
		GPIO_WriteBit(GPIOA,GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3,Bit_SET);
		GPIO_WriteBit(GPIOA,row_pin[row],Bit_RESET);
		delay_us(10);   /* 等待电平稳定，避免刚拉低就读到旧电平 */

		/* 内层循环：检测当前行的4根列线 */
		for(col=0;col<4;col++)
		{
			/* 判断：该列读到低电平，说明可能有按键按下（列线被拉低的行连通） */
			if(GPIO_ReadInputDataBit(GPIOA,col_pin[col])==0)
			{
				delay_ms(10);   /* 消抖：延时10ms后复判，滤除机械触点抖动 */
				/* 判断：复判仍为低电平，确认是真按键而非抖动 */
				if(GPIO_ReadInputDataBit(GPIOA,col_pin[col])==0)
				{
					uint8_t key = key_value[row][col];   /* 按行、列索引查表得到对应键值 */
					while(GPIO_ReadInputDataBit(GPIOA,col_pin[col])==0); /* 松手检测：循环等待该列回到高电平，即等待按键松开 */
					delay_ms(10);                       /* 松手后再消抖10ms，防止连按 */
					GPIO_WriteBit(GPIOA,GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3,Bit_SET); /* 扫描结束，所有行线恢复拉高 */
					return key;                         /* 返回识别到的键值 */
				}
			}
		}
	}
	/* 4行全部扫描完仍未发现按键：恢复行线电平并返回无按键 */
	GPIO_WriteBit(GPIOA,GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3,Bit_SET);
	return KEY_NONE;
}
