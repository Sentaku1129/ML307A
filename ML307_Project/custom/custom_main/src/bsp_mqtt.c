#include "string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "cm_uart.h"
#include "cm_mqtt.h"
#include "cm_mem.h"

#include "bsp_mqtt.h"
#include "bsp_uart.h"

/* PUB QOS STATUS */
typedef enum
{
    CM_MQTT_PUBLISH_DUP = 8u,
    CM_MQTT_PUBLISH_QOS_0 = ((0u << 1) & 0x06),
    CM_MQTT_PUBLISH_QOS_1 = ((1u << 1) & 0x06),
    CM_MQTT_PUBLISH_QOS_2 = ((2u << 1) & 0x06),
    CM_MQTT_PUBLISH_QOS_MASK = ((3u << 1) & 0x06),
    CM_MQTT_PUBLISH_RETAIN = 0x01
} cm_mqtt_publish_flags_e;

static cm_mqtt_client_t *mqtt_client = NULL;

/**
 *  \brief 连接状态回调
 *
 *  \param [in] client mqtt客户端
 *  \param [in] session session标志
 *  \param [in] conn_res 连接状态
 *  \return 成功返回0，失败返回-1
 *
 *  \details More details
 */
static int __mqtt_manager_default_connack_cb(cm_mqtt_client_t *client, int session, cm_mqtt_conn_state_e conn_res)
{
    uart0_printf("\r\nMQTT CONNECT STATUS , CONNECT: %d\r\n", conn_res);

    return 0;
}

/**
 *  \brief server->client发布消息回调
 *
 *  \param [in] client mqtt客户端
 *  \param [in] msgid 消息ID
 *  \param [in] topic 主题
 *  \param [in] payload 负载
 *  \param [in] payload_len 负载长度
 *  \param [in] total_len 负载总长度
 *  \return 成功返回0，失败返回-1
 *
 *  \details
 */
static int __mqtt_manager_default_publish_cb(cm_mqtt_client_t *client, unsigned short msgid, char *topic, int total_len, int payload_len, char *payload)
{
    uart0_printf("\r\nMQTT RECV , recv: %d,%s,%d,%d\r\n", msgid, topic, total_len, payload_len);

    /* 由于测试示例限制打印长度 */
    int printf_size = payload_len > 600 ? 600 : payload_len;
    uart0_printf("\r\n[MQTT]CM MQTT index[%d] , recv: %.*s\r\n", printf_size, payload);

    return 0;
}

/**
 *  \brief client->server发布消息ack回调
 *
 *  \param [in] client mqtt客户端
 *  \param [in] msgid 消息ID
 *  \param [in] dup dup标志
 *  \return 成功返回0，失败返回-1
 *
 *  \details More details
 */
static int __mqtt_manager_default_puback_cb(cm_mqtt_client_t *client, unsigned short msgid, char dup)
{
    uart0_printf("\r\nMQTT PUB_ACK , pub_ack: %d,%d\r\n", msgid, dup);
    return 0;
}

/**
 *  \brief client->server发布消息recv回调
 *
 *  \param [in] client mqtt客户端
 *  \param [in] msgid 消息ID
 *  \param [in] dup dup标志
 *  \return 成功返回0，失败返回-1
 *
 *  \details More details
 */
static int __mqtt_manager_default_pubrec_cb(cm_mqtt_client_t *client, unsigned short msgid, char dup)
{
    uart0_printf("\r\nMQTT PUB_RECV , pub_rec: %d,%d\r\n", msgid, dup);
    return 0;
}

/**
 *  \brief client->server发布消息comp回调
 *
 *  \param [in] client mqtt客户端
 *  \param [in] msgid 消息ID
 *  \param [in] dup dup标志
 *  \return 成功返回0，失败返回-1
 *
 *  \details More details
 */
static int __mqtt_manager_default_pubcomp_cb(cm_mqtt_client_t *client, unsigned short msgid, char dup)
{
    uart0_printf("\r\nMQTT PUB , pub_comp: %d,%d\r\n", msgid, dup);
    return 0;
}

/**
 *  \brief 订阅ack回调
 *
 *  \param [in] client mqtt客户端
 *  \param [in] msgid 消息ID
 *  \return 成功返回0，失败返回-1
 *
 *  \details More details
 */
static int __mqtt_manager_default_suback_cb(cm_mqtt_client_t *client, unsigned short msgid, int count, int qos[])
{
    uart0_printf("\r\nMQTT SUB , sub_ack: %d\r\n", msgid);
    return 0;
}

/**
 *  \brief 取消订阅ack回调
 *
 *  \param [in] client mqtt客户端
 *  \param [in] msgid 消息ID
 *  \return 成功返回0，失败返回-1
 *
 *  \details More details
 */
static int __mqtt_manager_default_unsuback_cb(cm_mqtt_client_t *client, unsigned short msgid)
{
    uart0_printf("\r\nMQTT UNSUB, unsub_ack: %d\r\n", msgid);
    return 0;
}

/**
 *  \brief ping回调
 *
 *  \param [in] client mqtt客户端
 *  \param [in] ret 消息状态，0：ping成功，1：ping超时
 *  \return 成功返回0，失败返回-1
 *
 *  \details More details
 */
static int __mqtt_manager_default_pingresp_cb(cm_mqtt_client_t *client, int ret)
{
    uart0_printf("\r\nMQTT ping status : %s\r\n", ret ? "True" : "False");
    return 0;
}

/**
 *  \brief 消息超时回调，包括publish/subscribe/unsubscribe等
 *
 *  \param [in] client mqtt客户端
 *  \param [in] msgid 消息ID
 *  \return 成功返回0，失败返回-1
 *
 *  \details More details
 */
static int __mqtt_manager_default_timeout_cb(cm_mqtt_client_t *client, unsigned short msgid)
{
    uart0_printf("\r\n MQTT msg timeout!!!\r\n");
    return 0;
}

static void __cm_get_sub_topic(cm_mqtt_client_t *client)
{
    linklist_t *list = cm_mqtt_client_get_sub_topics(client); // 获取topic列表

    if (list == NULL || list->count == 0)
    {
        return;
    }

    char topic_tmp[200] = {0};
    int tmp_len = 0;
    int sub_topic_count = 0;
    linklist_element_t *element = NULL;
    cm_mqtt_topic_t *topic_msg = NULL;

    while ((element = linklist_next_element(list, &element)) != NULL) // 取出元素
    {
        topic_msg = (cm_mqtt_topic_t *)element->content;

        if (topic_msg->state != CM_MQTT_TOPIC_SUBSCRIBED)
        {
            continue;
        }

        /* 已订阅 */
        sub_topic_count++;
        memcpy(topic_tmp + tmp_len, topic_msg->topic, topic_msg->topic_len);
        tmp_len += topic_msg->topic_len;
        tmp_len += snprintf(topic_tmp + tmp_len, sizeof(topic_tmp) - tmp_len, ",%d,", topic_msg->qos);
    }

    topic_tmp[tmp_len - 1] = '\0';

    if (sub_topic_count > 0)
    {
        uart0_printf("\r\n%s\r\n", topic_tmp);
    }
}

int mqtt_init(void)
{
    mqtt_client = cm_mqtt_client_create();
    if (mqtt_client == NULL)
    {
        uart0_printf("mqtt create error\r\n");
        return -1;
    }

    /* 设置回调函数，连接、订阅、发布等接口均为异步接口，结果请根据回调函数返回进行判断 */
    cm_mqtt_client_cb_t callback = {0};
    callback.connack_cb = __mqtt_manager_default_connack_cb;
    callback.publish_cb = __mqtt_manager_default_publish_cb;
    callback.puback_cb = __mqtt_manager_default_puback_cb;
    callback.pubrec_cb = __mqtt_manager_default_pubrec_cb;
    callback.pubcomp_cb = __mqtt_manager_default_pubcomp_cb;
    callback.suback_cb = __mqtt_manager_default_suback_cb;
    callback.unsuback_cb = __mqtt_manager_default_unsuback_cb;
    callback.pingresp_cb = __mqtt_manager_default_pingresp_cb;
    callback.timeout_cb = __mqtt_manager_default_timeout_cb;

    /* 设置client参数 */
    int version = 4;       // 版本3.1.1
    int pkt_timeout = 10;  // 发送超时10秒
    int reconn_times = 5;  // 重连五次
    int reconn_cycle = 10; // 重连间隔10秒
    int reconn_mode = 0;   // 以固定间隔尝试重连
    int retry_times = 3;   // 重传三次
    int ping_cycle = 60;   // ping周期60秒
    int dns_priority = 2;  // MQTT dns解析ipv6优先

    cm_mqtt_client_set_opt(mqtt_client, CM_MQTT_OPT_EVENT, (void *)&callback);
    cm_mqtt_client_set_opt(mqtt_client, CM_MQTT_OPT_VERSION, (void *)&version);
    cm_mqtt_client_set_opt(mqtt_client, CM_MQTT_OPT_PKT_TIMEOUT, (void *)&pkt_timeout);
    cm_mqtt_client_set_opt(mqtt_client, CM_MQTT_OPT_RETRY_TIMES, (void *)&retry_times);
    cm_mqtt_client_set_opt(mqtt_client, CM_MQTT_OPT_RECONN_MODE, (void *)&reconn_mode);
    cm_mqtt_client_set_opt(mqtt_client, CM_MQTT_OPT_RECONN_TIMES, (void *)&reconn_times);
    cm_mqtt_client_set_opt(mqtt_client, CM_MQTT_OPT_RECONN_CYCLE, (void *)&reconn_cycle);
    cm_mqtt_client_set_opt(mqtt_client, CM_MQTT_OPT_PING_CYCLE, (void *)&ping_cycle);
    cm_mqtt_client_set_opt(mqtt_client, CM_MQTT_OPT_DNS_PRIORITY, (void *)&dns_priority);

    /* MQTT 连接*/
    strcpy(g_mqtt_client.hostname, "mqtts.heclouds.com");
    g_mqtt_client.hostport = 1883;

    uart0_printf("_mqtt_init_: onenet clientid : %s\r\n", g_mqtt_client.clientid);
    uart0_printf("_mqtt_init_: onenet uaername : %s\r\n", g_mqtt_client.username);
    uart0_printf("_mqtt_init_: onenet token : %s\r\n", g_mqtt_client.password);

    /* 配置连接参数，对于字符串参数，内部仅保留指针，不分配空间 */
    cm_mqtt_connect_options_t conn_options = {
        .hostport = (unsigned short)g_mqtt_client.hostport,
        .hostname = (const char *)g_mqtt_client.hostname,
        .clientid = (const char *)g_mqtt_client.clientid,
        .username = (const char *)g_mqtt_client.username,
        .password = (const char *)g_mqtt_client.password,
        .keepalive = (unsigned short)60,
        .will_topic = NULL,
        .will_message = NULL,
        .will_message_len = 0,
        .will_flag = 0, // 若要使用遗嘱机制请置1，并补充相关遗嘱信息
        .clean_session = (char)1,
    };

    cm_mqtt_client_connect(mqtt_client, &conn_options); // 连接

    return 0;
}

cm_mqtt_state_e mqtt_status_get()
{
    return cm_mqtt_client_get_state(mqtt_client);
}

void pub_onenet(char *my_sensor_string[5])
{
    // 创建根节点
    cJSON *root = cJSON_CreateObject();

    // 添加 id 字段
    cJSON_AddStringToObject(root, "id", "123");
    // 添加 version 字段
    cJSON_AddStringToObject(root, "version", "1.0");

    // 创建 params 节点
    cJSON *params = cJSON_CreateObject();

    // 创建五个sonsor节点
    cJSON *sensor0 = cJSON_CreateObject();
    cJSON *sensor1 = cJSON_CreateObject();
    cJSON *sensor2 = cJSON_CreateObject();
    cJSON *sensor3 = cJSON_CreateObject();
    cJSON *sensor4 = cJSON_CreateObject();

    // 添加相应的value到对应的sensor节点中
    cJSON_AddStringToObject(sensor0, "value", (const char *)my_sensor_string[0]);
    cJSON_AddStringToObject(sensor1, "value", (const char *)my_sensor_string[1]);
    cJSON_AddStringToObject(sensor2, "value", (const char *)my_sensor_string[2]);
    cJSON_AddStringToObject(sensor3, "value", (const char *)my_sensor_string[3]);
    cJSON_AddStringToObject(sensor4, "value", (const char *)my_sensor_string[4]);

    // 将sensor节点添加到params节点
    cJSON_AddItemToObject(params, "sensor0", sensor0);
    cJSON_AddItemToObject(params, "sensor1", sensor1);
    cJSON_AddItemToObject(params, "sensor2", sensor2);
    cJSON_AddItemToObject(params, "sensor3", sensor3);
    cJSON_AddItemToObject(params, "sensor4", sensor4);

    // 将 params 节点添加到根节点
    cJSON_AddItemToObject(root, "params", params);

    char *json_string = cJSON_Print(root);

    // uart0_printf("\r\n_pub_onenet_: json_string: %s", json_string);

    int ret = cm_mqtt_client_publish(mqtt_client, "$sys/54shZKHIWK/ML307_B/thing/property/post", json_string, strlen(json_string), CM_MQTT_PUBLISH_QOS_0);

    if (root != NULL)
    {
        cJSON_Delete(root);
        cm_free(json_string);
    }

    if (ret < 0)
    {
        uart0_printf("\r\n_pub_onenet_: pub error \r\n");
    }
}
