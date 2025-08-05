#ifndef __HARDWARE_INIT_H__
#define __HARDWARE_INIT_H__

#include "led_strip.h"
#include "esp_adc/adc_continuous.h"
// #include "driver/adc.h"
// #include "esp_adc_cal.h"

extern led_strip_handle_t led_strip;

// ws2812 IO_IN
#define LED_STRIP_BLINK_GPIO 12
// LED nums
#define LED_STRIP_LED_NUMBERS 4
// 10MHz resolution, 1 tick = 0.1us
#define LED_STRIP_RMT_RES_HZ (10 * 1000000)

void func(void);
esp_err_t setLED(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t flashLED(void);

#define ADC_CHANNEL_RIGHT_UP_DOWN ADC_CHANNEL_0    // 右上下 (GPIO36)
#define ADC_CHANNEL_RIGHT_LEFT_RIGHT ADC_CHANNEL_1 // 右左右 (GPIO37)
#define ADC_CHANNEL_LEFT_UP_DOWN ADC_CHANNEL_2     // 左上下 (GPIO38)
#define ADC_CHANNEL_LEFT_LEFT_RIGHT ADC_CHANNEL_3  // 左左右 (GPIO39)
#define ADC_CHANNEL_LEFT_TRIGGER ADC_CHANNEL_4     // 左板机 (GPIO32)
#define ADC_CHANNEL_RIGHT_TRIGGER ADC_CHANNEL_5    // 右板机 (GPIO33)
#define ADC_CHANNEL_BATTERY ADC_CHANNEL_6          // 电池电压 (GPIO34)
#define ADC_CHANNEL_DPAD ADC_CHANNEL_7             // 十字键 (GPIO35)

// ADC functions
// void init_adc(void);
// int read_adc_channel_voltage(adc_channel_t channel);
// void read_and_log_adc_values(void);
// ADC配置
#define EXAMPLE_ADC_UNIT ADC_UNIT_1
#define _EXAMPLE_ADC_UNIT_STR(unit) #unit
#define EXAMPLE_ADC_UNIT_STR(unit) _EXAMPLE_ADC_UNIT_STR(unit)
#define EXAMPLE_ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN ADC_ATTEN_DB_12
#define EXAMPLE_ADC_BIT_WIDTH SOC_ADC_DIGI_MAX_BITWIDTH

// 设置输出格式类型和获取通道/数据的宏
#define EXAMPLE_ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_GET_CHANNEL(p_data) ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data) ((p_data)->type1.data)

#define ADC_CHANNEL_COUNT 8
#define EXAMPLE_READ_LEN 256
#define AVERAGE_LEN 10

extern uint8_t resultAvr[ADC_CHANNEL_COUNT][AVERAGE_LEN];
extern adc_continuous_handle_t ADC_init_handle;

// Define GPIOs
// 右上角按键 (X Y A B)
#define GPIO_INPUT_KEY_X 25 // 右上角按键X (左)
#define GPIO_INPUT_KEY_Y 26 // 右上角按键Y (上)
#define GPIO_INPUT_KEY_A 27 // 右上角按键A (下)
#define GPIO_INPUT_KEY_B 14 // 右上角按键B (右)

// 摇杆按键
#define GPIO_INPUT_LEFT_JOYSTICK_BTN 15  // 左摇杆按键
#define GPIO_INPUT_RIGHT_JOYSTICK_BTN 19 // 右摇杆按键

// 肩键
#define GPIO_INPUT_LEFT_SHOULDER_BTN 23  // 左肩键
#define GPIO_INPUT_RIGHT_SHOULDER_BTN 18 // 右肩键
// 超薄按键
#define GPIO_INPUT_SELECT_BTN 4   // SELECT按键
#define GPIO_INPUT_START_BTN 2    // START按键
#define GPIO_INPUT_HOME_BTN 13    // HOME按键，开关机检测按键
#define GPIO_INPUT_IKEY_BTN 0     // IKEY按键
#define GPIO_INPUT_IOS_BTN 21     // IOS按键
#define GPIO_INPUT_WINDOWS_BTN 22 // Windows按键

#define GPIO_INPUT_ANDROID_BTN 4 // Android按键

#define GPIO_OUTPUT_POWER_KEEP_IO 5

void init_all(void);

#endif
