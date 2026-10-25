#include "OLED.h"
#include "OLED_Font.h"
#include "Delay.h"

#define OLED_SCL_PORT GPIOB
#define OLED_SCL_PIN  GPIO_Pin_8
#define OLED_SDA_PORT GPIOB
#define OLED_SDA_PIN  GPIO_Pin_9

static void OLED_W_SCL(uint8_t bit)
{
    GPIO_WriteBit(OLED_SCL_PORT, OLED_SCL_PIN, bit ? Bit_SET : Bit_RESET);
}

static void OLED_W_SDA(uint8_t bit)
{
    GPIO_WriteBit(OLED_SDA_PORT, OLED_SDA_PIN, bit ? Bit_SET : Bit_RESET);
}

static void OLED_I2C_Start(void)
{
    OLED_W_SDA(1);
    OLED_W_SCL(1);
    OLED_W_SDA(0);
    OLED_W_SCL(0);
}

static void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0);
    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

static void OLED_I2C_SendByte(uint8_t byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        OLED_W_SDA(byte & (0x80 >> i));
        OLED_W_SCL(1);
        OLED_W_SCL(0);
    }
    OLED_W_SCL(1);
    OLED_W_SCL(0);
}

static void OLED_WriteCommand(uint8_t command)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);
    OLED_I2C_SendByte(0x00);
    OLED_I2C_SendByte(command);
    OLED_I2C_Stop();
}

static void OLED_WriteData(uint8_t data)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);
    OLED_I2C_SendByte(0x40);
    OLED_I2C_SendByte(data);
    OLED_I2C_Stop();
}

static void OLED_SetCursor(uint8_t y, uint8_t x)
{
    OLED_WriteCommand(0xB0 | y);
    OLED_WriteCommand(0x10 | ((x & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (x & 0x0F));
}

static uint32_t OLED_Pow(uint32_t x, uint32_t y)
{
    uint32_t result = 1;
    while (y--)
    {
        result *= x;
    }
    return result;
}

void OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    Delay_ms(100);

    OLED_WriteCommand(0xAE);
    OLED_WriteCommand(0xD5);
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8);
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3);
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40);
    OLED_WriteCommand(0xA1);
    OLED_WriteCommand(0xC8);
    OLED_WriteCommand(0xDA);
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81);
    OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9);
    OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB);
    OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4);
    OLED_WriteCommand(0xA6);
    OLED_WriteCommand(0x8D);
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);

    OLED_Clear();
}

void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        for (i = 0; i < 128; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_ShowChar(uint8_t line, uint8_t column, char ch)
{
    uint8_t i;
    uint8_t index;

    if (line < 1 || line > 4 || column < 1 || column > 16)
    {
        return;
    }
    if (ch < ' ' || ch > 'Z')
    {
        ch = '?';
    }

    index = (uint8_t)(ch - ' ');
    OLED_SetCursor((line - 1) * 2, (column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[index][i]);
    }
    OLED_SetCursor((line - 1) * 2 + 1, (column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[index][i + 8]);
    }
}

void OLED_ShowString(uint8_t line, uint8_t column, const char *string)
{
    uint8_t i = 0;
    while (string[i] != '\0' && (column + i) <= 16)
    {
        OLED_ShowChar(line, column + i, string[i]);
        i++;
    }
}

void OLED_ShowNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    uint8_t i;
    for (i = 0; i < length; i++)
    {
        OLED_ShowChar(line, column + i, (char)(number / OLED_Pow(10, length - i - 1) % 10 + '0'));
    }
}

void OLED_ShowSignedNum(uint8_t line, uint8_t column, int32_t number, uint8_t length)
{
    if (number >= 0)
    {
        OLED_ShowChar(line, column, '+');
        OLED_ShowNum(line, column + 1, (uint32_t)number, length);
    }
    else
    {
        OLED_ShowChar(line, column, '-');
        OLED_ShowNum(line, column + 1, (uint32_t)(-number), length);
    }
}

void OLED_ShowHexNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    uint8_t i, single;
    for (i = 0; i < length; i++)
    {
        single = number / OLED_Pow(16, length - i - 1) % 16;
        OLED_ShowChar(line, column + i, single < 10 ? single + '0' : single - 10 + 'A');
    }
}

void OLED_ShowVoltage(uint8_t line, uint8_t column, uint16_t mv)
{
    OLED_ShowNum(line, column, mv / 1000, 1);
    OLED_ShowChar(line, column + 1, '.');
    OLED_ShowNum(line, column + 2, (mv % 1000) / 10, 2);
    OLED_ShowChar(line, column + 4, 'V');
}
