/*
 * @Author: xuhongv
 * @Date: 2020-03-13 18:09:56
 * @LastEditTime: 2020-03-15 15:04:42
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
#include "driver/pwm.h"
#include "cJSON.h"

static const char *TAG = "AiThinkerMqtt";

#define MQTT_DATA_PUBLISH "/light/deviceOut" //设备发送消息的主题
#define MQTT_DATA_SUBLISH "/light/deviceIn" //设备订阅消息的主题

//PWM 周期 100us(也就是10Khz)
#define PWM_PERIOD (100)
//pwm gpio口配置
#define CHANNLE_PWM_TOTAL 1 //一共1个通道
#define CHANNLE_PWM 0
#define PWM_OUT_IO_NUM 2 //灯管脚 也就是我们NodeMCU的蓝灯
// pwm pin number
const uint32_t pinNum[CHANNLE_PWM_TOTAL] = {PWM_OUT_IO_NUM};
// don't alter it !!! dutys table, (duty/PERIOD)*depth , init
uint32_t setDuties[CHANNLE_PWM_TOTAL] = {50};
//相位设置，不懂的或者不需要的全部为0即可
int16_t phase[CHANNLE_PWM_TOTAL] = {
    0,
};
//mqtt
static esp_mqtt_client_handle_t client;
static void post_data_to_clouds();


//设置pwm方法
static void pwm_set_start(uint8_t duty)
{
    pwm_set_duty(CHANNLE_PWM, duty);
    pwm_start();
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
     client = event->client;
    int msg_id;
    switch (event->event_id)
    {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, MQTT_DATA_SUBLISH, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        {
            ////首先整体判断是否为一个json格式的数据
            cJSON *pJsonRoot = cJSON_Parse(event->data);

            //如果是否json格式数据
            if (pJsonRoot == NULL)
            {
                break;
            }

            cJSON *pChange = cJSON_GetObjectItem(pJsonRoot, "change");
            cJSON *pValue = cJSON_GetObjectItem(pJsonRoot, "value");

            //判断字段是否pChange格式
            if (pChange && pValue)
            {
                //用来打印服务器的topic主题
                ESP_LOGI(TAG, "xQueueReceive topic: %.*s ", event->topic_len, event->topic);
                //打印用于接收服务器的json数据
                ESP_LOGI(TAG, "xQueueReceive payload: %.*s",event->data_len, event->data);
                
                ESP_LOGI(TAG, "esp_get_free_heap_size : %d \n", esp_get_free_heap_size());

                //判断字段是否string类型
                if (cJSON_IsString(pChange))
                    printf("get pChange:%s \n", pChange->valuestring);
                else
                    break;

                //获取最新的状态，对应的获取温湿度按钮
                if (strcmp(pChange->valuestring, "query") == 0)
                {
                }
                //收到服务器的开关灯指令
                else if (strcmp(pChange->valuestring, "power") == 0)
                {
                    //开灯
                    if (strcmp(pValue->valuestring, "true") == 0)
                    {
                        pwm_set_start(100);
                    }
                    //关灯
                    else
                    {
                        pwm_set_start(0);
                    }
                }
                //收到服务器的调节亮度灯指令
                else if (strcmp(pChange->valuestring, "pwm") == 0)
                {

                    pwm_set_start(pValue->valueint);
                    // 主动发送数组到串口
                }
                //每次下发成功控制都要主动上报给服务器
                post_data_to_clouds();
            }
            else
                printf("get pChange failed \n");
            //删除json，释放内存
            cJSON_Delete(pJsonRoot);
            break;
        }

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        break;
    }
    return ESP_OK;
}

/**
 * @description: 上报数据给服务器
 * @param {type} 
 * @return: 
 */
static void post_data_to_clouds()
{

    cJSON *pRoot = cJSON_CreateObject();

    uint32_t duty_p = 0;
    //获取当前的pwm输出百分比
    if (pwm_get_duty(CHANNLE_PWM, &duty_p) != ESP_OK)
    {
        printf("Error in getting period...\n\n");
    }
    //是否为0,否则就是开灯状态！
    if (duty_p != 0)
        cJSON_AddBoolToObject(pRoot, "power", true);
    else
        cJSON_AddBoolToObject(pRoot, "power", false);
    //上报pwm百分比，作为亮度参数给服务器
    cJSON_AddNumberToObject(pRoot, "brightNess", duty_p);
    //格式化为字符串
    char *s = cJSON_Print(pRoot);
    //发布消息
    esp_mqtt_client_publish(client, MQTT_DATA_PUBLISH, s, strlen(s), 1, 0);
    //删除json结构体，释放内存
    cJSON_free((void *)s);
    cJSON_Delete(pRoot);
}


static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle = mqtt_event_handler,
        .host = "xxxxxx.mqtt.iot.gz.baidubce.com",
        .username = "xxxxx/device",
        .password = "xxxxxzBrl",
        .port = 1883,
        .keepalive = 200,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

void TaskGpio(void *p)
{
    pwm_init(PWM_PERIOD, setDuties, CHANNLE_PWM_TOTAL, pinNum);
    //设置相位：具体有什么用? 访问了解 https://blog.csdn.net/xh870189248/article/details/88526251#PWM_143
    pwm_set_phases(phase);
    //我司安信可出品的 NodeMCU的蓝灯是低电平有效，设置反相低电平有效
    pwm_set_channel_invert(1ULL<< 0);
    //设置50%亮度
    pwm_set_start(50);

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

