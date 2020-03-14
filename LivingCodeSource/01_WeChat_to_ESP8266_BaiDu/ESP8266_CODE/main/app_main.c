/*
 * @Author: xuhongv
 * @Date: 2020-03-13 18:09:56
 * @LastEditTime: 2020-03-14 17:43:32
 * @LastEditors: Please set LastEditors
 * @Description:  微信小程序连接百度天工服务器控制ESP8266的服务器的   ESP8266代码
 * @FilePath: /ESP8266_RTOS_SDK/mycode/tcp/main/app_main.c
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_EXAMPLE";

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id)
    {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, "/iot/device/topic", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        //  msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        //   ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

      

        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        break;
    }
    return ESP_OK;
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle = mqtt_event_handler,
        .host = "3h222a6.mqtt.iot.gz.baidubce.com",
        .username = "3h222a6/iot_8266",
        .password = "LXSFzu50zI5ezBrl",
        .port = 1883,
        .keepalive = 200,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

void TaskGpio(void *p)
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = (1ULL << 15);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    vTaskDelete(NULL);
}
/* 
 * @Description: 程序入口
 * @return:  null
*/
void app_main()
{

    ESP_ERROR_CHECK(nvs_flash_init());

    //gpio init
    xTaskCreate(TaskGpio, "TaskGpio", 1024, NULL, 10, NULL);

    ESP_ERROR_CHECK(esp_netif_init());
    //通过面板设置 要连接的路由器账号和密码
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
}
