/*********************************************************
*  @file    cm_main.h
*  @brief   ML302 OpenCPU main header file
*  Copyright (c) 2019 China Mobile IOT.
*  All rights reserved.
*  created by XieGangLiang 2019/10/08
********************************************************/
#ifndef __CM_MAIN_H__
#define __CM_MAIN_H__

#include "cm_os.h"

#define pet_post_uri "http://47.108.89.231:9004"

typedef enum{
	RET_BUSY = -2,
	RET_ERROR =-1,
	RET_SUCCESS = 0
}CM_RET_E;

#ifndef FALSE
#define FALSE (0U)
#endif

#ifndef TRUE
#define TRUE (1U)
#endif

/* 加速度 */
typedef struct 
{
    float x;
    float y;
    float z;
}g_acceleration_t;

/* 角速度 */
typedef struct 
{
    float x;
    float y;
    float z;
}g_palstance_t;

/* 角度 */
typedef struct
{
    float x;
    float y;
    float z;
}g_angle_t;

/* 传感器数据 */
typedef struct
{
    g_acceleration_t g_acceleration;
    g_palstance_t g_palstance;
    g_angle_t g_angle;
}g_sensor_t;

extern osMessageQueueId_t sensor_MessageQueueId;

#define UART_TASK_PRIORITY osPriorityNormal


#endif
