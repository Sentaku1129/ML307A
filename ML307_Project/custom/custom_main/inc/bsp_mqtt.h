#ifndef _BSP_MQTT_
#define _BSP_MQTT_

#include "cm_mqtt.h"
#include "custom_main.h"

typedef struct
{
    char hostname[32];
    int hostport;
    char clientid[32];
    char username[32];
    char password[256];
} g_mqtt_client_t;

extern g_mqtt_client_t g_mqtt_client;

int mqtt_init(void);
cm_mqtt_state_e mqtt_status_get();
void pub_onenet(char *my_sensor_string[5]);

#endif
