#ifndef __MAIN_H__
#define __MAIN_H__


#define gamePadMode 1

// 函数声明
esp_err_t ble_init(void);
esp_err_t ble_sec_config(void);

#if (gamePadMode == 0)
void hid_demo_task(void *pvParameters);
void mouse_move_task(void *pvParameters);
#elif (gamePadMode == 1)
void gamepad_button_task(void *pvParameters);
#endif

#endif