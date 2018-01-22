/*
 * Copyright (c) 2014-2016 Alibaba Group. All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iot_import.h"
#include "iot_export.h"
#include "json_parser.h"

#if defined(MQTT_ID2_AUTH) && defined(TEST_ID2_DAILY)
    #define PRODUCT_KEY             "*******************"
    #define DEVICE_NAME             "*******************"
    #define DEVICE_SECRET           "*******************"
#else
    #define PRODUCT_KEY             "*******************"
    #define DEVICE_NAME             "*******************"
    #define DEVICE_SECRET           "*******************"
#endif

/*
 * Get the PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET from json file.
 * And restrict the name of json file to be same with this c file.
 * Also restrict the path of json file at /usr/sbin directory.
 */
#define DEV_JSON_FILE "/usr/sbin/mqtt-perform.json"

#if defined(DEV_JSON_FILE)
	/* These are pre-defined topics */
	#define TOPIC_UPDATE(topic, p_key, d_name) \
		sprintf(topic, "/%s/%s/update", p_key, d_name)
	#define TOPIC_ERROR(topic, p_key, d_name) \
		sprintf(topic, "/%s/%s/update/error", p_key, d_name)
	#define TOPIC_GET(topic, p_key, d_name) \
		sprintf(topic, "/%s/%s/get", p_key, d_name)
	#define TOPIC_DATA(topic, p_key, d_name) \
		sprintf(topic, "/%s/%s/data", p_key, d_name)
#else
	/* These are pre-defined topics */
	#define TOPIC_UPDATE            "/"PRODUCT_KEY"/"DEVICE_NAME"/update"
	#define TOPIC_ERROR             "/"PRODUCT_KEY"/"DEVICE_NAME"/update/error"
	#define TOPIC_GET               "/"PRODUCT_KEY"/"DEVICE_NAME"/get"
	#define TOPIC_DATA              "/"PRODUCT_KEY"/"DEVICE_NAME"/data"
#endif

#define MSG_LEN_MAX             (1024)

#define EXAMPLE_TRACE(fmt, args...)  \
    do { \
        printf("%s|%03d :: ", __func__, __LINE__); \
        printf(fmt, ##args); \
        printf("%s", "\r\n"); \
    } while(0)

#define MAX_DEV_NUM 32

typedef struct _iot_device_t {
	char *dev_name;
	char *dev_secret;
} iot_device_t;

static int      user_argc;
static char   **user_argv;
static char *product_key = NULL;
static iot_device_t iot_dev[MAX_DEV_NUM];
static int iot_dev_num = 0;

static void json_dev_from_file(const char *filename)
{
	FILE *file = NULL;
	long length = 0;
	size_t read_chars = 0;
	char *dev_str = NULL;
	list_head_t *key_list = NULL;
	json_key_t *pos;
	int i;

	/* open in read binary mode */
	file = fopen(filename, "rb");
	if (file == NULL)
	{
		goto cleanup;
	}

        EXAMPLE_TRACE("Get the device JSON file %s and open it.", filename);

	/* get the length */
	if (fseek(file, 0, SEEK_END) != 0)
	{
		goto cleanup;
	}
	length = ftell(file);
	if (length < 0)
	{
		goto cleanup;
	}
	if (fseek(file, 0, SEEK_SET) != 0)
	{
		goto cleanup;
	}

	/* allocate content buffer */
	dev_str = (char*)malloc((size_t)length + sizeof(""));
	if (dev_str == NULL)
	{
		goto cleanup;
	}

	/* read the file into memory */
	read_chars = fread(dev_str, sizeof(char), (size_t)length, file);
	if ((long)read_chars != length)
	{
		free(dev_str);
		dev_str = NULL;
		goto cleanup;
	}
	dev_str[read_chars] = '\0';

	/* Obtain the product key first */
	product_key = LITE_json_value_of((char *)"product_key", (char *)dev_str);
	if (product_key == NULL) {
		EXAMPLE_TRACE("Can NOT obtain the product key from %s!", DEV_JSON_FILE);
		free(dev_str);
		dev_str = NULL;
		goto cleanup;
	}
	EXAMPLE_TRACE("The product key is %s.", product_key);

	for (i = 0; i < MAX_DEV_NUM; i++) {
	    iot_dev[i].dev_name = NULL;
	    iot_dev[i].dev_secret = NULL;
	}

	/* Parse all the devices from json file */
	key_list = LITE_json_keys_of(dev_str, "");

	list_for_each_entry(pos, key_list, list, json_key_t) {
	    if (pos->key) {
		if (strstr(pos->key, "dev_name")) {
			iot_dev[iot_dev_num].dev_name = LITE_json_value_of(pos->key, dev_str);
			log_info("%-28s: %.48s", pos->key, iot_dev[iot_dev_num].dev_name);
		}

		if (strstr(pos->key, "dev_secret")) {
			iot_dev[iot_dev_num].dev_secret = LITE_json_value_of(pos->key, dev_str);
			log_info("%-28s: %.48s", pos->key, iot_dev[iot_dev_num].dev_secret);
		}
	    }

	    if (iot_dev[iot_dev_num].dev_name && iot_dev[iot_dev_num].dev_secret) {
		iot_dev_num++;
		if (iot_dev_num >= MAX_DEV_NUM) {
			EXAMPLE_TRACE("Reach max support device: %u!", (unsigned int)iot_dev_num);
			break;
		}
	    }
	}
	EXAMPLE_TRACE("The total devices: %u!", (unsigned int)iot_dev_num);
	LITE_json_keys_release(key_list);
	free(dev_str);

cleanup:
	if (file != NULL)
	{
		fclose(file);
	}
}

void event_handle(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    uintptr_t packet_id = (uintptr_t)msg->msg;
    iotx_mqtt_topic_info_pt topic_info = (iotx_mqtt_topic_info_pt)msg->msg;

    switch (msg->event_type) {
        case IOTX_MQTT_EVENT_UNDEF:
            EXAMPLE_TRACE("undefined event occur.");
            break;

        case IOTX_MQTT_EVENT_DISCONNECT:
            EXAMPLE_TRACE("MQTT disconnect.");
            break;

        case IOTX_MQTT_EVENT_RECONNECT:
            EXAMPLE_TRACE("MQTT reconnect.");
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_SUCCESS:
            EXAMPLE_TRACE("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_TIMEOUT:
            EXAMPLE_TRACE("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_NACK:
            EXAMPLE_TRACE("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_SUCCESS:
            EXAMPLE_TRACE("unsubscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
            EXAMPLE_TRACE("unsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_NACK:
            EXAMPLE_TRACE("unsubscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_SUCCESS:
            EXAMPLE_TRACE("publish success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_TIMEOUT:
            EXAMPLE_TRACE("publish timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_NACK:
            EXAMPLE_TRACE("publish nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_RECVEIVED:
            EXAMPLE_TRACE("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                          topic_info->topic_len,
                          topic_info->ptopic,
                          topic_info->payload_len,
                          topic_info->payload);
            break;

        default:
            EXAMPLE_TRACE("Should NOT arrive here.");
            break;
    }
}

static int current_stat = 0;
#define STAT_ON "{\"mystate\":\"on\"}"
static void _demo_message_arrive(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    int result;
    char cmd[1024];
    char payload[256];
    iotx_mqtt_topic_info_pt ptopic_info = (iotx_mqtt_topic_info_pt) msg->msg;

    /* print topic name and topic message */
    EXAMPLE_TRACE("----");
    EXAMPLE_TRACE("Topic: '%.*s' (Length: %d)",
                  ptopic_info->topic_len,
                  ptopic_info->ptopic,
                  ptopic_info->topic_len);
    EXAMPLE_TRACE("Payload: '%.*s' (Length: %d)",
                  ptopic_info->payload_len,
                  ptopic_info->payload,
                  ptopic_info->payload_len);
    EXAMPLE_TRACE("Current State: '%s'",
		  current_stat == 0?"OFF":"ON");
    EXAMPLE_TRACE("----");


	memset(payload, 0, 256);
	memcpy(payload, ptopic_info->payload, ptopic_info->payload_len);
	result = strcmp(payload, STAT_ON);
	if (result == 0) {
		if (current_stat == 0) {
			int ret = 0;

			sprintf(cmd, "mqtt-perform.sh %s", "on");
			current_stat = 1;
			ret = system(cmd);
			printf("Perform: %s, result: %d, Power on the lamp!\n", cmd, ret);	
		}
	} else if (current_stat == 1) {
		int ret = 0;

		sprintf(cmd, "mqtt-perform.sh %s", "off");
		current_stat = 0;
		ret = system(cmd);
		printf("Perform: %s, result: %d, Power off the lamp!\n", cmd, ret);	
	}
	printf("\n");
}

int mqtt_client(void)
{
    int rc = 0, /* msg_len,*/ cnt = 0;
    void *pclient;
    iotx_conn_info_pt pconn_info;
    iotx_mqtt_param_t mqtt_params;
    char *msg_buf = NULL, *msg_readbuf = NULL;
    char *device_name = iot_dev[0].dev_name;
    char *device_secret = iot_dev[0].dev_secret;
    char topic[256];

    if (NULL == (msg_buf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    if (NULL == (msg_readbuf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    /* Device AUTH */
    if (0 != IOT_SetupConnInfo(product_key, device_name, device_secret, (void **)&pconn_info)) {
        EXAMPLE_TRACE("AUTH request failed!");
        rc = -1;
        goto do_exit;
    }

    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = pconn_info->port;
    mqtt_params.host = pconn_info->host_name;
    mqtt_params.client_id = pconn_info->client_id;
    mqtt_params.username = pconn_info->username;
    mqtt_params.password = pconn_info->password;
    mqtt_params.pub_key = pconn_info->pub_key;

    mqtt_params.request_timeout_ms = 2000;
    mqtt_params.clean_session = 0;
    mqtt_params.keepalive_interval_ms = 60000;
    mqtt_params.pread_buf = msg_readbuf;
    mqtt_params.read_buf_size = MSG_LEN_MAX;
    mqtt_params.pwrite_buf = msg_buf;
    mqtt_params.write_buf_size = MSG_LEN_MAX;

    mqtt_params.handle_event.h_fp = event_handle;
    mqtt_params.handle_event.pcontext = NULL;


    /* Construct a MQTT client with specify parameter */
    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (NULL == pclient) {
        EXAMPLE_TRACE("MQTT construct failed");
        rc = -1;
        goto do_exit;
    }

    TOPIC_DATA(topic, product_key, device_name);
    /* Subscribe the specific topic */
    rc = IOT_MQTT_Subscribe(pclient, topic, IOTX_MQTT_QOS1, _demo_message_arrive, NULL);
    if (rc < 0) {
        IOT_MQTT_Destroy(&pclient);
        EXAMPLE_TRACE("IOT_MQTT_Subscribe() failed, rc = %d", rc);
        rc = -1;
        goto do_exit;
    }

    HAL_SleepMs(1000);

    do {
        cnt++;
        /* handle the MQTT packet received from TCP or SSL connection */
        IOT_MQTT_Yield(pclient, 200);

        /* infinite loop if running with 'loop' argument */
        if (user_argc >= 2 && !strcmp("loop", user_argv[1])) {
            HAL_SleepMs(2000);
            cnt = 0;
        }

    } while (cnt < 1);

    IOT_MQTT_Unsubscribe(pclient, topic);

    HAL_SleepMs(200);

    IOT_MQTT_Destroy(&pclient);

do_exit:
    if (NULL != msg_buf) {
        HAL_Free(msg_buf);
    }

    if (NULL != msg_readbuf) {
        HAL_Free(msg_readbuf);
    }

    return rc;
}

int main(int argc, char **argv)
{
    int i;

    IOT_OpenLog("mqtt");
    IOT_SetLogLevel(IOT_LOG_DEBUG);

    user_argc = argc;
    user_argv = argv;

    json_dev_from_file(DEV_JSON_FILE);
    if (!iot_dev_num) {
	EXAMPLE_TRACE("Can NOT get any device!");
	if (product_key != NULL) {
	    LITE_free(product_key);
	}
	IOT_DumpMemoryStats(IOT_LOG_DEBUG);
	IOT_CloseLog();

	EXAMPLE_TRACE("out of sample!");
	return 0;
    }

    mqtt_client();

    if (product_key != NULL) {
	LITE_free(product_key);
    }

    for (i = 0; i < iot_dev_num; i++) {
	LITE_free(iot_dev[i].dev_name);
	LITE_free(iot_dev[i].dev_secret);
	iot_dev[i].dev_name = NULL;
	iot_dev[i].dev_secret = NULL;
    }

    IOT_DumpMemoryStats(IOT_LOG_DEBUG);
    IOT_CloseLog();

    EXAMPLE_TRACE("out of sample!");
    return 0;
}
