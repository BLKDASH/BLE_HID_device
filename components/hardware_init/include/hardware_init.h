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

// ADC functions
void init_adc(void);
int read_adc_channel_voltage(adc_channel_t channel);
void read_and_log_adc_values(void);


// Define GPIOs
#define GPIO_INPUT_IO_25     25
#define GPIO_INPUT_IO_26     26
#define GPIO_INPUT_IO_27     27
#define GPIO_INPUT_IO_14     14

#define GPIO_INPUT_IO_15     15 //右摇杆按键
#define GPIO_INPUT_IO_19     19 //左摇杆按键

void init_gpio(void);

void init_all(void);


#endif


