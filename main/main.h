#ifndef __MAIN_H__
#define __MAIN_H__


#define gamePadMode 1

// 函数声明
esp_err_t ble_init(void);
esp_err_t ble_sec_config(void);

void SLEEP(void);
void START_FAIL(void);
esp_err_t setHomeButton(void);
esp_err_t START_UP(void);


void blink_task(void *pvParameter);
void LED_flash_task(void *pvParameter);

void gpio_read_task(void *pvParameter);
void adc_read_task(void *pvParameter);

void gamepad_button_task(void *pvParameters);

// 全局变量声明
extern bool led_running;
extern bool adc_running;


#endif