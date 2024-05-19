#include "cm_uart.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#include "cm_fs.h"
#include "cm_mem.h"
#include "cm_sys.h"
#include "cm_sim.h"
#include "cm_os.h"
#include "cm_virt_at.h"
#include "cm_rtc.h"
#include "cm_modem.h"
#include "cm_mqtt.h"
#include "cJSON.h"

#include "bsp_uart.h"
#include "bsp_mqtt.h"
#include "bsp_http.h"
#include "bsp_i2c.h"

osMessageQueueId_t sensor_MessageQueueId = NULL;
g_mqtt_client_t g_mqtt_client = {0};

#define SECOND_OF_DAY (24 * 60 * 60)

typedef struct cm_tm
{
	int tm_sec;	 /* 秒 – 取值区间为[0,59] */
	int tm_min;	 /* 分 - 取值区间为[0,59] */
	int tm_hour; /* 时 - 取值区间为[0,23] */
	int tm_mday; /* 一个月中的日期 - 取值区间为[1,31] */
	int tm_mon;	 /* 月份 */
	int tm_year; /* 年份 */
} cm_tm_t;

osThreadId_t OC_APP_TaskHandle;	// main_app 任务句柄
osThreadId_t OC_MPU6050_TaskHandle;	// mpu6050  传感器任务句柄
osEventFlagsId_t cmd_task_flag;

static const char *weekday[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
static const char DayOfMon[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void oc_ring_cb(unsigned char *param)
{
	if (0 == strncmp((char *)param, "\r\nRING", 6)) // 来电提示
	{
		uart0_printf("oc_ring_cb:%s\n", param);
	}
	else if (0 == strncmp((char *)param, "\r\n+CLCC:", 8)) // 来电信息
	{
		uart0_printf("oc_ring_cb:%s\n", param);
	}
	else if (0 == strncmp((char *)param, "\r\nNO CARRIER", 12)) // 对方挂断
	{
		uart0_printf("oc_ring_cb:%s\n", param);
	}
	if (0 == strncmp((char *)param, "\r\n+CMTI:", 8)) // 短信信息
	{
		uart0_printf("message:%s\n", param);
	}
}

static void cm_sec_to_date(long lSec, cm_tm_t *tTime)
{
	unsigned short i, j, iDay;
	unsigned long lDay;

	lDay = lSec / SECOND_OF_DAY;
	lSec = lSec % SECOND_OF_DAY;

	i = 1970;
	while (lDay > 365)
	{
		if (((i % 4 == 0) && (i % 100 != 0)) || (i % 400 == 0))
		{
			lDay -= 366;
		}
		else
		{
			lDay -= 365;
		}
		i++;
	}
	if ((lDay == 365) && !(((i % 4 == 0) && (i % 100 != 0)) || (i % 400 == 0)))
	{
		lDay -= 365;
		i++;
	}
	tTime->tm_year = i;
	for (j = 0; j < 12; j++)
	{
		if ((j == 1) && (((i % 4 == 0) && (i % 100 != 0)) || (i % 400 == 0)))
		{
			iDay = 29;
		}
		else
		{
			iDay = DayOfMon[j];
		}
		if (lDay >= iDay)
			lDay -= iDay;
		else
			break;
	}
	tTime->tm_mon = j + 1;
	tTime->tm_mday = lDay + 1;
	tTime->tm_hour = ((lSec / 3600)) % 24; // 这里注意，世界时间已经加上北京时间差8，
	tTime->tm_min = (lSec % 3600) / 60;
	tTime->tm_sec = (lSec % 3600) % 60;
}

static uint8_t cm_time_to_weekday(cm_tm_t *t)
{
	uint32_t u32WeekDay = 0;
	uint32_t u32Year = t->tm_year;
	uint8_t u8Month = t->tm_mon;
	uint8_t u8Day = t->tm_mday;
	if (u8Month < 3U)
	{
		/*D = { [(23 x month) / 9] + day + 4 + year + [(year - 1) / 4] - [(year - 1) / 100] + [(year - 1) / 400] } mod 7*/
		u32WeekDay = (((23U * u8Month) / 9U) + u8Day + 4U + u32Year + ((u32Year - 1U) / 4U) - ((u32Year - 1U) / 100U) + ((u32Year - 1U) / 400U)) % 7U;
	}
	else
	{
		/*D = { [(23 x month) / 9] + day + 4 + year + [year / 4] - [year / 100] + [year / 400] - 2 } mod 7*/
		u32WeekDay = (((23U * u8Month) / 9U) + u8Day + 4U + u32Year + (u32Year / 4U) - (u32Year / 100U) + (u32Year / 400U) - 2U) % 7U;
	}

	if (0U == u32WeekDay)
	{
		u32WeekDay = 7U;
	}

	return (uint8_t)u32WeekDay;
}

int my_main_app()
{
	/* 串口初始化 */
	uart0_init();

	char buf[CM_VER_LEN] = {0};
	cm_tm_t t;
	int ret;
	int pdp_time_out = 0;
	cm_fs_system_info_t info = {0, 0};
	cm_heap_stats_t stats = {0};

	uart0_printf("\n\n\n\n\n\n\n\n\n\n");
	uart0_printf("OpenCPU Starts\n");
	cm_sys_get_cm_ver(buf, CM_VER_LEN);
	uart0_printf("SDK VERSION:%s\n", buf);
	cm_fs_getinfo(&info);
	cm_mem_get_heap_stats(&stats);
	uart0_printf("fs total:%d,remain:%d\n", info.total_size, info.free_size);
	uart0_printf("heap total:%d,remain:%d\n", stats.total_size, stats.free);

	uart0_printf("waiting for network...\n");

	while (1)
	{
		uart0_printf("_cm_opencpu_entry_: try to times : %d\r\n", pdp_time_out);
		if (pdp_time_out > 20)
		{
			uart0_printf("network timeout\n");
			break;
		}
		if (cm_modem_get_pdp_state(1) == 1)
		{
			uart0_printf("network ready\n");
			break;
		}
		osDelay(200);
		pdp_time_out++;
	}
	uart0_printf("network OK!!!\r\n");

	osDelay(200);
	cm_virt_at_urc_reg((cm_at_urc_callback)oc_ring_cb);
	cm_sec_to_date((long)(cm_rtc_get_current_time() + cm_rtc_get_timezone() * 60 * 60), &t);
	uart0_printf("Now:%d-%d-%d:%d:%d:%d,%s\n", t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, weekday[cm_time_to_weekday(&t) - 1]);

	memset(buf, 0, CM_VER_LEN);
	ret = cm_sys_get_sn(buf);
	if (ret == 0)
	{
		uart0_printf("SN:%s\n", buf);
	}
	else
	{
		uart0_printf("SN ERROR\n");
	}

	memset(buf, 0, CM_VER_LEN);
	ret = cm_sys_get_imei(buf);
	if (ret == 0)
	{
		uart0_printf("IMEI:%s\n", buf);
	}
	else
	{
		uart0_printf("IMEI ERROR\n");
	}

	memset(buf, 0, CM_VER_LEN);
	ret = cm_sim_get_imsi(buf);
	if (ret == 0)
	{
		uart0_printf("IMSI:%s\n", buf);
	}
	else
	{
		uart0_printf("IMSI ERROR\n");
	}

	/* 获取ICCID示例 */
	char cm_iccid[32] = {0};
	cm_sim_get_iccid(cm_iccid);
	uart0_printf("ICCID:%s\n", cm_iccid);

	/* 获取MQTT参数 */  
	while (1)
	{
		cJSON *root = cJSON_CreateObject();
		cJSON_AddStringToObject(root, "deviceName", "ML307_B");
		cJSON_AddStringToObject(root, "productId", "54shZKHIWK");
		char *body = cJSON_PrintUnformatted(root);

		char *pet_resp = cm_malloc(1024);
		memset(pet_resp, 0x00, 1024);

		/* http请求 */
		bool http_ret = http_post(pet_post_uri, "/admin/oneNet/register", body, pet_resp, 1024);

		cm_free(body);
		cJSON_Delete(root);
		if(http_ret == true)
		{
			uart0_printf("%s\r\n", pet_resp);
			cJSON *resp_root =  cJSON_Parse(pet_resp);
			cJSON *resp_status = cJSON_GetObjectItem(resp_root, "status");
			if(resp_status->valueint == 200)
			{
				memset(g_mqtt_client.hostname, 0, sizeof(g_mqtt_client.hostname));
				memset(g_mqtt_client.clientid, 0, sizeof(g_mqtt_client.clientid));
				memset(g_mqtt_client.username, 0, sizeof(g_mqtt_client.username));
				memset(g_mqtt_client.password, 0, sizeof(g_mqtt_client.password));

				cJSON *resp_data = cJSON_GetObjectItem(root, "data");
				strcpy(g_mqtt_client.username, cJSON_GetObjectItem(resp_data, "productId")->valuestring);
				strcpy(g_mqtt_client.clientid, cJSON_GetObjectItem(resp_data, "deviceName")->valuestring);
				strcpy(g_mqtt_client.password, cJSON_GetObjectItem(resp_data, "token")->valuestring);

				cJSON_Delete(resp_root);
				break;
			}
			cJSON_Delete(resp_root);
		}
		osDelay(5000);
	}

	/* MQTT初始化 */
	mqtt_init();

	osThreadAttr_t mpu6050_task_attr = {0};
	mpu6050_task_attr.name = "mpu6050_task";
	mpu6050_task_attr.stack_size = 4096 * 2;
	mpu6050_task_attr.priority = osPriorityLow;

	OC_MPU6050_TaskHandle = osThreadNew((osThreadFunc_t)mpu6050_data_task, 0, &mpu6050_task_attr);

	// uart1_init();

	/* 初始化队列 */
	if (sensor_MessageQueueId == NULL)
	{
		sensor_MessageQueueId = osMessageQueueNew(10, sizeof(g_sensor_t), NULL);
		uart0_printf("_my_main_app_: sensor_MessageQueueId init\r\n");
	}

	g_sensor_t g_sensor[5];
	char *g_sensor_string[5];
	char number_string[32];
	cJSON *root[5];
	int g_sensor_num = 0;

	uint64_t snesor_time = 0;
	

	/* 主函数任务循环 */
	while (1)
	{
		uart0_printf("%s: test main_app\r\n", __func__);
		if (g_sensor_num == 0)
		{
			for (int i = 0; i < 5; i++)
			{
				root[i] = cJSON_CreateObject();
				if (root[i] == NULL)
				{
					uart0_printf("_my_main_app_: Error: Failed to create cJSON object\n");
					// 在创建 cJSON 对象失败时进行错误处理，比如返回错误码或者退出程序
				}
			}
		}
		for (g_sensor_num = 0; g_sensor_num < 5; g_sensor_num++)
		{
			if (osMessageQueueGet(sensor_MessageQueueId, &g_sensor[g_sensor_num], NULL, osWaitForever) != osOK)
			{
				uart0_printf("get g_sensor data error\r\n");
				g_sensor_num--;
			}
		}

		if (g_sensor_num == 5 && (cm_rtc_get_current_time() - snesor_time >= 5))
		{
			if (mqtt_status_get() == CM_MQTT_STATE_CONNECTED)
			{
				for (int i = 0; i < 5; i++)
				{
					// 加速度
					memset(number_string, 0, sizeof(number_string));
					snprintf(number_string, sizeof(number_string), "%.4f", g_sensor[i].g_acceleration.x);
					cJSON_AddStringToObject(root[i], "accelerationX", number_string);
					memset(number_string, 0, sizeof(number_string));
					snprintf(number_string, sizeof(number_string), "%.4f", g_sensor[i].g_acceleration.y);
					cJSON_AddStringToObject(root[i], "accelerationY", number_string);
					memset(number_string, 0, sizeof(number_string));
					snprintf(number_string, sizeof(number_string), "%.4f", g_sensor[i].g_acceleration.z);
					cJSON_AddStringToObject(root[i], "accelerationZ", number_string);

					// 角速度
					memset(number_string, 0, sizeof(number_string));
					snprintf(number_string, sizeof(number_string), "%.4f", g_sensor[i].g_palstance.x);
					cJSON_AddStringToObject(root[i], "palstanceX", number_string);
					memset(number_string, 0, sizeof(number_string));
					snprintf(number_string, sizeof(number_string), "%.4f", g_sensor[i].g_palstance.y);
					cJSON_AddStringToObject(root[i], "palstanceY", number_string);
					memset(number_string, 0, sizeof(number_string));
					snprintf(number_string, sizeof(number_string), "%.4f", g_sensor[i].g_palstance.z);
					cJSON_AddStringToObject(root[i], "palstanceZ", number_string);

					// 角度
					memset(number_string, 0, sizeof(number_string));
					snprintf(number_string, sizeof(number_string), "%.4f", g_sensor[i].g_angle.x);
					cJSON_AddStringToObject(root[i], "angleX", number_string);
					memset(number_string, 0, sizeof(number_string));
					snprintf(number_string, sizeof(number_string), "%.4f", g_sensor[i].g_angle.y);
					cJSON_AddStringToObject(root[i], "angleY", number_string);
					memset(number_string, 0, sizeof(number_string));
					snprintf(number_string, sizeof(number_string), "%.4f", g_sensor[i].g_angle.z);
					cJSON_AddStringToObject(root[i], "angleZ", number_string);

					g_sensor_string[i] = cJSON_PrintUnformatted(root[i]);
				}
				cm_fs_getinfo(&info);
				cm_mem_get_heap_stats(&stats);
				uart0_printf("fs total:%d,remain:%d\n", info.total_size, info.free_size);
				uart0_printf("heap total:%d,remain:%d\n", stats.total_size, stats.free);
				snesor_time = cm_rtc_get_current_time();
				pub_onenet(g_sensor_string);
			}
			else
			{
				uart0_printf("%s: MQTT DISCONNECTED\r\n", __func__);
			}
			for (int i = 0; i < 5; i++)
			{
				cJSON_Delete(root[i]);
				if(g_sensor_string[i] != NULL)
					cm_free(g_sensor_string[i]);
				root[i] = NULL;
			}
			g_sensor_num = 0;
		}
	}
}

int cm_opencpu_entry(char *param)
{
	osThreadAttr_t app_task_attr = {0};
	app_task_attr.name = "main_task";
	app_task_attr.stack_size = 4096 * 2;
	app_task_attr.priority = osPriorityNormal;

	OC_APP_TaskHandle = osThreadNew((osThreadFunc_t)my_main_app, 0, &app_task_attr);
	return 0;
}

// 编译 .\ML307A_build.bat GCLN custom
