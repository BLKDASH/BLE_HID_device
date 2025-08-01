#include <stdio.h>
#include "hardware_init.h"
#include "esp_log.h"
#include "iot_button.h"
#include "led_strip.h"
#include "driver/gpio.h"
// #include "esp_adc/adc_oneshot.h"
// #include "esp_adc/adc_cali.h"

// #include "driver/adc.h"
// #include "esp_adc_cal.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


void func(void)
{
    ESP_LOGI("func", "OK");
}
//----------------------------------- LED--------------------------------------------------


// New LED configuration options
#define LED_DEFAULT_BRIGHTNESS 0 // Default brightness level (percentage)
#define LED_DEFAULT_COLOR_RED   0 // Default red value
#define LED_DEFAULT_COLOR_GREEN 0 // Default green value
#define LED_DEFAULT_COLOR_BLUE  0 // Default blue value

// Global LED handle
static led_strip_handle_t led_strip = NULL;

static const char *TAG = "hardware_init";
led_strip_handle_t configure_led(void)
{
    led_strip_config_t strip_config = {
            .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO of ws2812_input
            .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs
            .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // Pixel format of ws2812
            .led_model = LED_MODEL_WS2812,            // LED strip model
            .flags.invert_out = false,                // not invert the output signal
        };

    // RMT configuration for ws2812
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
} 


esp_err_t setLED(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
{
     ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, index, red, green, blue));
     ESP_ERROR_CHECK(led_strip_refresh(led_strip));
     return ESP_OK;
}


//--------------------------------------------- ADC ------------------------------------------

adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_handle;

void init_adc(void)
{
    // ADC 初始化配置
    adc_oneshot_unit_init_cfg_t init_param = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_param, &adc1_handle));

    // 配置单个通道
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12, //增益12（150mv-3.9V（最高为满程电压））
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    // 为每个通道配置

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_RIGHT_UP_DOWN, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_RIGHT_LEFT_RIGHT, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_LEFT_UP_DOWN, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_LEFT_LEFT_RIGHT, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_LEFT_TRIGGER, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_RIGHT_TRIGGER, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_BATTERY, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_DPAD, &chan_cfg));


    // 校准配置
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc1_cali_handle));
}

// 读取 ADC 值
int read_adc_channel_voltage(adc_channel_t channel)
{
    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, &raw));
    int voltage_mv;
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage_mv));
    return voltage_mv;
}

void read_and_log_adc_values(void)
{

    float voltage0 = (float)read_adc_channel_voltage(ADC_CHANNEL_RIGHT_UP_DOWN) / 1000.0f;
    float voltage1 = (float)read_adc_channel_voltage(ADC_CHANNEL_RIGHT_LEFT_RIGHT) / 1000.0f;
    float voltage2 = (float)read_adc_channel_voltage(ADC_CHANNEL_LEFT_UP_DOWN) / 1000.0f;
    float voltage3 = (float)read_adc_channel_voltage(ADC_CHANNEL_LEFT_LEFT_RIGHT) / 1000.0f;
    float voltage4 = (float)read_adc_channel_voltage(ADC_CHANNEL_LEFT_TRIGGER) / 1000.0f;
    float voltage5 = (float)read_adc_channel_voltage(ADC_CHANNEL_RIGHT_TRIGGER) / 1000.0f;
    float voltage6 = (float)read_adc_channel_voltage(ADC_CHANNEL_BATTERY) / 1000.0f;
    float voltage7 = (float)read_adc_channel_voltage(ADC_CHANNEL_DPAD) / 1000.0f;


    printf("%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
           voltage0, voltage1, voltage2, voltage3,
           voltage4, voltage5, voltage6, voltage7);
}




//--------------------------------------------- GPIO ------------------------------------



// GPIO初始化
void init_gpio(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;         // Disable interrupt
    io_conf.mode = GPIO_MODE_INPUT;                // Set as input
    // 详情查看init.h
    io_conf.pin_bit_mask = BIT64(GPIO_INPUT_KEY_X) | BIT64(GPIO_INPUT_KEY_Y) |
                           BIT64(GPIO_INPUT_KEY_A) | BIT64(GPIO_INPUT_KEY_B) |
                           BIT64(GPIO_INPUT_LEFT_JOYSTICK_BTN) | BIT64(GPIO_INPUT_RIGHT_JOYSTICK_BTN) | 
                           BIT64(GPIO_INPUT_LEFT_SHOULDER_BTN) | BIT64(GPIO_INPUT_RIGHT_SHOULDER_BTN) |
                           BIT64(GPIO_INPUT_SELECT_BTN)| BIT64(GPIO_INPUT_START_BTN) | BIT64(GPIO_INPUT_IKEY_BTN) | BIT64(GPIO_INPUT_IOS_BTN)|BIT64(GPIO_INPUT_WINDOWS_BTN);
    io_conf.pull_down_en = false;                  // Disable pull-down
    io_conf.pull_up_en = true;                     // Enable pull-up
    gpio_config(&io_conf);
    
    // 为HOME按键单独配置为拉低模式（已经在button中初始化）
    // gpio_config_t home_btn_conf = {};
    // home_btn_conf.intr_type = GPIO_INTR_DISABLE;
    // home_btn_conf.mode = GPIO_MODE_INPUT;
    // home_btn_conf.pin_bit_mask = BIT64(GPIO_INPUT_HOME_BTN);
    // home_btn_conf.pull_down_en = true;             // Enable pull-down for HOME button
    // home_btn_conf.pull_up_en = false;              // Disable pull-up for HOME button
    // gpio_config(&home_btn_conf);
}



//-------------------------------------------------------------------------------------
void init_all(void)
{
    // led_strip = configure_led();
    init_adc();
    init_gpio();
    
}