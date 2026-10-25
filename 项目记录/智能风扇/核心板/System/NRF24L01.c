#include "NRF24L01.h"
#include "Delay.h"

/* ---------------- 引脚定义（软件 SPI） ---------------- */
#define NRF24_SCK_PORT   GPIOA
#define NRF24_SCK_PIN    GPIO_Pin_8
#define NRF24_MISO_PORT  GPIOA
#define NRF24_MISO_PIN   GPIO_Pin_9
#define NRF24_MOSI_PORT  GPIOA
#define NRF24_MOSI_PIN   GPIO_Pin_10
#define NRF24_CE_PORT    GPIOA
#define NRF24_CE_PIN     GPIO_Pin_11
#define NRF24_CSN_PORT   GPIOA
#define NRF24_CSN_PIN    GPIO_Pin_12

#define NRF24_READ_REG    0x00
#define NRF24_WRITE_REG   0x20
#define NRF24_RD_RX_PLOAD 0x61
#define NRF24_WR_TX_PLOAD 0xA0
#define NRF24_FLUSH_TX    0xE1
#define NRF24_FLUSH_RX    0xE2
#define NRF24_NOP         0xFF

#define REG_CONFIG      0x00
#define REG_EN_AA       0x01
#define REG_EN_RXADDR   0x02
#define REG_SETUP_AW    0x03
#define REG_SETUP_RETR  0x04
#define REG_RF_CH       0x05
#define REG_RF_SETUP    0x06
#define REG_STATUS      0x07
#define REG_RX_ADDR_P0  0x0A
#define REG_TX_ADDR     0x10
#define REG_RX_PW_P0    0x11
#define REG_FIFO_STATUS 0x17

#define MASK_RX_DR 0x40
#define MASK_TX_DS 0x20
#define MASK_MAX_RT 0x10

/* ---------------- 底层 GPIO ---------------- */
static void NRF24_SCK(uint8_t bit)
{
    GPIO_WriteBit(NRF24_SCK_PORT, NRF24_SCK_PIN, bit ? Bit_SET : Bit_RESET);
}

static void NRF24_MOSI(uint8_t bit)
{
    GPIO_WriteBit(NRF24_MOSI_PORT, NRF24_MOSI_PIN, bit ? Bit_SET : Bit_RESET);
}

static uint8_t NRF24_MISO(void)
{
    return GPIO_ReadInputDataBit(NRF24_MISO_PORT, NRF24_MISO_PIN);
}

static void NRF24_CE(uint8_t bit)
{
    GPIO_WriteBit(NRF24_CE_PORT, NRF24_CE_PIN, bit ? Bit_SET : Bit_RESET);
}

static void NRF24_CSN(uint8_t bit)
{
    GPIO_WriteBit(NRF24_CSN_PORT, NRF24_CSN_PIN, bit ? Bit_SET : Bit_RESET);
}

/* ---------------- 软件 SPI（模式 0：CPOL=0, CPHA=0） ---------------- */
static uint8_t NRF24_SPI_SwapByte(uint8_t byte)
{
    uint8_t i;
    uint8_t rx = 0;

    for (i = 0; i < 8; i++)
    {
        NRF24_MOSI((byte & 0x80) ? 1 : 0);
        byte <<= 1;

        NRF24_SCK(1);
        Delay_us(1);

        rx <<= 1;
        if (NRF24_MISO())
        {
            rx |= 0x01;
        }

        NRF24_SCK(0);
        Delay_us(1);
    }
    return rx;
}

/* ---------------- 寄存器读写 ---------------- */
static uint8_t NRF24_ReadReg(uint8_t reg)
{
    uint8_t value;
    NRF24_CSN(0);
    NRF24_SPI_SwapByte(NRF24_READ_REG | reg);
    value = NRF24_SPI_SwapByte(NRF24_NOP);
    NRF24_CSN(1);
    return value;
}

static void NRF24_WriteReg(uint8_t reg, uint8_t value)
{
    NRF24_CSN(0);
    NRF24_SPI_SwapByte(NRF24_WRITE_REG | reg);
    NRF24_SPI_SwapByte(value);
    NRF24_CSN(1);
}

static void NRF24_ReadBuf(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t i;
    NRF24_CSN(0);
    NRF24_SPI_SwapByte(reg);
    for (i = 0; i < length; i++)
    {
        buffer[i] = NRF24_SPI_SwapByte(NRF24_NOP);
    }
    NRF24_CSN(1);
}

static void NRF24_WriteBuf(uint8_t reg, const uint8_t *buffer, uint8_t length)
{
    uint8_t i;
    NRF24_CSN(0);
    NRF24_SPI_SwapByte(reg);
    for (i = 0; i < length; i++)
    {
        NRF24_SPI_SwapByte(buffer[i]);
    }
    NRF24_CSN(1);
}

static void NRF24_FlushTx(void)
{
    NRF24_CSN(0);
    NRF24_SPI_SwapByte(NRF24_FLUSH_TX);
    NRF24_CSN(1);
}

static void NRF24_FlushRx(void)
{
    NRF24_CSN(0);
    NRF24_SPI_SwapByte(NRF24_FLUSH_RX);
    NRF24_CSN(1);
}

/* ---------------- 对外接口 ---------------- */
void NRF24_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* SCK / MOSI / CE / CSN 推挽输出 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = NRF24_SCK_PIN | NRF24_MOSI_PIN |
                                  NRF24_CE_PIN  | NRF24_CSN_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* MISO 上拉输入 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = NRF24_MISO_PIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    NRF24_CE(0);
    NRF24_CSN(1);
    NRF24_SCK(0);

    Delay_ms(100);

    NRF24_WriteReg(REG_CONFIG, 0x0E);
    NRF24_WriteReg(REG_EN_AA, 0x01);
    NRF24_WriteReg(REG_EN_RXADDR, 0x01);
    NRF24_WriteReg(REG_SETUP_AW, 0x03);
    NRF24_WriteReg(REG_SETUP_RETR, 0x1F);   /* 建议：5 次重传，1500us */
    NRF24_WriteReg(REG_RF_CH, 115);       /* 2475MHz，避开 Wi-Fi 常用信道 */
    NRF24_WriteReg(REG_RF_SETUP, 0x26);   /* 250kbps，抗干扰更强（两端必须一致） */
    NRF24_WriteReg(REG_RX_PW_P0, NRF24_PAYLOAD_SIZE);
    NRF24_WriteReg(REG_STATUS, MASK_RX_DR | MASK_TX_DS | MASK_MAX_RT);
    NRF24_FlushTx();
    NRF24_FlushRx();
}

uint8_t NRF24_Check(void)
{
    uint8_t test_addr[5] = {0xA5, 0xA5, 0xA5, 0xA5, 0xA5};
    uint8_t read_addr[5] = {0};
    uint8_t i;

    NRF24_WriteBuf(NRF24_WRITE_REG | REG_TX_ADDR, test_addr, 5);
    NRF24_ReadBuf(NRF24_READ_REG | REG_TX_ADDR, read_addr, 5);
    for (i = 0; i < 5; i++)
    {
        if (read_addr[i] != test_addr[i])
        {
            return 0;
        }
    }
    return 1;
}

void NRF24_SetTxAddress(const uint8_t *addr)
{
    NRF24_WriteBuf(NRF24_WRITE_REG | REG_TX_ADDR, addr, 5);
    NRF24_WriteBuf(NRF24_WRITE_REG | REG_RX_ADDR_P0, addr, 5);
}

void NRF24_SetRxAddress(const uint8_t *addr)
{
    NRF24_WriteBuf(NRF24_WRITE_REG | REG_RX_ADDR_P0, addr, 5);
}

void NRF24_SetModeRx(void)
{
    NRF24_CE(0);
    NRF24_WriteReg(REG_CONFIG, 0x0F);
    NRF24_WriteReg(REG_STATUS, MASK_RX_DR | MASK_TX_DS | MASK_MAX_RT);
    NRF24_FlushRx();
    NRF24_CE(1);
    Delay_us(150);
}

void NRF24_SetModeTx(void)
{
    NRF24_CE(0);
    NRF24_WriteReg(REG_CONFIG, 0x0E);
    NRF24_WriteReg(REG_STATUS, MASK_RX_DR | MASK_TX_DS | MASK_MAX_RT);
    NRF24_FlushTx();
    Delay_us(150);
}

NRF24_Status NRF24_SendPacket(const uint8_t *buffer)
{
    uint8_t status;
    uint32_t start;

    NRF24_SetModeTx();
    NRF24_WriteBuf(NRF24_WR_TX_PLOAD, buffer, NRF24_PAYLOAD_SIZE);
    NRF24_CE(1);
    Delay_us(20);
    NRF24_CE(0);

    start = Millis();
    while ((Millis() - start) < 30)
    {
        status = NRF24_ReadReg(REG_STATUS);
        if (status & MASK_TX_DS)
        {
            NRF24_WriteReg(REG_STATUS, MASK_TX_DS);
            NRF24_SetModeRx();
            return NRF24_TX_OK;
        }
        if (status & MASK_MAX_RT)
        {
            NRF24_WriteReg(REG_STATUS, MASK_MAX_RT);
            NRF24_FlushTx();
            NRF24_SetModeRx();
            return NRF24_TX_TIMEOUT;
        }
    }

    NRF24_SetModeRx();
    return NRF24_TX_TIMEOUT;
}

NRF24_Status NRF24_ReceivePacket(uint8_t *buffer)
{
    uint8_t status = NRF24_ReadReg(REG_STATUS);

    if (status & MASK_RX_DR)
    {
        NRF24_ReadBuf(NRF24_RD_RX_PLOAD, buffer, NRF24_PAYLOAD_SIZE);
        NRF24_WriteReg(REG_STATUS, MASK_RX_DR);
        return NRF24_RX_OK;
    }

    return NRF24_RX_EMPTY;
}
