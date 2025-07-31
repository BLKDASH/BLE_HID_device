#ifndef __HARDWARE_INIT_H__
#define __HARDWARE_INIT_H__

#include "led_strip.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

void func(void);
led_strip_handle_t configure_led(void);
// esp_err_t set_led_brightness(uint8_t brightness);
// esp_err_t update_leds(void);
esp_err_t setLED(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);


#define ADC_CHANNEL_RIGHT_UP_DOWN     ADC_CHANNEL_0  // 右上下 (GPIO36)
#define ADC_CHANNEL_RIGHT_LEFT_RIGHT  ADC_CHANNEL_1  // 右左右 (GPIO37)
#define ADC_CHANNEL_LEFT_UP_DOWN      ADC_CHANNEL_2  // 左上下 (GPIO38)
#define ADC_CHANNEL_LEFT_LEFT_RIGHT   ADC_CHANNEL_3  // 左左右 (GPIO39)
#define ADC_CHANNEL_LEFT_TRIGGER      ADC_CHANNEL_4  // 左板机 (GPIO32)
#define ADC_CHANNEL_RIGHT_TRIGGER     ADC_CHANNEL_5  // 右板机 (GPIO33)
#define ADC_CHANNEL_BATTERY           ADC_CHANNEL_6  // 电池电压 (GPIO34)
#define ADC_CHANNEL_DPAD              ADC_CHANNEL_7  // 十字键 (GPIO35)

// ADC functions
void init_adc(void);
int read_adc_channel_voltage(adc_channel_t channel);
void read_and_log_adc_values(void);


// Define GPIOs
// 右上角按键 (X Y A B)
#define GPIO_INPUT_KEY_X         25  // 右上角按键X (左)
#define GPIO_INPUT_KEY_Y         26  // 右上角按键Y (上)
#define GPIO_INPUT_KEY_A         27  // 右上角按键A (下)
#define GPIO_INPUT_KEY_B         14  // 右上角按键B (右)

// 摇杆按键
#define GPIO_INPUT_LEFT_JOYSTICK_BTN   15  // 左摇杆按键
#define GPIO_INPUT_RIGHT_JOYSTICK_BTN  19  // 右摇杆按键

// 肩键
#define GPIO_INPUT_LEFT_SHOULDER_BTN   23  // 左肩键
#define GPIO_INPUT_RIGHT_SHOULDER_BTN  18  // 右肩键
// 超薄按键
#define GPIO_INPUT_SELECT_BTN    4  // SELECT按键
#define GPIO_INPUT_START_BTN     2   // START按键
#define GPIO_INPUT_HOME_BTN      13   // HOME按键
#define GPIO_INPUT_IKEY_BTN      0   // IKEY按键
#define GPIO_INPUT_IOS_BTN       21   // IOS按键
#define GPIO_INPUT_WINDOWS_BTN   22   // Windows按键

#define GPIO_INPUT_ANDROID_BTN   4   // Android按键


void init_gpio(void);

void init_all(void);


#endif


