#ifndef __MAIN_H__
#define __MAIN_H__


#define gamePadMode 1

// 函数声明
esp_err_t ble_init(void);
esp_err_t ble_sec_config(void);

void SLEEP(void);
// void START_FAIL(void);
esp_err_t setHomeButton(void);
esp_err_t START_UP(void);


typedef enum {
    DEVICE_STATE_INIT,           // 初始化状态
    DEVICE_STATE_ADVERTISING,    // 广播中
    DEVICE_STATE_CONNECTING,     // 连接中
    DEVICE_STATE_CONNECTED,      // 已连接
    DEVICE_STATE_DISCONNECTING,  // 断开连接中
    DEVICE_STATE_DISCONNECTED,   // 已断开连接
    DEVICE_STATE_ERROR,          // 错误状态
    DEVICE_STATE_SLEEP,          // 睡眠状态
} device_state_t;

void blink_task(void *pvParameter);
void LED_flash_task(void *pvParameter);

void gpio_read_task(void *pvParameter);
void adc_read_task(void *pvParameter);
void adc_aver_send(void *pvParameters);
void gamepad_button_task(void *pvParameters);

// 全局变量声明
extern bool led_running;
extern bool adc_running;


#endif