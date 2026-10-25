#include "stm32f10x.h"
#include "Delay.h"
#include "NRF24L01.h"
#include "SmartFanProtocol.h"
#include "Motor.h"
#include "Voltage.h"
#include "Safety.h"
#include "Alarm.h"

#define FAN_SPEED_DEFAULT 70
#define COMM_TIMEOUT_MS 30000
#define SAFE_TIMEOUT_MS 15000

/* 调试开关：置 1 上电后电机自转 500ms，用于验证电机回路（验证完务必改回 0） */
#define MOTOR_SELF_TEST 1

static const uint8_t CORE_RX_ADDR[5] = {'F', 'A', 'N', '0', '1'};
static const uint8_t REMOTE_RX_ADDR[5] = {'R', 'E', 'M', '0', '1'};

static FanState g_state = FAN_STATE_OFF;
static FanState g_pause_prev = FAN_STATE_RUN;  /* 暂停前状态，恢复时还原 */
static uint8_t g_timer_set_s = 10;
static uint8_t g_time_left_s = 0;
static uint16_t g_voltage_mv = 0;
static uint8_t g_speed = FAN_SPEED_DEFAULT;
static uint8_t g_seq = 0;
static uint32_t g_last_status_ms = 0;
static uint32_t g_last_adc_ms = 0;
static uint32_t g_last_second_ms = 0;
static uint32_t g_last_rx_ms = 0;
static uint32_t g_safe_enter_ms = 0;

static void Fan_Stop(FanState next_state)
{
    Motor_Stop();
    Alarm_RunLed(0);
    g_time_left_s = 0;
    g_state = next_state;
    if (next_state == FAN_STATE_SAFE_STOP)
    {
        g_safe_enter_ms = Millis();
    }
}

static void Fan_Start(void)
{
    if (Safety_IsBlocked())
    {
        Fan_Stop(FAN_STATE_SAFE_STOP);
        Alarm_Buzzer(1);
        return;
    }

    Alarm_Buzzer(0);
    Alarm_SafeLed(0);
    Motor_Start(g_speed);
    Alarm_RunLed(1);
    g_state = FAN_STATE_RUN;
    g_time_left_s = 0;
}

static void Fan_StartTimer(void)
{
    if (Safety_IsBlocked())
    {
        Fan_Stop(FAN_STATE_SAFE_STOP);
        Alarm_Buzzer(1);
        return;
    }

    Alarm_Buzzer(0);
    Alarm_SafeLed(0);
    Motor_Start(g_speed);
    Alarm_RunLed(1);
    g_state = FAN_STATE_TIMING;
    g_time_left_s = g_timer_set_s;
    g_last_second_ms = Millis();
}

static void SendStatus(void)
{
    FanPacket packet;
    uint8_t buffer[NRF24_PAYLOAD_SIZE];

    FanPacket_Clear(&packet);
    packet.cmd = FAN_CMD_STATUS;
    packet.seq = g_seq++;
    packet.state = (uint8_t)g_state;
    packet.timer_set_s = g_timer_set_s;
    packet.time_left_s = g_time_left_s;
    packet.voltage_mv = g_voltage_mv;
    packet.speed = g_speed;
    packet.flags = 0;
    if ((Millis() - g_last_rx_ms) < COMM_TIMEOUT_MS)
    {
        packet.flags |= FAN_FLAG_LINK_OK;
    }
    if (g_state == FAN_STATE_SAFE_STOP)
    {
        packet.flags |= FAN_FLAG_SAFE;
    }
    if (g_state == FAN_STATE_TIMING)
    {
        packet.flags |= FAN_FLAG_TIMER;
    }

    FanPacket_ToBytes(&packet, buffer);
    NRF24_SetTxAddress(REMOTE_RX_ADDR);
    NRF24_SendPacket(buffer);
    NRF24_SetRxAddress(CORE_RX_ADDR);
    NRF24_SetModeRx();
}

static void HandleCommand(const FanPacket *packet)
{
    g_last_rx_ms = Millis();

    if (packet->timer_set_s >= 1 && packet->timer_set_s <= 300)
    {
        g_timer_set_s = packet->timer_set_s;
    }

    switch (packet->cmd)
    {
        case FAN_CMD_START:
            if (g_state == FAN_STATE_SAFE_STOP && Safety_IsBlocked())
            {
                break;
            }
            Fan_Start();
            break;

        case FAN_CMD_STOP:
            Fan_Stop(FAN_STATE_OFF);
            Alarm_Buzzer(0);
            Alarm_SafeLed(0);
            break;

        case FAN_CMD_PAUSE:
            if (g_state == FAN_STATE_RUN || g_state == FAN_STATE_TIMING)
            {
                g_pause_prev = g_state;
                Motor_Stop();
                Alarm_RunLed(0);
                g_state = FAN_STATE_PAUSED;
                /* g_time_left_s 保留，恢复后继续倒计时 */
            }
            break;

        case FAN_CMD_RESUME:
            if (g_state == FAN_STATE_PAUSED)
            {
                if (Safety_IsBlocked())
                {
                    Fan_Stop(FAN_STATE_SAFE_STOP);
                    Alarm_Buzzer(1);
                    break;
                }
                Motor_Start(g_speed);
                Alarm_RunLed(1);
                g_state = g_pause_prev;
                if (g_state == FAN_STATE_TIMING)
                {
                    g_last_second_ms = Millis();
                }
            }
            break;

        case FAN_CMD_SET_SPEED:
            if (packet->speed >= 10 && packet->speed <= 100)
            {
                g_speed = packet->speed;
            }
            if (g_state == FAN_STATE_RUN || g_state == FAN_STATE_TIMING)
            {
                Motor_SetSpeed(g_speed);
            }
            break;

        case FAN_CMD_SET_TIMER:
            break;

        case FAN_CMD_TIMER_START:
            if (packet->timer_set_s >= 1 && packet->timer_set_s <= 300)
            {
                g_timer_set_s = packet->timer_set_s;
            }
            Fan_StartTimer();
            break;

        case FAN_CMD_STATUS_REQ:
        case FAN_CMD_HEARTBEAT:
            break;

        default:
            break;
    }
}

static void ReceiveTask(void)
{
    uint8_t buffer[NRF24_PAYLOAD_SIZE];
    FanPacket packet;

    if (NRF24_ReceivePacket(buffer) == NRF24_RX_OK)
    {
        if (FanPacket_FromBytes(&packet, buffer))
        {
            HandleCommand(&packet);
        }
    }
}

static void SafetyTask(void)
{
    if (Safety_IsBlocked() && (g_state == FAN_STATE_RUN || g_state == FAN_STATE_TIMING))
    {
        Fan_Stop(FAN_STATE_SAFE_STOP);
        Alarm_Buzzer(1);
    }

    if (g_state == FAN_STATE_SAFE_STOP)
    {
        /* 报警 15 秒后自动解除：停报警、停闪烁，回到初始状态等待指令 */
        if ((Millis() - g_safe_enter_ms) >= SAFE_TIMEOUT_MS)
        {
            Fan_Stop(FAN_STATE_OFF);
            Alarm_Buzzer(0);
            Alarm_SafeLed(0);
        }
        else
        {
            Alarm_Buzzer(1);
            Alarm_RunLed(0);
        }
    }
}

static void TimerTask(void)
{
    if (g_state != FAN_STATE_TIMING)
    {
        return;
    }

    if ((Millis() - g_last_second_ms) >= 1000)
    {
        g_last_second_ms = Millis();
        if (g_time_left_s > 0)
        {
            g_time_left_s--;
        }
        if (g_time_left_s == 0)
        {
            Fan_Stop(FAN_STATE_OFF);
        }
    }
}

static void CommWatchdogTask(void)
{
    /* 只有"无限转"模式看门狗：断链 30s 强制停机，防止遥控器丢失后风扇一直转。
       定时模式免看门狗——它到点会自己停，电机干扰导致短暂断链也不会误停。 */
    if (g_state == FAN_STATE_RUN &&
        (Millis() - g_last_rx_ms) > COMM_TIMEOUT_MS)
    {
        Fan_Stop(FAN_STATE_OFF);
    }
}

int main(void)
{
    Delay_Init();
    Motor_Init();

#if MOTOR_SELF_TEST
    /* 上电自检：直接启动电机 500ms，不经过 Safety/无线，验证 TB6612+电机硬件 */
    Motor_Start(70);
    Delay_ms(500);
    Motor_Stop();
#endif

    Voltage_Init();
    Safety_Init();
    Alarm_Init();
    NRF24_Init();

    /* 自检：SPI 读写失败说明 NRF24 接线/供电有问题，板载 PC13 灯快闪提示
       （不用 PB0，保证 PB0 报警灯只在 Safety 触发时才亮） */
    if (!NRF24_Check())
    {
        while (1)
        {
            Alarm_RunLed(1);
            Delay_ms(100);
            Alarm_RunLed(0);
            Delay_ms(100);
        }
    }

    NRF24_SetRxAddress(CORE_RX_ADDR);
    NRF24_SetModeRx();
    g_last_rx_ms = Millis();

    while (1)
    {
        ReceiveTask();
        SafetyTask();
        TimerTask();
        CommWatchdogTask();
        Alarm_SafeBlinkTask();

        if ((Millis() - g_last_adc_ms) >= 100)
        {
            g_last_adc_ms = Millis();
            g_voltage_mv = Voltage_GetMilliVolt();
        }

        if ((Millis() - g_last_status_ms) >= 200)
        {
            g_last_status_ms = Millis();
            SendStatus();
        }
    }
}
