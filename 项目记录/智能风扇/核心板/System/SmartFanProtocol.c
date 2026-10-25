#include "SmartFanProtocol.h"

static uint8_t FanPacket_Checksum(const uint8_t *buffer)
{
    uint8_t i;
    uint8_t sum = 0;
    for (i = 0; i < NRF24_PAYLOAD_SIZE - 1; i++)
    {
        sum += buffer[i];
    }
    return sum;
}

void FanPacket_Clear(FanPacket *packet)
{
    packet->magic = SMART_FAN_MAGIC;
    packet->cmd = FAN_CMD_NONE;
    packet->seq = 0;
    packet->state = FAN_STATE_OFF;
    packet->timer_set_s = 10;
    packet->time_left_s = 0;
    packet->voltage_mv = 0;
    packet->flags = 0;
    packet->speed = 0;
}

void FanPacket_ToBytes(const FanPacket *packet, uint8_t *buffer)
{
    uint8_t i;
    for (i = 0; i < NRF24_PAYLOAD_SIZE; i++)
    {
        buffer[i] = 0;
    }

    buffer[0] = SMART_FAN_MAGIC;
    buffer[1] = packet->cmd;
    buffer[2] = packet->seq;
    buffer[3] = packet->state;
    buffer[4] = packet->timer_set_s;
    buffer[5] = packet->time_left_s;
    buffer[6] = (uint8_t)(packet->voltage_mv >> 8);
    buffer[7] = (uint8_t)(packet->voltage_mv & 0xFF);
    buffer[8] = packet->flags;
    buffer[9] = packet->speed;
    buffer[NRF24_PAYLOAD_SIZE - 1] = FanPacket_Checksum(buffer);
}

uint8_t FanPacket_FromBytes(FanPacket *packet, const uint8_t *buffer)
{
    if (buffer[0] != SMART_FAN_MAGIC)
    {
        return 0;
    }
    if (buffer[NRF24_PAYLOAD_SIZE - 1] != FanPacket_Checksum(buffer))
    {
        return 0;
    }

    packet->magic = buffer[0];
    packet->cmd = buffer[1];
    packet->seq = buffer[2];
    packet->state = buffer[3];
    packet->timer_set_s = buffer[4];
    packet->time_left_s = buffer[5];
    packet->voltage_mv = ((uint16_t)buffer[6] << 8) | buffer[7];
    packet->flags = buffer[8];
    packet->speed = buffer[9];
    return 1;
}
