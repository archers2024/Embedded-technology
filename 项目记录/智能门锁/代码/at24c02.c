#include "at24c02.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_i2c.h"
#include "delay.h"

/* ==================== AT24C02 EEPROM 器件地址定义 ==================== */
/* AT24C02 的7位器件地址为 1010 A2 A1 A0，硬件接地时为 0101000；
 * 左移1位后：写命令 0xA0（最低位R/W=0），读命令 0xA1（最低位R/W=1） */
#define AT24C02_ADDR_W   0xA0      /* 器件写地址（R/W位=0） */
#define AT24C02_ADDR_R   0xA1      /* 器件读地址（R/W位=1） */

/*
 * 函数：at24c02_init
 * 功能：AT24C02 初始化
 * 说明：本工程使用硬件I2C1，引脚时钟在其它模块已配置，此处为空实现，保留接口
 */
void at24c02_init(void){}

/*
 * 函数：wait_flag（内部静态函数，仅本文件使用）
 * 功能：带超时地等待 I2C 指定标志位置位，避免总线异常时死等
 * 参数：flag —— 要等待的I2C标志（SB起始/ADDR地址/TXE发送空/BTF传完/RXNE接收满等）
 * 返回：0=等待成功（标志已置位）；1=超时失败（计数耗尽仍未置位）
 */
static uint8_t wait_flag(uint32_t flag)
{
	uint32_t to=10000;          /* 超时计数上限，作为软件防死锁的退避余量 */
	/* 循环：只要目标标志仍为RESET就继续等待，每轮把超时计数减1 */
	while(I2C_GetFlagStatus(I2C1,flag)==RESET) if(--to==0) return 1; /* 计数减到0即超时，立即返回失败1 */
	return 0;                   /* 标志成功置位，返回0表示正常 */
}

/*
 * 函数：at24c02_write
 * 功能：向 AT24C02 指定存储单元写入一个字节
 * 参数：addr —— EEPROM单元地址(0~255)；data —— 待写入的1字节数据
 * 流程：起始 -> 发写地址 -> 发单元地址 -> 发数据 -> 停止，并等待内部烧写完成
 */
void at24c02_write(uint8_t addr, uint8_t data)
{
	/* 等待I2C总线空闲；此处判断后无额外处理，仅作为总线忙检测占位 */
	if(wait_flag(I2C_FLAG_BUSY)==0){}
	I2C_GenerateSTART(I2C1, ENABLE);                 /* 第1步：发送I2C起始信号 */
	if(wait_flag(I2C_FLAG_SB)) return;                /* 等待起始标志SB，超时则直接返回 */
	I2C_SendData(I2C1, AT24C02_ADDR_W);               /* 第2步：发送器件写地址 */
	if(wait_flag(I2C_FLAG_ADDR)) { I2C_GenerateSTOP(I2C1,ENABLE); return; } /* 等待地址应答失败：发停止信号并返回 */
	(void)I2C1->SR1; (void)I2C1->SR2;                /* 读SR1、SR2以硬件清除ADDR地址标志（I2C规范要求） */
	I2C_SendData(I2C1, addr);                         /* 第3步：发送要写入的存储单元地址 */
	if(wait_flag(I2C_FLAG_TXE)) { I2C_GenerateSTOP(I2C1,ENABLE); return; }  /* 等待发送缓冲TXE失败：发停止并返回 */
	I2C_SendData(I2C1, data);                        /* 第4步：发送待写入的数据字节 */
	if(wait_flag(I2C_FLAG_BTF)) { I2C_GenerateSTOP(I2C1,ENABLE); return; } /* 等待字节传输完成BTF失败：发停止并返回 */
	I2C_GenerateSTOP(I2C1, ENABLE);                  /* 发送停止信号，结束本次写操作 */
	delay_ms(5);                                      /* 等待AT24C02内部页烧写周期(典型≤5ms)完成，防止连续写出错 */
}

/*
 * 函数：at24c02_read
 * 功能：从 AT24C02 指定存储单元读出一个字节
 * 参数：addr —— 要读取的EEPROM单元地址
 * 返回：读到的字节值；任何通信失败都返回 0xFF
 * 流程：先写方式设置存储指针，再重复起始切换为读方向，接收1字节后回NACK并停止
 */
uint8_t at24c02_read(uint8_t addr)
{
	uint8_t val=0;                                   /* 变量：保存最终读到的字节 */
	I2C_GenerateSTART(I2C1, ENABLE);                 /* 第一次起始：用于写入存储指针地址 */
	if(wait_flag(I2C_FLAG_SB)) return 0xFF;          /* 等待起始标志失败：返回0xFF */
	I2C_SendData(I2C1, AT24C02_ADDR_W);              /* 发器件写地址（先定位到目标单元） */
	if(wait_flag(I2C_FLAG_ADDR)) { I2C_GenerateSTOP(I2C1,ENABLE); return 0xFF; }
	(void)I2C1->SR1; (void)I2C1->SR2;                /* 读SR1、SR2清除ADDR地址标志 */
	I2C_SendData(I2C1, addr);                        /* 发送目标存储单元地址，设置读指针 */
	if(wait_flag(I2C_FLAG_TXE)) { I2C_GenerateSTOP(I2C1,ENABLE); return 0xFF; }

	I2C_GenerateSTART(I2C1, ENABLE);                 /* 重复起始：不释放总线，切换为读方向 */
	if(wait_flag(I2C_FLAG_SB)) { I2C_GenerateSTOP(I2C1,ENABLE); return 0xFF; }
	I2C_SendData(I2C1, AT24C02_ADDR_R);              /* 发器件读地址（R/W位=1） */
	if(wait_flag(I2C_FLAG_ADDR)) { I2C_GenerateSTOP(I2C1,ENABLE); return 0xFF; }
	(void)I2C1->SR1; (void)I2C1->SR2;                /* 读SR1、SR2清除ADDR地址标志 */
	I2C1->CR1 &= ~(1<<10);                           /* 清零ACK位(bit10)：只收1字节后回NACK，告知从机结束 */
	I2C_GenerateSTOP(I2C1, ENABLE);                  /* 发出停止条件 */
	if(wait_flag(I2C_FLAG_RXNE)) { I2C1->CR1 |= (1<<10); return 0xFF; } /* 等待接收满RXNE失败：恢复ACK并返回0xFF */
	val = I2C_ReceiveData(I2C1);                     /* 读取从机发来的字节到val */
	I2C1->CR1 |= (1<<10);                            /* 重新置位ACK位，恢复应答，供下次通信使用 */
	return val;                                      /* 返回读到的字节 */
}
