#ifndef __SMART_FAN_PROTOCOL_H
#define __SMART_FAN_PROTOCOL_H

#include "stm32f10x.h"
#include "NRF24L01.h"

#define SMART_FAN_MAGIC 0x5A

typedef enum
{
    FAN_CMD_NONE = 0,
    FAN_CMD_START = 1,
    FAN_CMD_STOP = 2,
    FAN_CMD_SET_TIMER = 3,
    FAN_CMD_TIMER_START = 4,
    FAN_CMD_STATUS_REQ = 5,
    FAN_CMD_STATUS = 6,
    FAN_CMD_HEARTBEAT = 7,
    FAN_CMD_PAUSE = 8,
    FAN_CMD_RESUME = 9,
    FAN_CMD_SET_SPEED = 10
} FanCommand;

typedef enum
{
    FAN_STATE_OFF = 0,
    FAN_STATE_RUN = 1,
    FAN_STATE_TIMING = 2,
    FAN_STATE_SAFE_STOP = 3,
    FAN_STATE_PAUSED = 4
} FanState;

#define FAN_FLAG_LINK_OK 0x01
#define FAN_FLAG_SAFE    0x02
#define FAN_FLAG_TIMER   0x04

typedef struct
{
    uint8_t magic;
    uint8_t cmd;
    uint8_t seq;
    uint8_t state;
    uint8_t timer_set_s;
    uint8_t time_left_s;
    uint16_t voltage_mv;
    uint8_t flags;
    uint8_t speed;      /* 风速百分比 10~100 */
} FanPacket;

void FanPacket_Clear(FanPacket *packet);
void FanPacket_ToBytes(const FanPacket *packet, uint8_t *buffer);
uint8_t FanPacket_FromBytes(FanPacket *packet, const uint8_t *buffer);

#endif
