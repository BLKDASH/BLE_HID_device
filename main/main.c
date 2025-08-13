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
#include "esp_bt_main.h"
#include "esp_bt.h"

#include "main.h"

#include "sdkconfig.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "led_strip.h"
#include "hardware_init.h"
#include "processing.h"
#include "calibration.h"

#define GAMEPAD_DEBUG_MODE

// todo:遗忘上一次连接的设备
// todo:优化发包速度
// todo：添加定时器唤醒
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
bool js_calibration_running = false;

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

            // stop_adc_sampling();

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
            xTaskCreatePinnedToCore(blink_task, "blink_task", 4096, NULL, 5, NULL, 1);
            xTaskCreatePinnedToCore(LED_flash_task, "LED_flash_task", 4096, NULL, 5, NULL, 1);
            // 先闪灯，让用户以为开机了
            while (gpio_get_level(GPIO_INPUT_HOME_BTN) == BUTTON_HOME_PRESSED)
            {
                vTaskDelay(pdMS_TO_TICKS(100));
            } // 让出时间给LED任务} // 等待按键释放
            ESP_LOGI("main", "register home button--");
            // 创建关机任务
            shutdown_semaphore = xSemaphoreCreateBinary();
            setHomeButton(); // 释放后再注册home按键长按
            xTaskCreatePinnedToCore(shutdown_task, "shutdown_task", 2048, NULL, 7, NULL, 1);

            // 创建摇杆校准任务
            xTaskCreatePinnedToCore(joystick_calibration_task, "calibration_task", 8192, NULL, 10, NULL, 1);

            // 创建XYAB按键状态监控任务
            xTaskCreatePinnedToCore(all_buttons_monitor_task, "xyab_button_monitor", 2048, NULL, 7, NULL, 1);

            // （排查这里的缓存读取错误）
            //  高优先级保证实时性
            // 启动ADC采样

            xTaskCreatePinnedToCore(adc_read_task, "adc_read_task", 8192, NULL, 7, NULL, 1);
            // 可以是低优先级，反正每次处理的都是最新数据
            xTaskCreatePinnedToCore(adc_aver_send_task, "adc_aver_send_task", 2048, NULL, 6, NULL, 1);
            // 模拟手柄任务
            xTaskCreatePinnedToCore(gamepad_packet_send_task, "gamepad_packet_send_task", 4096, NULL, 8, NULL, 1);
            // 使命完成，删除自己
            vTaskDelete(NULL);
        }
        else
        {
            ESP_LOGI("STARTUP", "startup fail");
            vTaskDelay(pdMS_TO_TICKS(500));
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
    vTaskDelay(pdMS_TO_TICKS(50));
    while (gpio_get_level(GPIO_INPUT_HOME_BTN) == BUTTON_HOME_PRESSED)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    } // 等待按键释放

    // 不要做这些操作，直接关机即可。这些操作的导致的延时后果不确定
    // esp_bluedroid_disable();
    // esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    // 关闭adc
    // adc_continuous_deinit(ADC_init_handle);

    // 下拉输出powerkeep0，拉低电源保持。如果是电池状态，此时已经关断电源。如果是充电状态，那么下拉也没用，直接进入深度睡眠，等待 HOME 按键唤醒
    for (int i = 0; i < 2; i++)
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
    vTaskDelay(100);

    // 配置HOME按键为唤醒源，检测上升沿唤醒
    esp_sleep_enable_ext0_wakeup(GPIO_INPUT_HOME_BTN, 1); // 1表示高电平唤醒
    // 半小时后自动唤醒，如果此时唤醒没有成功，说明已经完全关机（未在充电），如果唤醒成功，则尝试重新进入睡眠模式？
    // esp_sleep_enable_timer_wakeup(1800LL * 1000000LL);
    esp_sleep_enable_timer_wakeup(30LL * 1000000LL);

    // ESP_LOGW("SLEEP", "深度睡眠中");
    // vTaskDelay(200);
    esp_deep_sleep_start();
}

// -------------------------------------------------------------------- TASK -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// 关机任务

void shutdown_task(void *pvParameter)
{
    for (;;)
    {
        // 判断关机信号量
        if (xSemaphoreTake(shutdown_semaphore, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI("SHUTDOWN", "Shutdown signal received");
            // 等待松开
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
            setLED(3, 0, 0, 0);
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
            vTaskDelay(pdMS_TO_TICKS(400));
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
                    setLED(i, 20, 10, 0);
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
            setLED(0, 0, led_on_off ? 10 : 0, led_on_off ? 5 : 0);
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
    for (;;)
    {
        // 等待LED操作完成，一直阻塞直到信号量被释放
        if (xSemaphoreTake(led_flash_semaphore, portMAX_DELAY) == pdTRUE)
        {
            flashLED();
        }
    }
    vTaskDelete(NULL);
}

// adc 读取raw任务

// ADC读取任务，现在即使在蓝牙未连接时也能运行，但只在连接时处理数据
void adc_read_task(void *pvParameters)
{
    ESP_LOGI("adc_read_task", "ADC读取任务启动");

    adc_continuous_handle_t handle = ADC_init_handle;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN];
    memset(result, 0, EXAMPLE_READ_LEN);

    // 用于追踪每个通道的数据写入位置
    static uint8_t writeIndex[ADC_CHANNEL_COUNT] = {0};
    // 用于追踪每个通道的数据计数
    static uint16_t count[ADC_CHANNEL_COUNT] = {0};

    // 记录上一次的sec_conn状态
    bool last_sec_conn_state = false;

    for (;;)
    {
        // 该判断只在非校准模式下有效
        if (js_calibration_running == false)
        // 只有在sec_conn状态发生变化时才调用start/stop函数
        {
            if (sec_conn != last_sec_conn_state)
            {
                if (sec_conn)
                {
                    // 连上了再开始初始化
                    ESP_LOGI("adc_read_task", "Starting ADC sampling");
                    start_adc_sampling();
                }
                else
                {
                    ESP_LOGI("adc_read_task", "Stopping ADC sampling");
                    stop_adc_sampling();
                }
                last_sec_conn_state = sec_conn;
            }
        }

        // 从DMA缓冲区读取数据
        // esp_err_t ret = ESP_ERR_TIMEOUT;
        if (adc_running)
        {
            esp_err_t ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK)
            {
                // 遍历读取到的数据
                for (int i = 0; i < ret_num; i += sizeof(adc_digi_output_data_t))
                {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
                    uint32_t chan = EXAMPLE_ADC_GET_CHANNEL(p);
                    uint32_t data = EXAMPLE_ADC_GET_DATA(p);

                    if (chan < ADC_CHANNEL_COUNT)
                    {
                        // if(chan == ADC_CHANNEL_RIGHT_LEFT_RIGHT){ESP_LOGI("ADCraw","%d",data);}
                        // 将数据写入对应通道的数组
                        resultAvr[chan][writeIndex[chan]] = data;
                        writeIndex[chan]++;
                        if (writeIndex[chan] >= AVERAGE_LEN)
                        {
                            writeIndex[chan] = 0;
                        }

                        // 更新计数
                        if (count[chan] < AVERAGE_LEN)
                        {
                            count[chan]++;
                        }

                        // 如果启用了多通道缓冲区，也将数据添加到mcb中
                        if (mcb != NULL)
                        {
                            mcb_push(mcb, chan, data);
                        }
                    }
                }
            }
            else if (ret == ESP_ERR_TIMEOUT)
            {
                // 超时，继续循环
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static int32_t clamp(int32_t value, int32_t min, int32_t max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

// 安全除法
static int32_t safe_divide(int32_t numerator, int32_t denominator, int32_t default_val)
{
    if (denominator == 0)
    {
        return default_val; // 处理校准数据异常
    }
    return numerator / denominator;
}

// 摇杆值映射到 0~255 范围（中心对应127左右）
static uint8_t map_joystick(int32_t raw, int32_t center, int32_t min, int32_t max)
{
    int32_t offset = raw - center; // 计算相对于中心的偏移（可正可负）
    int32_t deadzone = 50;         // 死区范围

    // 如果在死区内，返回中心值127
    if (offset >= -deadzone && offset <= deadzone)
    {
        return 127;
    }

    // 调整偏移量，排除死区影响
    if (offset > 0)
    {
        offset -= deadzone;
    }
    else
    {
        offset += deadzone;
    }

    // 调整有效范围
    int32_t range_pos = max - center - deadzone; // 中心到最大值的有效范围（正向）
    int32_t range_neg = center - min - deadzone; // 中心到最小值的有效范围（负向）

    int32_t scaled;
    if (offset >= 0)
    {
        // 正向偏移：映射到 127~255
        scaled = 128 + safe_divide(offset * 127, range_pos, 127);
    }
    else
    {
        // 负向偏移：映射到 0~127（取绝对值计算）
        scaled = 128 - safe_divide((-offset) * 128, range_neg, 128);
    }

    return (uint8_t)clamp(scaled, 0, 255); // 确保结果在 0~255 范围内
}

// 摇杆校准数据
// joystick_calibration_data_t left_joystick_cal_data;
// joystick_calibration_data_t right_joystick_cal_data;
void adc_aver_send_task(void *pvParameters)
{
    uint32_t all_avg[8];
    for (;;)
    {
        // 当正在进行环形校准时不要进行此操作，防止耗时
        if (js_calibration_running != true)
        {
            mcb_get_all_averages(mcb, all_avg);
            
            

            for (int i = 0; i < 8; i++)
            {
                //esp_err_t ret = adc_cali_raw_to_voltage(adc1_cali_handle, all_avg[i], &voltage_mv);
                printf("[%d]=%ld ", i, all_avg[i]);
            }
            printf("\n");

            // 此处可以直接认为，平均后的值为 ADC 的原始数据

            // 右摇杆 Y 轴
            gamepad_report_buffer[3] = map_joystick(all_avg[0],
                                                    right_joystick_cal_data.center_y,
                                                    right_joystick_cal_data.min_y,
                                                    right_joystick_cal_data.max_y);

            // 右摇杆 X 轴（注意此处minmax反向）
            gamepad_report_buffer[2] = 255 - map_joystick(all_avg[1],
                                                          right_joystick_cal_data.center_x,
                                                          right_joystick_cal_data.min_x,
                                                          right_joystick_cal_data.max_x);

            // 左摇杆 Y 轴
            gamepad_report_buffer[1] = map_joystick(all_avg[2],
                                                    left_joystick_cal_data.center_y,
                                                    left_joystick_cal_data.min_y,
                                                    left_joystick_cal_data.max_y);

            // 左摇杆 X 轴（注意此处minmax反向）
            gamepad_report_buffer[0] = 255 - map_joystick(all_avg[3],
                                                          left_joystick_cal_data.center_x,
                                                          left_joystick_cal_data.min_x,
                                                          left_joystick_cal_data.max_x);

            // 处理扳机值 - 将ADC原始值(0-4095)映射到(255-0)
            // 左扳机所在的通道是all_avg[4]对应gamepad_report_buffer[8]
            // ESP_LOGI("Trigger", "%d     %d", all_avg[4],all_avg[5]);

            if (all_avg[4] > 1215)
            {
                gamepad_report_buffer[8] = 0;
            }
            else
            {
                gamepad_report_buffer[8] = 255 - (all_avg[4] * 255 / 1215);
            }

            // 右扳机all_avg[5]对应gamepad_report_buffer[7]
            if (all_avg[5] > 1215)
            {
                gamepad_report_buffer[7] = 0;
            }
            else
            {
                gamepad_report_buffer[7] = 255 - (all_avg[5] * 255 / 1215);
            }

            // 处理十字键
            uint32_t dpad_adc_value = all_avg[7];
            uint8_t dpad_value = 0x00; // 默认无按键状态
                
            // 根据电压值判断十字键状态
            if (dpad_adc_value >= (DPAD_NONE - CONFIDENCE_RANGE) && dpad_adc_value <= (DPAD_NONE + CONFIDENCE_RANGE)) {
                // 无按键
                dpad_value = 0xff;
            } else if (dpad_adc_value >= (DPAD_UP - CONFIDENCE_RANGE) && dpad_adc_value <= (DPAD_UP + CONFIDENCE_RANGE)) {
                // 上
                dpad_value = 0x00;
            } else if (dpad_adc_value >= (DPAD_DOWN - CONFIDENCE_RANGE) && dpad_adc_value <= (DPAD_DOWN + CONFIDENCE_RANGE)) {
                // 下
                dpad_value = 0x04;
            } else if (dpad_adc_value >= (DPAD_LEFT - CONFIDENCE_RANGE) && dpad_adc_value <= (DPAD_LEFT + CONFIDENCE_RANGE)) {
                // 左
                dpad_value = 0x06;
            } else if (dpad_adc_value >= (DPAD_RIGHT - CONFIDENCE_RANGE) && dpad_adc_value <= (DPAD_RIGHT + CONFIDENCE_RANGE)) {
                // 右
                dpad_value = 0x02;
            } else if (dpad_adc_value >= (DPAD_UP_LEFT - CONFIDENCE_RANGE) && dpad_adc_value <= (DPAD_UP_LEFT + CONFIDENCE_RANGE)) {
                // 上左
                dpad_value = 0x07; // UP | LEFT
            } else if (dpad_adc_value >= (DPAD_UP_RIGHT - CONFIDENCE_RANGE) && dpad_adc_value <= (DPAD_UP_RIGHT + CONFIDENCE_RANGE)) {
                // 上右
                dpad_value = 0x01; // UP | RIGHT
            } else if (dpad_adc_value >= (DPAD_DOWN_LEFT - CONFIDENCE_RANGE) && dpad_adc_value <= (DPAD_DOWN_LEFT + CONFIDENCE_RANGE)) {
                // 下左
                dpad_value = 0x05; // DOWN | LEFT
            } else if (dpad_adc_value >= (DPAD_DOWN_RIGHT - CONFIDENCE_RANGE) && dpad_adc_value <= (DPAD_DOWN_RIGHT + CONFIDENCE_RANGE)) {
                // 下右
                dpad_value = 0x03; // DOWN | RIGHT
            } else {
                // 无法识别的值，保持默认无按键状态
                dpad_value = 0xff;
            }
            
            gamepad_report_buffer[4] = dpad_value;

        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}



void joystick_calibration_task(void *pvParameter)
{
    for (;;)
    {
        if (xSemaphoreTake(calibration_semaphore, portMAX_DELAY) == pdTRUE)
        {
            sec_conn = false;
            // 失能蓝牙
            esp_bluedroid_disable();
            esp_bluedroid_deinit();
            esp_bt_controller_disable();
            esp_bt_controller_deinit();
            vTaskDelay(500);
            js_calibration_running = true;
            start_adc_sampling();
            ESP_LOGI("CALIBRATION", "开始摇杆校准");
            // 保存当前设备状态，以便校准结束后恢复
            // device_state_t prev_state = current_device_state;
            current_device_state = DEVICE_STATE_CALI_START;
            vTaskDelay(pdMS_TO_TICKS(2000));
            current_device_state = DEVICE_STATE_CALI_RING;

            // 初始化最大最小值
            uint32_t max_values[4] = {0, 0, 0, 0};             // 通道0,1,2,3的最大值
            uint32_t min_values[4] = {4095, 4095, 4095, 4095}; // 通道0,1,2,3的最小值

            uint32_t all_avg[8];
            uint32_t start_time = xTaskGetTickCount();
            uint32_t duration_ticks = pdMS_TO_TICKS(8000); // 8秒

            // 持续读取8秒数据
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
            vTaskDelay(pdMS_TO_TICKS(3000));
            // 等待用户松手
            // 获取平均值作为中心点
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

            current_device_state = DEVICE_STATE_SLEEP;
            vTaskDelay(pdMS_TO_TICKS(100));
            xSemaphoreGive(calibration_semaphore); // 归还信号量
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        }
    }
}

void all_buttons_monitor_task(void *pvParameter)
{
    ESP_LOGI("XYAB_MONITOR", "XYAB Button Monitor Task Started");

    // 用于检测SELECT和START按键同时按下的计时
    TickType_t press_start_time = 0;
    bool calibration_triggered = false;

    for (;;)
    {
        // 等待100ms
        vTaskDelay(pdMS_TO_TICKS(50));
        uint8_t xyab_button_value = 0;
        // 读取XYAB按键事件组状态
        EventBits_t xyab_bits = xEventGroupGetBits(xyab_button_event_group);
        if (xyab_bits & XYAB_KEY_X_PRESSED)
        {
            xyab_button_value |= 0x08; // X 按下
        }
        if (xyab_bits & XYAB_KEY_Y_PRESSED)
        {
            xyab_button_value |= 0x10; // Y 按下
        }
        if (xyab_bits & XYAB_KEY_A_PRESSED)
        {
            xyab_button_value |= 0x01; // A 按下
        }
        if (xyab_bits & XYAB_KEY_B_PRESSED)
        {
            xyab_button_value |= 0x02; // B 按下
        }
        if (xyab_bits & LEFT_SHOULDER_BTN_PRESSED)
        {
            xyab_button_value |= 0x40; // 左肩键按下
        }
        if (xyab_bits & RIGHT_SHOULDER_BTN_PRESSED)
        {
            xyab_button_value |= 0x80; // 右肩键按下
        }

        // 更新 gamepad_report_buffer[5]
        gamepad_report_buffer[5] = xyab_button_value;

        // 打印XYAB按键状态
        // ESP_LOGI("XYAB_MONITOR", "XYAB Key States: X=%s, Y=%s, A=%s, B=%s",
        //          (xyab_bits & XYAB_KEY_X_PRESSED) ? "1" : "0",
        //          (xyab_bits & XYAB_KEY_Y_PRESSED) ? "1" : "0",
        //          (xyab_bits & XYAB_KEY_A_PRESSED) ? "1" : "0",
        //          (xyab_bits & XYAB_KEY_B_PRESSED) ? "1" : "0");

        // 读取其他按键事件组状态
        uint8_t other_button_value = 0;
        EventBits_t other_bits = xEventGroupGetBits(other_button_event_group);
        if (other_bits & SELECT_BTN_PRESSED)
        {
            other_button_value |= 0x04; // X 按下
        }
        if (other_bits & START_BTN_PRESSED)
        {
            other_button_value |= 0x08; // Y 按下
        }
        if (other_bits & LEFT_JOYSTICK_BTN_PRESSED)
        {
            other_button_value |= 0x20; // A 按下
        }
        if (other_bits & RIGHT_JOYSTICK_BTN_PRESSED)
        {
            other_button_value |= 0x40; // B 按下
        }
        gamepad_report_buffer[6] = other_button_value;




        // 未进入 calibration 模式才执行
        if (js_calibration_running == false)
        {
            // 打印其他按键状态
            // ESP_LOGI("OTHER_MONITOR", "Other Key States: LJS=%s, RJS=%s, SEL=%s, STA=%s, IKEY=%s, IOS=%s, WIN=%s",
            //          (other_bits & LEFT_JOYSTICK_BTN_PRESSED) ? "1" : "0",
            //          (other_bits & RIGHT_JOYSTICK_BTN_PRESSED) ? "1" : "0",
            //          (other_bits & SELECT_BTN_PRESSED) ? "1" : "0",
            //          (other_bits & START_BTN_PRESSED) ? "1" : "0",
            //          (other_bits & IKEY_BTN_PRESSED) ? "1" : "0",
            //          (other_bits & IOS_BTN_PRESSED) ? "1" : "0",
            //          (other_bits & WINDOWS_BTN_PRESSED) ? "1" : "0");

            if ((other_bits & SELECT_BTN_PRESSED) && (other_bits & START_BTN_PRESSED))
            {
                //  todo:这里的 calibration trigg可能没有用
                // 如果是初次检测到同时按下，记录开始时间
                if (!calibration_triggered)
                {
                    press_start_time = xTaskGetTickCount();
                    calibration_triggered = true;
                    ESP_LOGI("CALIBRATION", "SELECT and START buttons pressed, waiting for 3 seconds...");
                }
                // 如果已经按下一段时间，检查是否达到3秒
                else if ((xTaskGetTickCount() - press_start_time) >= pdMS_TO_TICKS(3000))
                {
                    ESP_LOGI("CALIBRATION", "SELECT and START buttons held for 3 seconds, triggering calibration...");
                    // 触发校准信号量
                    if (calibration_semaphore != NULL)
                    {
                        xSemaphoreGive(calibration_semaphore);
                    }
                    // 重置状态以避免重复触发
                    calibration_triggered = false;
                }
            }
            else
            {
                // 如果任何一个按键未按下，重置状态
                if (calibration_triggered)
                {
                    ESP_LOGI("CALIBRATION", "SELECT and START buttons released before 3 seconds");
                    calibration_triggered = false;
                }
            }
        }
    }
}

// 暂时没有用到
void update_buttons_packet()
{
    // 初始化 gamepad_report_buffer[5] 为 0

    // 读取XYAB按键事件组状态
    // EventBits_t xyab_bits = xEventGroupGetBits(xyab_button_event_group);

    // 根据按键状态更新 xyab_button_value
}


void gamepad_packet_send_task(void *pvParameters)
{

    for (;;)
    {
        if (sec_conn && current_device_state == DEVICE_STATE_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(50));

            // 打印游戏手柄报告缓冲区内容
#ifdef GAMEPAD_DEBUG_MODE
            // for (int i = 0; i < HID_GAMEPAD_STICK_IN_RPT_LEN; i++)
            // {
            //     printf("%d ", gamepad_report_buffer[i]);
            // }
            // printf("\r\n");
#endif

            esp_hidd_send_gamepad_report(hid_conn_id);
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGI(HID_TASK_TAG, "Waiting for connection...");
        }
    }
}