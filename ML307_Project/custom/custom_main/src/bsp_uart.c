#include "cm_uart.h"
#include "bsp_uart.h"
#include "cm_iomux.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#include "cm_os.h"
#include "cm_mem.h"
#include "cm_sys.h"
#include "cm_rtc.h"

#include "custom_main.h"
#include "bsp_uart.h"
#include "bsp_mqtt.h"

/* private type */
typedef struct
{
    int msg_type;
} uart_event_msg_t;

/* private data */

#define UART_BUF_LEN 1024

// uart0 data
static osMessageQueueId_t uart0_event_queue = NULL;
static osThreadId_t uart0_event_thread = NULL;
static void *g_uart0_sem = NULL;

// uart0 接收数据任务所需变量
static int uart0_rx_rev_len = 0;
static char uart0_rx_rev_data[UART_BUF_LEN] = {0};
static osThreadId_t Uart0_TaskHandle = NULL; // 串口数据接收、解析任务Handle

// uart1 data
static osMessageQueueId_t uart1_event_queue = NULL;
static osThreadId_t uart1_event_thread = NULL;
static void *g_uart1_sem = NULL;

// uart0 接收数据任务所需变量
static int uart1_rx_rev_len = 0;
static char uart1_rx_rev_data[UART_BUF_LEN] = {0};
static osThreadId_t Uart1_TaskHandle = NULL; // 串口数据接收、解析任务Handle

/* 串口事件回调函数 */
static void uart0_callback(void *param, uint32_t type)
{
    uart_event_msg_t msg = {0};
    if (CM_UART_EVENT_TYPE_RX_ARRIVED & type)
    {
        /* 收到接收事件，触发其他线程执行读取数据 */
        osSemaphoreRelease(g_uart0_sem);
    }

    if (CM_UART_EVENT_TYPE_RX_OVERFLOW & type)
    {
        /* 收到溢出事件，触发其他线程处理溢出事件 */
        msg.msg_type = type;

        if (uart0_event_queue != NULL) // 向队列发送数据
        {
            osMessageQueuePut(uart0_event_queue, &msg, 0, 0);
        }
    }
}

/* 串口事件回调函数 */
static void uart1_callback(void *param, uint32_t type)
{
    uart_event_msg_t msg = {0};
    if (CM_UART_EVENT_TYPE_RX_ARRIVED & type)
    {
        /* 收到接收事件，触发其他线程执行读取数据 */
        osSemaphoreRelease(g_uart1_sem);
    }

    if (CM_UART_EVENT_TYPE_RX_OVERFLOW & type)
    {
        /* 收到溢出事件，触发其他线程处理溢出事件 */
        msg.msg_type = type;

        if (uart1_event_queue != NULL) // 向队列发送数据
        {
            osMessageQueuePut(uart1_event_queue, &msg, 0, 0);
        }
    }
}

// uart0 串口数据处理
static void uart0_recv_task(void *param)
{
    int temp_len = 0;

    while (1)
    {
        if (g_uart0_sem != NULL)
        {
            osSemaphoreAcquire(g_uart0_sem, osWaitForever); // 阻塞
        }

        if (uart0_rx_rev_len < UART_BUF_LEN)
        {
            temp_len = cm_uart_read(bsp_uart_0, (void *)&uart0_rx_rev_data[uart0_rx_rev_len], UART_BUF_LEN - uart0_rx_rev_len, 1000);
            uart0_rx_rev_len += temp_len;
        }

        uart0_printf("_uart0_recv_task_: %s", uart0_rx_rev_data);
        uart0_rx_rev_len = 0;
        temp_len = 0;
        memset(uart0_rx_rev_data, 0, sizeof(uart0_rx_rev_data));
    }
}

static uint8_t sum_bytes(uint8_t data[],uint8_t mode, size_t size)
{
    uint8_t sum = 0x00;
    if(mode == 0x01)
        sum = 0xB1;
    else if(mode == 0x03)
        sum = 0xB9;
    for (size_t i = 0; i < size; ++i)
    {
        sum += data[i];
    }
    return sum;
}

// 比较相加后的结果与给定的单字节数值是否相同
static int compare_sum(uint8_t data[],uint8_t mode, size_t size, uint8_t target)
{
    uint8_t sum = sum_bytes(data, mode, size);
    /*
    if(mode == 0x01)
        uart0_printf("%#X, %#X, %#X, %#X, %#X, %#X\r\n", data[0], data[1], data[2], data[3], data[4], data[5]);
    else
        uart0_printf("%#X, %#X, %#X, %#X, %#X, %#X, %#X, %#X, %#X, %#X, %#X, %#X\r\n",  data[0], data[1], data[2], data[3], data[4], data[5],
                                                                                        data[6], data[7], data[8], data[9], data[10], data[11]);
    */
    return sum == target;
}

// uart1 串口数据处理
static void uart1_recv_task(void *param)
{
    int temp_len = 0;
    int temp_sensor_data_len = 0;
    g_sensor_t uart1_sensor = {0};
    uint8_t sensor_status = 0;

    uint64_t snesor_time = 0;

    static char sensor_check = 0x00;

    while (1)
    {
#if 0
        if (g_uart1_sem != NULL)
        {
            osSemaphoreAcquire(g_uart1_sem, osWaitForever);//阻塞
        }

        if (uart1_rx_rev_len < UART_BUF_LEN)
        {
            temp_len = cm_uart_read(bsp_uart_1, (void*)&uart1_rx_rev_data[uart1_rx_rev_len], UART_BUF_LEN - uart1_rx_rev_len, 1000);
            uart1_rx_rev_len += temp_len;
        }

        uart0_printf("_uart1_recv_task_: %s", uart1_rx_rev_data);
        uart1_rx_rev_len = 0;
        temp_len = 0;
        memset(uart1_rx_rev_data, 0, sizeof(uart1_rx_rev_data));
#else
        temp_len = cm_uart_read(bsp_uart_1, (void *)&uart1_rx_rev_data[0], 1, 1000);
        if (uart1_rx_rev_data[0] == 0x55)
        {
            memset(uart1_rx_rev_data, 0, temp_len);
            temp_len = cm_uart_read(bsp_uart_1, (void *)&uart1_rx_rev_data[0], 1, 1000);
            if (uart1_rx_rev_data[0] == 0x55)
            {
                memset(uart1_rx_rev_data, 0, temp_len);
                temp_len = cm_uart_read(bsp_uart_1, (void *)&uart1_rx_rev_data[0], 2, 1000);
                temp_sensor_data_len = (int)uart1_rx_rev_data[1];
                // uart0_printf("temp_sensor_data_len : %d\r\n", temp_sensor_data_len);

                switch (uart1_rx_rev_data[0])
                {
                case 0x01:
                    memset(uart1_rx_rev_data, 0, temp_len);
                    temp_len = cm_uart_read(bsp_uart_1, (void *)&uart1_rx_rev_data[0], temp_sensor_data_len, 1000);
                    cm_uart_read(bsp_uart_1, (void *)&sensor_check, 1, 1000);
                    if(!compare_sum((uint8_t *)uart1_rx_rev_data, 0x01, temp_sensor_data_len, (uint8_t)sensor_check))
                    {
                        // uart0_printf("check error!!!\r\n");
                        break;
                    }
                    /*
                    uart0_printf("0x01 %#X %#X %#X %#X %#X %#X\r\n", uart1_rx_rev_data[0], uart1_rx_rev_data[1],
                                 uart1_rx_rev_data[2], uart1_rx_rev_data[3],
                                 uart1_rx_rev_data[4], uart1_rx_rev_data[5]);
                    */
                    // 姿态角数据解析
                    // uint16_t temp_sensor_count = (int16_t)uart1_rx_rev_data[1] << 8 | uart1_rx_rev_data[0];
                    uart1_sensor.g_angle.x = ((float)((int16_t)(uart1_rx_rev_data[1] << 8) | uart1_rx_rev_data[0])) / 32768 * 180;
                    uart1_sensor.g_angle.y = ((float)((int16_t)(uart1_rx_rev_data[3] << 8) | uart1_rx_rev_data[2])) / 32768 * 180;
                    uart1_sensor.g_angle.z = ((float)((int16_t)(uart1_rx_rev_data[5] << 8) | uart1_rx_rev_data[4])) / 32768 * 180;
                    sensor_status |= 0x01;
                    break;
                // case 0x02:
                //     break;
                case 0x03:
                    memset(uart1_rx_rev_data, 0, temp_len);
                    temp_len = cm_uart_read(bsp_uart_1, (void *)&uart1_rx_rev_data[0], temp_sensor_data_len, 1000);
                    cm_uart_read(bsp_uart_1, (void *)&sensor_check, 1, 1000);
                    if(!compare_sum((uint8_t *)uart1_rx_rev_data, 0x03, temp_sensor_data_len, (uint8_t)sensor_check))
                    {
                        // uart0_printf("check error!!!\r\n");
                        break;
                    }
                    /*
                    uart0_printf("0x03 %#X %#X %#X %#X %#X %#X %#X %#X %#X %#X %#X %#X\r\n",    uart1_rx_rev_data[0], uart1_rx_rev_data[1],
                                                                                                uart1_rx_rev_data[2], uart1_rx_rev_data[3],
                                                                                                uart1_rx_rev_data[4], uart1_rx_rev_data[5],
                                                                                                uart1_rx_rev_data[6], uart1_rx_rev_data[7],
                                                                                                uart1_rx_rev_data[8], uart1_rx_rev_data[9],
                                                                                                uart1_rx_rev_data[10], uart1_rx_rev_data[11]);
                    */
                        // 加速度解析
                    uart1_sensor.g_acceleration.x = ((float)((int16_t)(uart1_rx_rev_data[1] << 8) | uart1_rx_rev_data[0])) / 32768 * 4;
                    uart1_sensor.g_acceleration.y = ((float)((int16_t)(uart1_rx_rev_data[3] << 8) | uart1_rx_rev_data[2])) / 32768 * 4;
                    uart1_sensor.g_acceleration.z = ((float)((int16_t)(uart1_rx_rev_data[5] << 8) | uart1_rx_rev_data[4])) / 32768 * 4;
                    // 角速度解析
                    uart1_sensor.g_palstance.x = ((float)((int16_t)(uart1_rx_rev_data[7] << 8) | uart1_rx_rev_data[6])) / 32768 * 2000;
                    uart1_sensor.g_palstance.y = ((float)((int16_t)(uart1_rx_rev_data[9] << 8) | uart1_rx_rev_data[8])) / 32768 * 2000;
                    uart1_sensor.g_palstance.z = ((float)((int16_t)(uart1_rx_rev_data[11] << 8) | uart1_rx_rev_data[10])) / 32768 * 2000;
                    sensor_status |= 0x02;
                    break;
                    // case 0x04:
                    //     break;

                default:
                    memset(uart1_rx_rev_data, 0, temp_len);
                    temp_len = cm_uart_read(bsp_uart_1, (void *)&uart1_rx_rev_data[0], temp_sensor_data_len, 1000);
                    break;
                }
                /*
                memset(uart1_rx_rev_data, 0, temp_len);
                temp_len = cm_uart_read(bsp_uart_1, (void *)&uart1_rx_rev_data[0], 1, 1000);
                */
                memset(uart1_rx_rev_data, 0, temp_len);
                temp_len = 0;
                temp_sensor_data_len = 0;
                sensor_check = 0x00;
            }

            /*
            uart0_printf("%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\r\n",
                         uart1_sensor.g_acceleration.x, uart1_sensor.g_acceleration.y, uart1_sensor.g_acceleration.z,
                         uart1_sensor.g_palstance.x, uart1_sensor.g_palstance.y, uart1_sensor.g_palstance.z,
                         uart1_sensor.g_angle.x, uart1_sensor.g_angle.y, uart1_sensor.g_angle.z);
            */

            if (sensor_status == 0x03)
            {
                if (cm_rtc_get_current_time() - snesor_time >= 1)
                {
                    snesor_time = cm_rtc_get_current_time();
                    osMessageQueuePut(sensor_MessageQueueId, &uart1_sensor, 0, 0);
                    uart0_printf("_uart1_recv_task_: sensor data OK\r\n");
                }

                sensor_status = 0;
            }
        }
#endif
        // osDelay(20);
    }
}

/* 用于测试串口事件，用户可参考 */
static void uart0_event_task(void *arg)
{
    uart_event_msg_t msg = {0};

    while (1)
    {
        if (osMessageQueueGet(uart0_event_queue, &msg, NULL, osWaitForever) == osOK)
        {
            // cm_log_printf(0, "uart event msg type = %d\n", msg.msg_type);
            if (CM_UART_EVENT_TYPE_RX_OVERFLOW & msg.msg_type)
            {
                cm_log_printf(0, "CM_UART_EVENT_TYPE_RX_OVERFLOW... ...");
            }
        }
    }
}

static void uart1_event_task(void *arg)
{
    uart_event_msg_t msg = {0};

    while (1)
    {
        if (osMessageQueueGet(uart1_event_queue, &msg, NULL, osWaitForever) == osOK)
        {
            // cm_log_printf(0, "uart event msg type = %d\n", msg.msg_type);
            if (CM_UART_EVENT_TYPE_RX_OVERFLOW & msg.msg_type)
            {
                cm_log_printf(0, "CM_UART_EVENT_TYPE_RX_OVERFLOW... ...");
            }
        }
    }
}

/* 用于测试串口事件，用户可参考 */
static int uart0_event_task_create(void)
{
    if (uart0_event_queue == NULL)
    {
        uart0_event_queue = osMessageQueueNew(10, sizeof(uart_event_msg_t), NULL);
    }

    if (uart0_event_thread == NULL)
    {
        osThreadAttr_t attr1 = {
            .name = "uart0_event",
            .priority = UART_TASK_PRIORITY,
            .stack_size = 1024,
        };
        uart0_event_thread = osThreadNew(uart0_event_task, NULL, (const osThreadAttr_t *)&attr1);
    }

    return 0;
}

/* 用于测试串口事件，用户可参考 */
static int uart1_event_task_create(void)
{
    if (uart1_event_queue == NULL)
    {
        uart1_event_queue = osMessageQueueNew(10, sizeof(uart_event_msg_t), NULL);
    }

    if (uart1_event_thread == NULL)
    {
        osThreadAttr_t attr1 = {
            .name = "uart1_event",
            .priority = osPriorityLow,
            .stack_size = 1024,
        };
        uart1_event_thread = osThreadNew(uart1_event_task, NULL, (const osThreadAttr_t *)&attr1);
    }

    return 0;
}

void uart0_init(void)
{
    int32_t ret = -1;

    /* 配置参数 */
    cm_uart_cfg_t config =
        {
            CM_UART_BYTE_SIZE_8,
            CM_UART_PARITY_NONE,
            CM_UART_STOP_BIT_ONE,
            CM_UART_FLOW_CTRL_NONE,
            CM_UART_BAUDRATE_115200,
            // CM_UART_BAUDRATE_9600,
            0 // 配置为普通串口模式，若要配置为低功耗模式可改为1
        };

    /* 事件参数 */
    cm_uart_event_t event =
        {
            CM_UART_EVENT_TYPE_RX_ARRIVED | CM_UART_EVENT_TYPE_RX_OVERFLOW, // 注册需要上报的事件类型
            "bsp_uart0",                                                    // 用户参数
            uart0_callback                                                  // 上报事件的回调函数
        };

    cm_log_printf(0, "uart NUM = %d start... ...", bsp_uart_0);

    /* 配置引脚复用 */
    cm_iomux_set_pin_func(BSP_UART0TX_IOMUX);
    cm_iomux_set_pin_func(BSP_UART0RX_IOMUX);

    /* 注册事件和回调函数 */
    ret = cm_uart_register_event(bsp_uart_0, &event);

    /* 开启串口 */
    ret = cm_uart_open(bsp_uart_0, &config);

    if (ret != RET_SUCCESS)
    {
        cm_log_printf(0, "uart init err,ret=%d\n", ret);
        return;
    }

    /* 以下为串口接收示例，不影响串口配置，用户可酌情参考 */
    osThreadAttr_t uart_task_attr = {0};
    uart_task_attr.name = "uart_task";
    uart_task_attr.stack_size = 2048;
    uart_task_attr.priority = UART_TASK_PRIORITY;

    if (g_uart0_sem == NULL)
    {
        g_uart0_sem = osSemaphoreNew(1, 0, NULL);
    }

    Uart0_TaskHandle = osThreadNew(uart0_recv_task, 0, &uart_task_attr);

    uart0_event_task_create();
}

void uart0_printf(char *str, ...)
{
    static char s[600]; // This needs to be large enough to store the string TODO Change magic number
    va_list args;
    int len;

    if ((str == NULL) || (strlen(str) == 0))
    {
        return;
    }

    va_start(args, str);
    len = vsnprintf((char *)s, 600, str, args);
    va_end(args);
    cm_uart_write(bsp_uart_0, s, len, 1000);
}

void uart1_init(void)
{
    int32_t ret = -1;

    /* 配置参数 */
    cm_uart_cfg_t config =
        {
            CM_UART_BYTE_SIZE_8,
            CM_UART_PARITY_NONE,
            CM_UART_STOP_BIT_ONE,
            CM_UART_FLOW_CTRL_NONE,
            CM_UART_BAUDRATE_115200,
            0 // 配置为普通串口模式，若要配置为低功耗模式可改为1
        };

    /* 事件参数 */
    cm_uart_event_t event =
        {
            CM_UART_EVENT_TYPE_RX_ARRIVED | CM_UART_EVENT_TYPE_RX_OVERFLOW, // 注册需要上报的事件类型
            "bsp_uart1",                                                    // 用户参数
            uart1_callback                                                  // 上报事件的回调函数
        };

    cm_log_printf(0, "uart NUM = %d start... ...", bsp_uart_1);

    /* 配置引脚复用 */
    cm_iomux_set_pin_func(BSP_UART1TX_IOMUX);
    cm_iomux_set_pin_func(BSP_UART1RX_IOMUX);

    /* 注册事件和回调函数 */
    ret = cm_uart_register_event(bsp_uart_1, &event);

    /* 开启串口 */
    ret = cm_uart_open(bsp_uart_1, &config);

    if (ret != RET_SUCCESS)
    {
        cm_log_printf(0, "uart init err,ret=%d\n", ret);
        return;
    }

    /* 以下为串口接收示例，不影响串口配置，用户可酌情参考 */
    osThreadAttr_t uart_task_attr = {0};
    uart_task_attr.name = "uart1_task";
    uart_task_attr.stack_size = 2048;
    uart_task_attr.priority = osPriorityLow;

    if (g_uart1_sem == NULL)
    {
        g_uart1_sem = osSemaphoreNew(1, 0, NULL);
    }

    Uart1_TaskHandle = osThreadNew(uart1_recv_task, 0, &uart_task_attr);

    uart1_event_task_create();
}
