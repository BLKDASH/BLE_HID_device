#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "hid_dev.h"

#include "esp_timer.h"

#include "main.h"

#include "sdkconfig.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "led_strip.h"
#include "hardware_init.h"
#include "processing.h"
#include "calibration.h"

// todo:遗忘上一次连接的设备
// todo:断连后重新连接，会导致崩溃（adc 缓冲区无法读取）
// todo:把按键注册为 iot button
// todo:重新组织代码结构
// todo:开辟 EEPROM 空间

#define HID_TASK_TAG "TASKinfo"
// adc多通道均值缓冲区
// 结构（channel 对应索引）：
// #define ADC_CHANNEL_RIGHT_UP_DOWN ADC_CHANNEL_0    // 右上下 (GPIO36)
// #define ADC_CHANNEL_RIGHT_LEFT_RIGHT ADC_CHANNEL_1 // 右左右 (GPIO37)
// #define ADC_CHANNEL_LEFT_UP_DOWN ADC_CHANNEL_2     // 左上下 (GPIO38)
// #define ADC_CHANNEL_LEFT_LEFT_RIGHT ADC_CHANNEL_3  // 左左右 (GPIO39)
// #define ADC_CHANNEL_LEFT_TRIGGER ADC_CHANNEL_4     // 左板机 (GPIO32)
// #define ADC_CHANNEL_RIGHT_TRIGGER ADC_CHANNEL_5    // 右板机 (GPIO33)
// #define ADC_CHANNEL_BATTERY ADC_CHANNEL_6          // 电池电压 (GPIO34)
// #define ADC_CHANNEL_DPAD ADC_CHANNEL_7             // 十字键 (GPIO35)
MultiChannelBuffer *mcb = NULL;

// LED更新的信号量
SemaphoreHandle_t led_flash_semaphore = NULL;
// 关机信号量
SemaphoreHandle_t shutdown_semaphore = NULL;
// 校准信号量
SemaphoreHandle_t calibration_semaphore = NULL;

bool led_running = false;
bool adc_running = false;

// 摇杆校准数据
joystick_calibration_data_t left_joystick_cal_data;
joystick_calibration_data_t right_joystick_cal_data;

void app_main(void)
{
    // 第一步：拉高 powerKeep
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(GPIO_OUTPUT_POWER_KEEP_IO);
    io_conf.pull_down_en = false; // Disable pull-down
    io_conf.pull_up_en = false;   // Enable pull-up
    gpio_config(&io_conf);
    gpio_set_level(GPIO_OUTPUT_POWER_KEEP_IO, 1);

    // 使用 while1 是防止启动失败时，一次没关机成功
    while (1)
    {
        ESP_LOGW("main", "Into MAIN while");
        if (ESP_OK == START_UP()) // DEBUG模式下，始终返回ESP_OK
        {
            ESP_LOGI("main", "START_UP OK");

            // 创建多通道平均缓冲区，长度为10
            mcb = mcb_init(10);
            init_all(); // 初始化除了HOME按键之外的外设
            // 读取开机次数
            uint64_t boot_count;
            esp_err_t err = nvs_get_boot_count(&boot_count);
            if (err == ESP_OK)
            {
                ESP_LOGI("main", "系统启动完成，当前是第%llu次开机", boot_count);
            }

            calibration_semaphore = xSemaphoreCreateBinary();
            read_joystick_calibration_data(0, &left_joystick_cal_data);  // 左摇杆
            read_joystick_calibration_data(1, &right_joystick_cal_data); // 右摇杆

            // LED任务
            led_flash_semaphore = xSemaphoreCreateBinary();
            xTaskCreatePinnedToCore(blink_task, "blink_task", 2048, NULL, 5, NULL, 1);
            xTaskCreatePinnedToCore(LED_flash_task, "LED_flash_task", 2048, NULL, 5, NULL, 1);
            // 先闪灯，让用户以为开机了
            while (gpio_get_level(GPIO_INPUT_HOME_BTN) == BUTTON_HOME_PRESSED)
            {
                vTaskDelay(pdMS_TO_TICKS(100));
            } // 让出时间给LED任务} // 等待按键释放
            ESP_LOGI("main", "register home button--");
            // 创建关机任务
            shutdown_semaphore = xSemaphoreCreateBinary();
            setHomeButton(); // 释放后再注册home按键长按
            xTaskCreatePinnedToCore(shutdown_task, "shutdown_task", 4096, NULL, 5, NULL, 1);

            // 创建摇杆校准任务
            xTaskCreatePinnedToCore(joystick_calibration_task, "calibration_task", 4096, NULL, 5, NULL, 1);

            while (1)
            {
                // 等待连接
                if (sec_conn == true)
                {
                    // （排查这里的缓存读取错误）
                    //  todo:为了保证任务的合理运行，此处不应该在创建任务前进行判断 sec_conn == true
                    //  GPIO与ADC读取任务
                    //  xTaskCreatePinnedToCore(gpio_read_task, "gpio_toggle_task", 4096, NULL, 6, NULL, 1);
                    //  高优先级保证实时性
                    xTaskCreatePinnedToCore(adc_read_task, "adc_read_task", 4096, NULL, 7, NULL, 1);
                    // 可以是低优先级，反正每次处理的都是最新数据
                    xTaskCreatePinnedToCore(adc_aver_send, "adc_aver_send", 2048, NULL, 6, NULL, 1);
                    // 模拟手柄任务
                    xTaskCreatePinnedToCore(gamepad_button_task, "gamepad_button_task", 4096, NULL, 9, NULL, 1);
                    // 使命完成，删除自己
                    vTaskDelete(NULL);
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
            }
        }
        else
        {
            ESP_LOGI("STARTUP", "startup fail");
            SLEEP();
        }
    }
}

esp_err_t START_UP(void)
{
    // 配置home按键下拉输入
    gpio_config_t home_btn_conf = {};
    home_btn_conf.intr_type = GPIO_INTR_DISABLE;
    home_btn_conf.mode = GPIO_MODE_INPUT;
    home_btn_conf.pin_bit_mask = BIT64(GPIO_INPUT_HOME_BTN);
    home_btn_conf.pull_down_en = true; // 下拉
    home_btn_conf.pull_up_en = false;
    gpio_config(&home_btn_conf);

    // 阻塞方式检测按键是否持续高电平3秒
    int64_t start_time = esp_timer_get_time(); // 获取起始时间(微秒)
    int64_t required_duration = 1000000;       // 1秒 = 1,000,000微秒
    while (true)
    {
        // 读取当前按键状态
        int level = gpio_get_level(GPIO_INPUT_HOME_BTN);

        // 如果按键为低电平，说明没有按下
        if (level == BUTTON_HOME_RELEASED)
        {
            return ESP_FAIL;
        }
        // 如果按键持续高电平达到3秒，返回成功
        else if (esp_timer_get_time() - start_time >= required_duration)
        {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 回调函数中不能有 TaskDelay 等阻塞的操作
static void button_long_press_home_cb(void *arg, void *usr_data)
{
    ESP_LOGW("button_cb", "HOME_BUTTON_LONG_PRESS");
    // 发送关机信号量
    xSemaphoreGive(shutdown_semaphore);
    // current_device_state = DEVICE_STATE_SLEEP;
}

esp_err_t setHomeButton(void)
{
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = GPIO_INPUT_HOME_BTN,
        .active_level = 1,
    };
    button_handle_t gpio_btn = NULL;
    iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn);
    // 设置属性
    button_event_args_t args = {
        .long_press.press_time = 2000,
    };
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_START, &args, button_long_press_home_cb, NULL);
    return ESP_OK;
}

void SLEEP(void)
{
    current_device_state = DEVICE_STATE_SLEEP;
    ESP_LOGI("SLEEP", "Sleeping...");
    // setLED函数同时会影响IO12单LED的初始化
    vTaskDelay(pdMS_TO_TICKS(50));
    while (gpio_get_level(GPIO_INPUT_HOME_BTN) == BUTTON_HOME_PRESSED)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    } // 等待按键释放

    // 不要做这些操作，直接关机即可。这些操作的导致的延时后果不确定
    // esp_bluedroid_disable();
    // esp_bluedroid_deinit();
    // esp_bt_controller_disable();
    // esp_bt_controller_deinit();
    // 关闭adc
    // adc_continuous_deinit(ADC_init_handle);

    // 下拉输出powerkeep0，拉低电源保持，阻塞式不断执行直到电源断电为止
    while (1)
    {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = BIT64(GPIO_OUTPUT_POWER_KEEP_IO);
        io_conf.pull_down_en = true; // Disable pull-down
        io_conf.pull_up_en = false;  // Enable pull-up
        gpio_config(&io_conf);

        gpio_set_level(GPIO_OUTPUT_POWER_KEEP_IO, 0);
    }
}

// -------------------------------------------------------------------- TASK -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// 关机任务
void shutdown_task(void *pvParameter)
{
    while (1)
    {
        // 判断关机信号量
        if (xSemaphoreTake(shutdown_semaphore, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI("SHUTDOWN", "Shutdown signal received");
            // 假如一直按住，则松开才执行后面的操作
            while (gpio_get_level(GPIO_INPUT_HOME_BTN) == BUTTON_HOME_PRESSED)
            {
                current_device_state = DEVICE_STATE_SLEEP;
                vTaskDelay(10);
            }
            SLEEP();
        }
    }
}

void blink_task(void *pvParameter)
{
    // 记录 LED 亮灭的标志位
    bool led_on_off = true;
    // 记录 LED 环闪的索引
    int led_ring = 0;
    for (int i = 0; i < 4; i++)
    {
        setLED(i, 0, 0, 0);
    }
    led_running = true;
    while (led_running)
    {
        switch (current_device_state)
        {
        case DEVICE_STATE_INIT:
            setLED(0, 0, led_on_off ? 10 : 0, led_on_off ? 10 : 0);
            setLED(1, 0, 0, 0);
            setLED(2, 0, 0, 0);
            setLED(3, led_on_off ? 10 : 0, led_on_off ? 10 : 0, 0);
            led_on_off = !led_on_off;
            vTaskDelay(pdMS_TO_TICKS(300));
            break;

        case DEVICE_STATE_ADVERTISING:
            setLED(0, 0, 0, led_on_off ? 15 : 0);
            setLED(1, 0, 0, led_on_off ? 0 : 15);
            setLED(2, 0, 0, led_on_off ? 15 : 0);
            setLED(3, 0, 0, led_on_off ? 0 : 15);
            led_on_off = !led_on_off;
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case DEVICE_STATE_CONNECTED:
            // 常亮绿色
            for (int i = 0; i < 4; i++)
            {
                setLED(i, 0, 15, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case DEVICE_STATE_DISCONNECTED:
            // 慢闪 (500ms间隔)
            uint8_t red = led_on_off ? 15 : 0;
            uint8_t green = 0;
            uint8_t blue = 0;

            setLED(0, led_on_off ? red : 0, green, blue);
            setLED(1, led_on_off ? 0 : red, green, blue);
            setLED(2, led_on_off ? red : 0, green, blue);
            setLED(3, led_on_off ? 0 : red, green, blue);

            led_on_off = !led_on_off;
            vTaskDelay(pdMS_TO_TICKS(500));
            break;

        case DEVICE_STATE_ERROR:
            uint8_t r = led_on_off ? 20 : 0;
            uint8_t g = 0;
            uint8_t b = 0;
            for (int i = 0; i < 4; i++)
            {
                setLED(i, r, g, b);
            }

            led_on_off = !led_on_off;
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case DEVICE_STATE_SLEEP:
            for (int i = 0; i < 4; i++)
            {
                setLED(i, 0, 0, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case DEVICE_STATE_CALI_START:
            for (int i = 0; i < 4; i++)
            {
                if (i == 0 && led_on_off)
                {
                    setLED(i, 20, 10, 5);
                }
                else
                {
                    setLED(i, 0, 0, 0);
                }
            }
            led_on_off = !led_on_off; // 切换亮暗状态
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case DEVICE_STATE_CALI_RING:
            // 循环闪烁
            for (int i = 0; i < 4; i++)
            {
                if (i == led_ring)
                {
                    setLED(i, 5, 5, 5); // 点亮当前LED
                }
                else
                {
                    setLED(i, 0, 0, 0); // 熄灭其他LED
                }
            }
            // 更新下一个要点亮的LED索引（循环0-3）
            led_ring = (led_ring + 1) % 4;
            // 延迟100ms
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case DEVICE_STATE_CALI_DONE:
            // 快闪灯0
            setLED(0, led_on_off ? 20 : 0, led_on_off ? 10 : 0, led_on_off ? 5 : 0);
            for (int i = 1; i < 4; i++)
            {
                setLED(i, 0, 0, 0);
            }
            led_on_off = !led_on_off;
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        default:
            for (int i = 0; i < 4; i++)
            {
                setLED(i, 0, 0, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        }
        // 每次更新 led 后，更新刷新信号量
        if (led_flash_semaphore != NULL)
        {
            xSemaphoreGive(led_flash_semaphore);
            // ESP_LOGI("main", "LED Semaphore Give!");
        }
    }
    // 销毁任务
    vTaskDelete(NULL);
}

void LED_flash_task(void *pvParameter)
{
    // 这里不要去判断 led runing，万一来不及赋值，该任务就会被删除
    while (1)
    {
        // 等待LED操作完成，一直阻塞直到信号量被释放
        if (xSemaphoreTake(led_flash_semaphore, portMAX_DELAY) == pdTRUE)
        {
            flashLED();
        }
    }
    vTaskDelete(NULL);
}

void gpio_read_task(void *pvParameter)
{
    int level_25, level_26, level_27, level_14;
    int level_15, level_19;
    int level_23, level_18;
    int level_4, level_2, level_0, level_21, level_22;

    while (1)
    {
        // Toggle GPIO32

        // Read input GPIO levels
        level_25 = gpio_get_level(GPIO_INPUT_KEY_X);
        level_26 = gpio_get_level(GPIO_INPUT_KEY_Y);
        level_27 = gpio_get_level(GPIO_INPUT_KEY_A);
        level_14 = gpio_get_level(GPIO_INPUT_KEY_B);

        level_15 = gpio_get_level(GPIO_INPUT_LEFT_JOYSTICK_BTN);
        level_19 = gpio_get_level(GPIO_INPUT_RIGHT_JOYSTICK_BTN);

        level_23 = gpio_get_level(GPIO_INPUT_LEFT_SHOULDER_BTN);
        level_18 = gpio_get_level(GPIO_INPUT_RIGHT_SHOULDER_BTN);

        level_4 = gpio_get_level(GPIO_INPUT_SELECT_BTN);
        level_2 = gpio_get_level(GPIO_INPUT_START_BTN);
        level_0 = gpio_get_level(GPIO_INPUT_IKEY_BTN);
        level_21 = gpio_get_level(GPIO_INPUT_IOS_BTN);
        level_22 = gpio_get_level(GPIO_INPUT_WINDOWS_BTN);

        // Log levels
        ESP_LOGI("ioTask", "X: %d | Y: %d | A: %d | B: %d",
                 level_25, level_26, level_27, level_14);
        ESP_LOGI("ioTask", "LS: %d | RS: %d",
                 level_15, level_19);
        ESP_LOGI("ioTask", "Shoulders - Left: %d | Right: %d",
                 level_23, level_18);
        ESP_LOGI("ioTask", "Special Keys - SELECT: %d | START: %d | IKEY: %d | IOS: %d | WINDOWS: %d",
                 level_4, level_2, level_0, level_21, level_22);

        // Delay 500ms
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

static TaskHandle_t s_task_handle;
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    // 在中断中通知任务 ADC 已经完成足够次数的转换
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
    // 如果转换完成，进行上下文切换（让步）
    return (mustYield == pdTRUE);
}

// adc 读取raw任务
void adc_read_task(void *pvParameter)
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t bufferADC[EXAMPLE_READ_LEN] = {0};
    memset(bufferADC, 0xcc, EXAMPLE_READ_LEN);
    s_task_handle = xTaskGetCurrentTaskHandle();
    // 错误检查回调
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(ADC_init_handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(ADC_init_handle));

    adc_running = true;

    // 连上了才读，否则会这是一个ESP32的Cache error错误，具体原因是"Cache disabled but cached memory region accessed"（缓存被禁用但访问了缓存内存区域）。从回溯信息看，错误发生在BLE连接过程中，当尝试读取ADC数据时触发。触发时机：在BLE连接事件(ESP_HIDD_EVENT_BLE_CONNECT)处理过程中。根本原因：中断处理程序中访问了被缓存的内存区域，而此时缓存已被禁用
    // 增加额外的状态稳定检查，确保连接完全稳定后再读取ADC
    bool conn_stable = false;
    uint32_t conn_time = 0;

    while (adc_running)
    {
        // 只有在已连接状态下才读取ADC数据
        if (sec_conn)
        {
            // 检查连接是否稳定，避免在连接刚建立时就读取ADC数据
            if (!conn_stable)
            {
                if (conn_time == 0)
                {
                    conn_time = xTaskGetTickCount();
                }
                else if ((xTaskGetTickCount() - conn_time) > pdMS_TO_TICKS(100))
                {
                    // 连接稳定超过100ms，可以安全读取ADC
                    conn_stable = true;
                    conn_time = 0;
                }
            }

            if (conn_stable)
            {
                vTaskDelay(pdMS_TO_TICKS(20));
                // 读取256个数据
                ret = adc_continuous_read(ADC_init_handle, bufferADC, EXAMPLE_READ_LEN, &ret_num, 0);
                if (ret == ESP_OK)
                {
                    // ESP_LOGI("TASK", "ret is %x, ret_num is %" PRIu32 " bytes", ret, ret_num);
                    // 每个ADC采样结果包含多个字节（通常是4字节），包含了通道号和采样值等信息。
                    for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
                    {
                        adc_digi_output_data_t *p = (adc_digi_output_data_t *)&bufferADC[i];
                        uint8_t chan_num = (uint8_t)EXAMPLE_ADC_GET_CHANNEL(p);
                        uint32_t data = EXAMPLE_ADC_GET_DATA(p);
                        // float voltage = (float)data * 3.3 / 4095.0;
                        // 推入 mcb
                        mcb_push(mcb, chan_num, data);
                    }
                }
                else if (ret != ESP_ERR_TIMEOUT)
                {
                    ESP_LOGE("ADCtask", "ADC read failed with error: %d", ret);
                }
            }
            else
            {
                // 连接尚未稳定，短暂延迟
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        else
        {
            // 如果未连接，则短暂延迟
            conn_stable = false; // 重置连接稳定状态
            conn_time = 0;       // 重置连接时间
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    vTaskDelete(NULL);
}

// 摇杆校准数据
// joystick_calibration_data_t left_joystick_cal_data;
// joystick_calibration_data_t right_joystick_cal_data;
void adc_aver_send(void *pvParameters)
{
    uint32_t all_avg[8];
    while (1)
    {
        // 当正在进行环形校准时不要进行此操作，防止耗时
        if (current_device_state != DEVICE_STATE_CALI_RING)
        {
            mcb_get_all_averages(mcb, all_avg);
            // 此处可以直接认为，平均后的值为 ADC 的原始数据
            printf("\r\n");
            for (uint8_t i = 0; i < 8; i++)
            {
                printf("%ld  ", all_avg[i]);
            }
            vTaskDelay(pdMS_TO_TICKS(40));
        }
    }
}

void gamepad_button_task(void *pvParameters)
{

    while (1)
    {
        if (sec_conn)
        {
            vTaskDelay(pdMS_TO_TICKS(80));
            esp_hidd_send_gamepad_report(hid_conn_id);
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10000));
            ESP_LOGI(HID_TASK_TAG, "Waiting for connection...");
        }
    }
}

void joystick_calibration_task(void *pvParameter)
{
    while (1)
    {
        if (xSemaphoreTake(calibration_semaphore, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI("CALIBRATION", "开始摇杆校准");
            // 保存当前设备状态，以便校准结束后恢复
            // device_state_t prev_state = current_device_state;
            current_device_state = DEVICE_STATE_CALI_START;
            vTaskDelay(pdMS_TO_TICKS(1000));
            current_device_state = DEVICE_STATE_CALI_RING;

            // 初始化最大最小值
            uint32_t max_values[4] = {0, 0, 0, 0};             // 通道0,1,2,3的最大值
            uint32_t min_values[4] = {4095, 4095, 4095, 4095}; // 通道0,1,2,3的最小值

            uint32_t all_avg[8];
            uint32_t start_time = xTaskGetTickCount();
            uint32_t duration_ticks = pdMS_TO_TICKS(5000); // 5秒

            // 持续读取5秒数据
            while ((xTaskGetTickCount() - start_time) < duration_ticks)
            {
                mcb_get_all_averages(mcb, all_avg);

                // 更新通道0-3的最大最小值
                for (int i = 0; i < 4; i++)
                {
                    if (all_avg[i] > max_values[i])
                    {
                        max_values[i] = all_avg[i];
                    }
                    if (all_avg[i] < min_values[i])
                    {
                        min_values[i] = all_avg[i];
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(10)); // 稍微延时以避免占用过多CPU
            }

            // 保存校准数据
            // all_avg[0] 对应右摇杆Y轴 (max -> max_y, min -> min_y)
            right_joystick_cal_data.max_y = max_values[0];
            right_joystick_cal_data.min_y = min_values[0];

            // all_avg[1] 对应右摇杆X轴 (max -> max_x, min -> min_x)
            right_joystick_cal_data.max_x = max_values[1];
            right_joystick_cal_data.min_x = min_values[1];

            // all_avg[2] 对应左摇杆Y轴 (max -> max_y, min -> min_y)
            left_joystick_cal_data.max_y = max_values[2];
            left_joystick_cal_data.min_y = min_values[2];

            // all_avg[3] 对应左摇杆X轴 (max -> max_x, min -> min_x)
            left_joystick_cal_data.max_x = max_values[3];
            left_joystick_cal_data.min_x = min_values[3];

            // 存储校准数据到NVS
            store_joystick_calibration_data(1, &right_joystick_cal_data); // 右摇杆ID=1
            store_joystick_calibration_data(0, &left_joystick_cal_data);  // 左摇杆ID=0

            current_device_state = DEVICE_STATE_CALI_DONE;
            vTaskDelay(pdMS_TO_TICKS(1000));
            // 关灯
            current_device_state = DEVICE_STATE_SLEEP;
            // 等待用户松手
            vTaskDelay(pdMS_TO_TICKS(1500));

            // 获取稳定值作为中心点
            uint32_t center_sum[4] = {0, 0, 0, 0}; // 用于累加各通道值
            uint32_t center_count = 0;             // 采样次数

            start_time = xTaskGetTickCount();
            duration_ticks = pdMS_TO_TICKS(3000); // 3秒
            while ((xTaskGetTickCount() - start_time) < duration_ticks)
            {
                mcb_get_all_averages(mcb, all_avg);

                // 累加各通道值
                for (int i = 0; i < 4; i++)
                {
                    center_sum[i] += all_avg[i];
                }
                center_count++;

                vTaskDelay(pdMS_TO_TICKS(10)); // 稍微延时以避免占用过多CPU
            }

            // 计算平均值作为中心点
            if (center_count > 0)
            {
                // all_avg[0] 对应右摇杆Y轴 -> center_y
                right_joystick_cal_data.center_y = center_sum[0] / center_count;

                // all_avg[1] 对应右摇杆X轴 -> center_x
                right_joystick_cal_data.center_x = center_sum[1] / center_count;

                // all_avg[2] 对应左摇杆Y轴 -> center_y
                left_joystick_cal_data.center_y = center_sum[2] / center_count;

                // all_avg[3] 对应左摇杆X轴 -> center_x
                left_joystick_cal_data.center_x = center_sum[3] / center_count;

                // 更新存储到NVS中的校准数据
                store_joystick_calibration_data(1, &right_joystick_cal_data); // 右摇杆ID=1
                store_joystick_calibration_data(0, &left_joystick_cal_data);  // 左摇杆ID=0

                ESP_LOGI("CALIBRATION", "右摇杆中心点: X=%lu, Y=%lu",
                         right_joystick_cal_data.center_x, right_joystick_cal_data.center_y);
                ESP_LOGI("CALIBRATION", "左摇杆中心点: X=%lu, Y=%lu",
                         left_joystick_cal_data.center_x, left_joystick_cal_data.center_y);
            }

            xSemaphoreGive(calibration_semaphore); // 归还信号量，等待下一次校准
        }
    }
}