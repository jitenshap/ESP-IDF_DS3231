#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ds3231.h>
#include "esp_log.h"
#define SDA_GPIO 21
#define SCL_GPIO 22


QueueHandle_t rtcQueue;             //キュー制御用ハンドル
static const char *TAG = "MAIN";    //ESP_LOGIとかやるときの出力につけるヘッダ

/*  rtc状態リスト   */
typedef enum rtcTaskQueue_t
{
    RTC_NONE = 0,       //何もしない予定
    RTC_GET_TIME = 1,
    RTC_GET_TEMP = 2
}rtcTaskQueue_t;

typedef struct _rtcTaskArg_t
{
    rtcTaskQueue_t rtcQueue;
    struct tm * timeNow_p;
    float * temperature;
} RTCTaskArg_t;

void ds3231_test(void *pvParameters)
{
    i2c_dev_t dev;

    while (i2cdev_init() != ESP_OK)
    {
        printf("Could not init I2Cdev library\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    while (ds3231_init_desc(&dev, 0, SDA_GPIO, SCL_GPIO) != ESP_OK)
    {
        printf("Could not init device descriptor\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    // setup datetime: 2019-05-18 21:10:00
    struct tm time = 
    {
        .tm_year = 2019,
        .tm_mon  = 4,  // 0-based
        .tm_mday = 18,
        .tm_hour = 21,
        .tm_min  = 10,
        .tm_sec  = 0
    };
    while (ds3231_set_time(&dev, &time) != ESP_OK)
    {
        printf("Could not set time\n");
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    while (1)
    {
        BaseType_t xStatus;
        RTCTaskArg_t rtcArg;
        rtcArg.rtcQueue = RTC_NONE;
        //キュー受信。受け取るまで portMAX_DELAY:無限に待つ
        xStatus = xQueueReceive(rtcQueue, &rtcArg, portMAX_DELAY);
        if(xStatus == pdPASS)   //キュー受け取ったら
        {
            ESP_LOGI(TAG, "received: %d", rtcArg.rtcQueue);
            /*  キューの引数によってピンを駆動  */
            switch(rtcArg.rtcQueue)
            {
                case RTC_GET_TIME:
                    while(ds3231_get_time(&dev, rtcArg.timeNow_p) != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Could not get time");
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                    }
                    break;
                case RTC_GET_TEMP:
                    while(ds3231_get_temp_float(&dev, rtcArg.temperature) != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Could not get temperature");
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                    }
                    break;
                default:
                    break;
            }
            vTaskDelay(250 / portTICK_PERIOD_MS);   //連発でI2Cを爆撃しないように待ち入れる
        }
    }
}

void app_main()
{
    rtcQueue = xQueueCreate(1, sizeof(RTCTaskArg_t));  //キューハンドル初期化
    xTaskCreate(ds3231_test, "ds3231_test", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    RTCTaskArg_t rtcArg;
    BaseType_t xStatus;
    struct tm time;
    float temp = -111.1;
    rtcArg.temperature = &temp;
    rtcArg.timeNow_p = &time;
    while (1)
    {
        rtcArg.rtcQueue=RTC_GET_TEMP;
        xStatus = xQueueSend(rtcQueue, &rtcArg, 0);  //キュー送信
        if(xStatus != pdPASS) //キュー通ったかチェック
        {
            ESP_LOGE(TAG, "Queue send error");
        }
        else
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "Get temp success!! temp = %.2f", temp);
        }

        rtcArg.rtcQueue=RTC_GET_TIME;
        xStatus = xQueueSend(rtcQueue, &rtcArg, 0);  //キュー送信
        if(xStatus != pdPASS) //キュー通ったかチェック
        {
            ESP_LOGE(TAG, "Queue send error");
        }
        else
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "Get time success!! Now time = %04d-%02d-%02d %02d:%02d:%02d", time.tm_year, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);  //1sec待つ
    }
}