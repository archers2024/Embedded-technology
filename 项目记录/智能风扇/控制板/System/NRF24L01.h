#ifndef __NRF24L01_H
#define __NRF24L01_H

#include "stm32f10x.h"

#define NRF24_PAYLOAD_SIZE 16

typedef enum
{
    NRF24_TX_OK = 0,
    NRF24_TX_TIMEOUT,
    NRF24_RX_OK,
    NRF24_RX_EMPTY,
    NRF24_ERROR
} NRF24_Status;

void NRF24_Init(void);
uint8_t NRF24_Check(void);
void NRF24_SetTxAddress(const uint8_t *addr);
void NRF24_SetRxAddress(const uint8_t *addr);
void NRF24_SetModeRx(void);
void NRF24_SetModeTx(void);
NRF24_Status NRF24_SendPacket(const uint8_t *buffer);
NRF24_Status NRF24_ReceivePacket(uint8_t *buffer);

#endif
