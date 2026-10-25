#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "NRF24L01.h"
#include "SmartFanProtocol.h"
#include "Key.h"

#define LINK_TIMEOUT_MS 2500

static const uint8_t CORE_RX_ADDR[5] = {'F', 'A', 'N', '0', '1'};
static const uint8_t REMOTE_RX_ADDR[5] = {'R', 'E', 'M', '0', '1'};

/* 定时档位：10s / 30s / 60s 循环 */
static const uint8_t TIMER_PRESETS[3] = {10, 30, 60};
/* 风速三档：40% / 70% / 100% */
static const uint8_t SPEED_PRESETS[3] = {40, 70, 100};

static FanPacket g_status;
static uint8_t g_seq = 0;
static uint8_t g_timer_set_s = 10;
static uint8_t g_timer_idx = 0;      /* 当前定时档位 */
static uint8_t g_speed = 70;
static uint8_t g_speed_idx = 1;      /* 当前风速档位 */
static uint8_t g_link_ok = 0;
static uint8_t g_last_key = 0;     /* 最近按下的按键编号（OLED 调试显示） */
static uint16_t g_rx_count = 0;    /* 收到核心板状态包计数 */
static uint16_t g_tx_ok = 0;       /* 发送成功（收到 ACK）计数 */
static uint16_t g_tx_fail = 0;     /* 发送失败计数 */
static uint32_t g_last_status_ms = 0;
static uint32_t g_last_send_ms = 0;
static uint32_t g_last_oled_ms = 0;

/* 返回 1 表示核心板已确认收到（收到 ACK），0 表示未送达 */
static uint8_t SendCommand(uint8_t cmd)
{
    FanPacket packet;
    uint8_t buffer[NRF24_PAYLOAD_SIZE];
    NRF24_Status result;

    FanPacket_Clear(&packet);
    packet.cmd = cmd;
    packet.seq = g_seq++;
    packet.timer_set_s = g_timer_set_s;
    packet.speed = g_speed;

    FanPacket_ToBytes(&packet, buffer);
    NRF24_SetTxAddress(CORE_RX_ADDR);
    result = NRF24_SendPacket(buffer);
    NRF24_SetRxAddress(REMOTE_RX_ADDR);
    NRF24_SetModeRx();
    if (result == NRF24_TX_OK)
    {
        g_tx_ok++;
        return 1;
    }
    g_tx_fail++;
    return 0;
}

/* 重要命令重发 3 次，直到核心板 ACK 确认为止 */
static void SendCommandReliable(uint8_t cmd)
{
    uint8_t i;
    for (i = 0; i < 3; i++)
    {
        if (SendCommand(cmd))
        {
            break;
        }
        Delay_ms(30);
    }
}

/* 关键控制命令（启停/暂停/恢复）更强送达：5 次重发，间隔 50ms，
   总窗口约 250ms，更容易穿过电机电火花造成的瞬时丢包 */
static void SendCommandReliableStrong(uint8_t cmd)
{
    uint8_t i;
    for (i = 0; i < 5; i++)
    {
        if (SendCommand(cmd))
        {
            break;
        }
        Delay_ms(50);
    }
}

static void ReceiveStatusTask(void)
{
    uint8_t buffer[NRF24_PAYLOAD_SIZE];
    FanPacket packet;

    if (NRF24_ReceivePacket(buffer) == NRF24_RX_OK)
    {
        if (FanPacket_FromBytes(&packet, buffer) && packet.cmd == FAN_CMD_STATUS)
        {
            g_status = packet;
            g_timer_set_s = packet.timer_set_s;
            g_speed = packet.speed;
            g_last_status_ms = Millis();
            g_link_ok = 1;
            g_rx_count++;
        }
    }

    if ((Millis() - g_last_status_ms) > LINK_TIMEOUT_MS)
    {
        g_link_ok = 0;
    }
}

static void KeyTask(void)
{
    uint8_t key = Key_GetNum();

    if (key != KEY_NONE)
    {
        g_last_key = key;   /* 记录最近按键，OLED 第 2 行显示 Kx */
    }

    switch (key)
    {
        case KEY_START_PAUSE:   /* 按键1：非定时模式 开关（启动/停止） */
            if (g_status.state == FAN_STATE_RUN)
            {
                SendCommandReliableStrong(FAN_CMD_STOP);   /* 运行中 → 停止 */
            }
            else if (g_status.state == FAN_STATE_TIMING)
            {
                /* 定时过程中：按键1 不干预 */
            }
            else if (g_status.state == FAN_STATE_PAUSED)
            {
                SendCommandReliableStrong(FAN_CMD_STOP);   /* 历史暂停状态 → 清除回 OFF */
            }
            else if (!g_link_ok)
            {
                /* 断链状态未知：发停止（安全兜底；风扇没转时该命令无害） */
                SendCommandReliableStrong(FAN_CMD_STOP);
            }
            else
            {
                SendCommandReliableStrong(FAN_CMD_START);  /* 停止 → 无限启动 */
            }
            break;

        case KEY_TIME_SWITCH:   /* 按键2：60s/120s/300s 循环切换 */
            g_timer_idx = (g_timer_idx + 1) % 3;
            g_timer_set_s = TIMER_PRESETS[g_timer_idx];
            SendCommandReliable(FAN_CMD_SET_TIMER);
            break;

        case KEY_TIMER_START_PAUSE: /* 按键3：定时启动（定时过程中不可暂停、不可干预） */
            if (g_status.state == FAN_STATE_TIMING)
            {
                /* 定时运行中：不响应，保证定时过程不被干预 */
            }
            else if (g_status.state == FAN_STATE_RUN)
            {
                /* 非定时运行中：按键3 不干预 */
            }
            else if (g_link_ok)
            {
                SendCommandReliableStrong(FAN_CMD_TIMER_START); /* 停止 → 定时启动 */
            }
            break;

        case KEY_SPEED:         /* 按键4：三档风速切换 */
            g_speed_idx = (g_speed_idx + 1) % 3;
            g_speed = SPEED_PRESETS[g_speed_idx];
            SendCommandReliable(FAN_CMD_SET_SPEED);
            break;

        case KEY_TIME_PLUS_1S:  /* 按键5：定时 +1s */
            if (g_timer_set_s < 300)
            {
                g_timer_set_s++;
                SendCommandReliable(FAN_CMD_SET_TIMER);
            }
            break;

        case KEY_TIME_MINUS_1S: /* 按键6：定时 -1s */
            if (g_timer_set_s > 1)
            {
                g_timer_set_s--;
                SendCommandReliable(FAN_CMD_SET_TIMER);
            }
            break;

        default:
            break;
    }
}

static void HeartbeatTask(void)
{
    if ((Millis() - g_last_send_ms) >= 250)
    {
        g_last_send_ms = Millis();
        SendCommand(FAN_CMD_HEARTBEAT);
    }
}

static void ShowState(uint8_t line, uint8_t column, uint8_t state)
{
    switch (state)
    {
        case FAN_STATE_RUN:
            OLED_ShowString(line, column, "RUN ");
            break;
        case FAN_STATE_TIMING:
            OLED_ShowString(line, column, "TIME");
            break;
        case FAN_STATE_SAFE_STOP:
            OLED_ShowString(line, column, "SAFE");
            break;
        case FAN_STATE_PAUSED:
            OLED_ShowString(line, column, "PAUS");
            break;
        case FAN_STATE_OFF:
        default:
            OLED_ShowString(line, column, "OFF ");
            break;
    }
}

static const char *SpeedName(uint8_t speed)
{
    if (speed <= 40)
    {
        return "LOW";
    }
    if (speed <= 70)
    {
        return "MID";
    }
    return "HIG";
}

static void OLEDTask(void)
{
    if ((Millis() - g_last_oled_ms) < 200)
    {
        return;
    }
    g_last_oled_ms = Millis();

    /* 第 1 行：状态 + 风速 */
    OLED_ShowString(1, 1, "FAN:");
    if (g_link_ok)
    {
        ShowState(1, 5, g_status.state);
    }
    else
    {
        OLED_ShowString(1, 5, "LOST");
    }
    OLED_ShowString(1, 10, "SPD:");
    OLED_ShowString(1, 14, SpeedName(g_speed));

    /* 第 2 行：定时档位 + 最近按键（调试） */
    OLED_ShowString(2, 1, "T:");
    OLED_ShowNum(2, 3, g_timer_set_s, 3);
    OLED_ShowChar(2, 6, 'S');
    OLED_ShowString(2, 7, "       ");
    if (g_last_key != KEY_NONE)
    {
        OLED_ShowChar(2, 14, 'K');
        OLED_ShowNum(2, 15, g_last_key, 1);
    }
    else
    {
        OLED_ShowString(2, 14, "  ");
    }

    /* 第 3 行：剩余时间 + 电压 */
    OLED_ShowString(3, 1, "LEFT:");
    if (g_status.state == FAN_STATE_TIMING || g_status.state == FAN_STATE_PAUSED)
    {
        OLED_ShowNum(3, 6, g_status.time_left_s, 3);
        OLED_ShowChar(3, 9, 'S');
    }
    else
    {
        OLED_ShowString(3, 6, "---S");
    }
    OLED_ShowString(3, 10, "V:");
    if (g_link_ok)
    {
        OLED_ShowNum(3, 12, g_status.voltage_mv / 1000, 1);
        OLED_ShowChar(3, 13, '.');
        OLED_ShowNum(3, 14, (g_status.voltage_mv % 1000) / 100, 1);
        OLED_ShowChar(3, 15, 'V');
    }
    else
    {
        OLED_ShowString(3, 12, "--.-V");
    }

    /* 第 4 行：链路状态 + 通信计数（左=发送成功数，右=收到状态包数） */
    OLED_ShowString(4, 1, "LINK:");
    OLED_ShowString(4, 6, g_link_ok ? "OK   " : "FAIL ");
    OLED_ShowNum(4, 11, g_tx_ok % 100, 2);
    OLED_ShowChar(4, 13, '/');
    OLED_ShowNum(4, 14, g_rx_count % 100, 2);
}

int main(void)
{
    Delay_Init();
    Key_Init();
    OLED_Init();
    NRF24_Init();

    FanPacket_Clear(&g_status);
    NRF24_SetRxAddress(REMOTE_RX_ADDR);
    NRF24_SetModeRx();

    OLED_Clear();
    OLED_ShowString(1, 1, "SMART FAN");
    OLED_ShowString(2, 1, "NRF24 INIT...");
    Delay_ms(500);
    OLED_Clear();

    /* 自检：SPI 读写失败说明 NRF24 接线/供电有问题 */
    if (!NRF24_Check())
    {
        OLED_ShowString(2, 1, "NRF ERR!");
        while (1)
        {
        }
    }

    while (1)
    {
        ReceiveStatusTask();
        KeyTask();
        HeartbeatTask();
        OLEDTask();
    }
}
