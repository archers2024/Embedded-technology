#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "key.h"
#include "OLED.h"
#include "beep.h"
#include "servo.h"
#include "at24c02.h"
#include "delay.h"

/* ==================== 宏定义 ==================== */
#define PWD_LEN        4        /* 密码固定长度：4位数字 */
#define MAX_ERR        3        /* 允许密码错误的最大次数，达到后进入锁定 */
#define LOCK_SEC       30       /* 锁定倒计时秒数：错误3次后锁定30秒 */
#define PWD_STORE_ADDR 0x00     /* EEPROM中保存4位密码的起始单元地址 */
#define PWD_FLAG_ADDR  0x10     /* EEPROM中"密码已初始化"标志位的单元地址(值0xAA表示已写过) */
/* ==================== 函数声明 ==================== */
uint8_t read_pwd(char *prompt, uint8_t *buf);
/*
 * 系统状态机：用枚举定义密码锁所处的各个工作状态
 */
typedef enum {
	S_INPUT = 0,    /* 状态：输入开锁密码（主界面） */
	S_OPEN,         /* 状态：开锁成功（本工程开锁动作为瞬时执行，未单独停留） */
	S_LOCK,         /* 状态：错误次数超限，锁定倒计时中 */
	S_ADMIN_OLD,    /* 管理员模式：先输入旧密码进行校验 */
	S_ADMIN_NEW1,   /* 管理员模式：输入第一遍新密码 */
	S_ADMIN_NEW2    /* 管理员模式：输入第二遍新密码进行确认比对 */
} SysState;

/* ==================== 全局静态变量 ==================== */
static uint8_t pwd[PWD_LEN];      /* 已保存的正确密码（从EEPROM读出） */
static uint8_t input[PWD_LEN];   /* 用户当前输入的密码缓冲区 */
static uint8_t new_pwd[PWD_LEN];  /* 管理员设置的新密码缓冲区 */
static uint8_t idx = 0;           /* 当前已输入的位数索引（0~4） */
static uint8_t err_cnt = 0;       /* 连续密码错误计数 */
static SysState state = S_INPUT;  /* 当前状态机所处状态，上电默认进入输入密码 */

/*
 * 函数：pwd_load
 * 功能：上电时从EEPROM读取密码；若首次使用（标志位不是0xAA）则写入默认密码1234
 */
static void pwd_load(void)
{
	uint8_t i;
	/* 判断：标志位不等于0xAA说明EEPROM从未初始化，需要写入默认密码 */
	if(at24c02_read(PWD_FLAG_ADDR) != 0xAA)
	{
		pwd[0]=1; pwd[1]=2; pwd[2]=3; pwd[3]=4;   /* 默认密码1234 */
		/* 循环：把4位默认密码逐个写入EEPROM密码区 */
		for(i=0;i<PWD_LEN;i++) at24c02_write(PWD_STORE_ADDR+i, pwd[i]);
		at24c02_write(PWD_FLAG_ADDR, 0xAA);        /* 写入已初始化标志0xAA */
	}
	else
	{
		/* 否则：EEPROM已有密码，循环把4位密码读回到pwd数组 */
		for(i=0;i<PWD_LEN;i++) pwd[i] = at24c02_read(PWD_STORE_ADDR+i);
	}
}

/*
 * 函数：pwd_save
 * 功能：把管理员设置的新密码保存到EEPROM，并同步到内存pwd数组
 */
static void pwd_save(void)
{
	uint8_t i;
	/* 循环：把new_pwd的4位逐个写入EEPROM密码区 */
	for(i=0;i<PWD_LEN;i++) at24c02_write(PWD_STORE_ADDR+i, new_pwd[i]);
	at24c02_write(PWD_FLAG_ADDR, 0xAA);          /* 置已初始化标志 */
	/* 循环：把新密码同步到内存pwd数组，使本次运行立即生效 */
	for(i=0;i<PWD_LEN;i++) pwd[i] = new_pwd[i];
}

/*
 * 函数：led_init
 * 功能：初始化板载LED（PC13，低电平点亮）
 */
static void led_init(void)
{
	GPIO_InitTypeDef g;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);  /* 使能GPIOC时钟 */
	g.GPIO_Pin = GPIO_Pin_13;
	g.GPIO_Mode = GPIO_Mode_Out_PP;               /* 推挽输出 */
	g.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &g);
	GPIO_SetBits(GPIOC, GPIO_Pin_13);             /* 默认拉高，LED熄灭（低电平亮） */
}
/* 函数：led_on —— PC13拉低，点亮LED */
static void led_on(void)  { GPIO_ResetBits(GPIOC, GPIO_Pin_13); }
/* 函数：led_off —— PC13拉高，熄灭LED */
static void led_off(void) { GPIO_SetBits(GPIOC, GPIO_Pin_13); }

/*
 * 函数：show_stars
 * 功能：在指定行用星号"*"回显已输入的位数（密码不显示明文）
 * 参数：row=显示行；count=当前已输入的位数
 */
static void show_stars(uint8_t row, uint8_t count)
{
	uint8_t i;
	/* 循环：遍历4个密码位，已输入的位置显示'*'，未输入的位置显示空格 */
	for(i=0;i<PWD_LEN;i++)
		Show_Char((i<count)?'*':' ', row, 48+i*16);
}

/*
 * 函数：read_pwd
 * 功能：通用的4位数字密码录入子流程（清屏、显示提示、采集数字/删除/确认）
 * 参数：prompt=顶部提示字符串；buf=录入结果存放缓冲区
 * 返回：1=已输满4位并按确认；0=未满4位按确认
 */
static uint8_t read_pwd(char *prompt, uint8_t *buf)
{
	uint8_t key;
	OLED_Clear();                 /* 清屏 */
	Show_String(prompt, 0, 0);    /* 顶部显示提示语 */
	idx = 0;                      /* 位数索引清零 */
	/* 循环：持续扫描键盘直到按下确认键才退出 */
	while(1)
	{
		key = key_scan();
		/* 判断：无按键则跳过本次，继续循环 */
		if(key == KEY_NONE) continue;
		/* 判断：按下的是数字键(0~9) */
		if(key <= 9)
		{
			/* 判断：还没输满4位才接收，防止越界写buf */
			if(idx < PWD_LEN)
			{
				beep_key();                 /* 按键提示音 */
				buf[idx++] = key;           /* 存入缓冲区并索引后移 */
				show_stars(1, idx);         /* 刷新星号回显 */
			}
		}
		/* 否则判断：按下删除键 */
		else if(key == KEY_DELETE)
		{
			beep_key();
			/* 判断：已有输入才回退一位 */
			if(idx > 0) { idx--; show_stars(1, idx); }
		}
		/* 否则判断：按下确认键 */
		else if(key == KEY_CONFIRM)
		{
			beep_key();
			/* 输满4位返回1，否则返回0 */
			return (idx == PWD_LEN) ? 1 : 0;
		}
	}
}

/* ==================== 主函数 ==================== */
int main(void)
{
	uint8_t key, i, sec, ok;

	delay_us(1);                  /* 上电短暂延时，等待外设稳定 */
	led_init();                   /* 初始化LED */
	led_on(); delay_ms(200); led_off(); delay_ms(200);   /* LED闪烁两次作为上电自检提示 */
	OLED_Init();                  /* 初始化OLED显示屏 */
	key_init();                   /* 初始化矩阵键盘 */
	beep_init();                  /* 初始化蜂鸣器 */
	servo_init();                 /* 初始化舵机（默认回到关锁位） */
	at24c02_init();              /* 初始化EEPROM */
	pwd_load();                   /* 从EEPROM加载密码（首次写默认1234） */
	led_on(); delay_ms(100); led_off();   /* 加载完成再短亮一次LED */
	delay_ms(500);

	OLED_Clear();
	Show_String("INPUT PW:", 0, 28);    /* 主界面提示：输入密码 */

	/* 主循环：状态机永久运行 */
	while(1)
	{
		/* 根据当前状态分支处理 */
		switch(state)
		{
		/* ---------- 状态S_INPUT：输入开锁密码 ---------- */
		case S_INPUT:
			key = key_scan();
			/* 判断：无按键则跳出本次switch，等待下一轮扫描 */
			if(key == KEY_NONE) break;

			/* 判断：在主界面直接按数字8，进入管理员改密模式 */
			if(key == 8)
			{
				beep_key();
				OLED_Clear();
				state = S_ADMIN_OLD;     /* 切换到"输入旧密码"状态 */
				break;
			}

			/* 判断：按下数字键(0~9)，录入开锁密码 */
			if(key <= 9)
			{
				/* 判断：未满4位才接收 */
				if(idx < PWD_LEN)
				{
					beep_key();
					input[idx++] = key;      /* 存入输入缓冲区 */
					show_stars(1, idx);      /* 刷新星号 */
				}
			}
			/* 否则判断：删除键，回退一位 */
			else if(key == KEY_DELETE)
			{
				beep_key();
				if(idx > 0) { idx--; show_stars(1, idx); }
			}
			/* 否则判断：确认键，提交密码进行校验 */
			else if(key == KEY_CONFIRM)
			{
				/* 判断：必须输满4位才校验 */
				if(idx == PWD_LEN)
				{
					ok = 1;
					/* 循环：逐位比对输入与正确密码，有一位不同就标记为不匹配 */
					for(i=0;i<PWD_LEN;i++) if(input[i]!=pwd[i]) ok = 0;
					/* 判断：密码正确 */
					if(ok)
					{
						err_cnt = 0;                 /* 错误计数清零 */
						beep_success();              /* 成功双短声 */
						led_on();                    /* 点亮LED */
						servo_unlock();              /* 舵机转到开锁位 */
						OLED_Clear();
						Show_String("OPEN!", 0, 44); /* 显示开锁成功 */
						delay_ms(3000);              /* 保持开锁3秒 */
						servo_lock();                /* 舵机回到关锁位 */
						led_off();                   /* 熄灭LED */
						OLED_Clear();
						idx = 0;
						Show_String("INPUT PW:", 0, 28);  /* 回到主界面 */
					}
					/* 否则：密码错误 */
					else
					{
						err_cnt++;                   /* 错误计数加1 */
						beep_error();                /* 错误长鸣 */
						OLED_Clear();
						Show_String("PWD ERR!", 0, 32);
						delay_ms(1500);
						OLED_Clear();
						idx = 0;
						Show_String("INPUT PW:", 0, 28);
						/* 判断：错误次数达到上限，进入锁定状态 */
						if(err_cnt >= MAX_ERR) state = S_LOCK;
					}
				}
			}
			break;

		/* ---------- 状态S_LOCK：错误超限锁定倒计时 ---------- */
		case S_LOCK:
			OLED_Clear();
			/* 循环：从LOCK_SEC秒逐秒倒数到1 */
			for(sec = LOCK_SEC; sec > 0; sec--)
			{
				Show_String("LOCK ", 0, 24);
				Show_Char('0'+sec/10, 0, 72);    /* 显示秒的十位 */
				Show_Char('0'+sec%10, 0, 88);    /* 显示秒的个位 */
				delay_ms(1000);                  /* 每秒刷新一次 */
			}
			OLED_Clear();
			err_cnt = 0;                 /* 锁定结束清零错误计数 */
			idx = 0;
			Show_String("INPUT PW:", 0, 28);
			state = S_INPUT;             /* 回到输入密码状态 */
			break;

		/* ---------- 状态S_ADMIN_OLD：管理员输入旧密码 ---------- */
		case S_ADMIN_OLD:
			OLED_Clear();
			Show_String("OLD PW:", 0, 36);
			idx = 0;
			/* 循环：采集旧密码直到按确认 */
			while(1)
			{
				key = key_scan();
				/* 判断：无按键继续 */
				if(key == KEY_NONE) continue;
				/* 判断：数字键录入 */
				if(key <= 9)
				{
					if(idx < PWD_LEN) { beep_key(); input[idx++] = key; show_stars(1, idx); }
				}
				/* 否则判断：删除键 */
				else if(key == KEY_DELETE)
				{
					beep_key();
					if(idx > 0) { idx--; show_stars(1, idx); }
				}
				/* 否则判断：确认键，校验旧密码 */
				else if(key == KEY_CONFIRM)
				{
					beep_key();
					/* 判断：未满4位则继续录入 */
					if(idx != PWD_LEN) continue;
					ok = 1;
					/* 循环：逐位比对旧密码是否正确 */
					for(i=0;i<PWD_LEN;i++) if(input[i]!=pwd[i]) ok = 0;
					/* 判断：旧密码正确，进入输入新密码状态 */
					if(ok) { state = S_ADMIN_NEW1; }
					/* 否则：旧密码错误，报错并退回主界面 */
					else
					{
						beep_error();
						OLED_Clear();
						Show_String("OLD PW ERR!", 0, 20);
						delay_ms(1500);
						OLED_Clear();
						Show_String("INPUT PW:", 0, 28);
						state = S_INPUT;
					}
					break;
				}
			}
			break;

		/* ---------- 状态S_ADMIN_NEW1：管理员输入第一遍新密码 ---------- */
		case S_ADMIN_NEW1:
			OLED_Clear();
			Show_String("NEW PW:", 0, 36);
			idx = 0;
			/* 循环：采集第一遍新密码直到按确认 */
			while(1)
			{
				key = key_scan();
				/* 判断：无按键继续 */
				if(key == KEY_NONE) continue;
				/* 判断：数字键录入到new_pwd */
				if(key <= 9)
				{
					if(idx < PWD_LEN) { beep_key(); new_pwd[idx++] = key; show_stars(1, idx); }
				}
				/* 否则判断：删除键 */
				else if(key == KEY_DELETE)
				{
					beep_key();
					if(idx > 0) { idx--; show_stars(1, idx); }
				}
				/* 否则判断：确认键，满4位后进入确认新密码状态 */
				else if(key == KEY_CONFIRM)
				{
					beep_key();
					if(idx == PWD_LEN) state = S_ADMIN_NEW2;
					break;
				}
			}
			break;

		/* ---------- 状态S_ADMIN_NEW2：管理员确认第二遍新密码 ---------- */
		case S_ADMIN_NEW2:
			OLED_Clear();
			Show_String("CONFIRM:", 0, 32);
			idx = 0;
			/* 循环：采集第二遍新密码直到按确认 */
			while(1)
			{
				key = key_scan();
				/* 判断：无按键继续 */
				if(key == KEY_NONE) continue;
				/* 判断：数字键录入到input用于比对 */
				if(key <= 9)
				{
					if(idx < PWD_LEN) { beep_key(); input[idx++] = key; show_stars(1, idx); }
				}
				/* 否则判断：删除键 */
				else if(key == KEY_DELETE)
				{
					beep_key();
					if(idx > 0) { idx--; show_stars(1, idx); }
				}
				/* 否则判断：确认键，比对两遍新密码是否一致 */
				else if(key == KEY_CONFIRM)
				{
					beep_key();
					/* 判断：未满4位继续录入 */
					if(idx != PWD_LEN) continue;
					ok = 1;
					/* 循环：逐位比对第二遍输入与第一遍new_pwd */
					for(i=0;i<PWD_LEN;i++) if(input[i]!=new_pwd[i]) ok = 0;
					/* 判断：两遍一致，保存新密码 */
					if(ok)
					{
						pwd_save();                 /* 写入EEPROM并同步内存 */
						beep_success();
						OLED_Clear();
						Show_String("PWD OK!", 0, 36);
						delay_ms(1500);
					}
					/* 否则：两遍不一致，提示不匹配 */
					else
					{
						beep_error();
						OLED_Clear();
						Show_String("NOT MATCH!", 0, 24);
						delay_ms(1500);
					}
					OLED_Clear();
					idx = 0;
					Show_String("INPUT PW:", 0, 28);
					state = S_INPUT;               /* 无论成功失败都回到主界面 */
					break;
				}
			}
			break;

		/* ---------- 默认：异常状态复位到输入密码 ---------- */
		default:
			state = S_INPUT;
			break;
		}
	}
}
