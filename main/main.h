#ifndef __MAIN_H__
#define __MAIN_H__


#define gamePadMode 1

extern bool led_running;
extern bool adc_running;

esp_err_t ble_init(void);
esp_err_t ble_sec_config(void);

void SLEEP(void);
esp_err_t setHomeButton(void);
esp_err_t START_UP(void);



void shutdown_task(void *pvParameter);
void blink_task(void *pvParameter);
void LED_flash_task(void *pvParameter);


void adc_read_task(void *pvParameter);
void adc_aver_send_task(void *pvParameters);
void gamepad_packet_send_task(void *pvParameters);
void joystick_calibration_task(void *pvParameter);
void all_buttons_monitor_task(void *pvParameter);
void connection_timeout_task(void *pvParameters);





#endif