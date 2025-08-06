#include <stdio.h>
#include <string.h>
#include "hardware_init.h"
#include "esp_log.h"
#include "iot_button.h"
#include "driver/gpio.h"
// #include "esp_adc/adc_oneshot.h"
// #include "esp_adc/adc_cali.h"

// #include "driver/adc.h"
// #include "esp_adc_cal.h"

// #include "esp_adc/adc_oneshot.h"
// #include "esp_adc/adc_cali.h"
// #include "esp_adc/adc_cali_scheme.h"

void func(void)
{
    ESP_LOGI("func", "OK");
}
//----------------------------------- LED--------------------------------------------------
#include "led_strip.h"

// New LED configuration options
#define LED_DEFAULT_BRIGHTNESS 0  // Default brightness level (percentage)
#define LED_DEFAULT_COLOR_RED 0   // Default red value
#define LED_DEFAULT_COLOR_GREEN 0 // Default green value
#define LED_DEFAULT_COLOR_BLUE 0  // Default blue value

// Global LED handle
led_strip_handle_t led_strip = NULL;

static const char *TAG = "hardware_init";
static led_strip_handle_t configure_led(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,                      // The GPIO of ws2812_input
        .max_leds = LED_STRIP_LED_NUMBERS,                           // The number of LEDs
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // Pixel format of ws2812
        .led_model = LED_MODEL_WS2812,                               // LED strip model
        .flags.invert_out = false,                                   // not invert the output signal
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

// 如果是多个led一起动，那么此处性能可以优化
esp_err_t setLED(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err;
    
    err = led_strip_set_pixel(led_strip, index, red, green, blue);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED pixel, error: %d", err);
        return err;
    }
    

    return ESP_OK;
}

esp_err_t flashLED(void)
{
    esp_err_t err;
    err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh LED strip, error: %d", err);
        return err;
    }
    return ESP_OK;
}

//--------------------------------------------- ADC ------------------------------------------
#include "esp_adc/adc_continuous.h"

adc_continuous_handle_t ADC_init_handle = NULL;

static adc_channel_t channel[8] = {ADC_CHANNEL_RIGHT_UP_DOWN, ADC_CHANNEL_RIGHT_LEFT_RIGHT, ADC_CHANNEL_LEFT_UP_DOWN, ADC_CHANNEL_LEFT_LEFT_RIGHT, ADC_CHANNEL_LEFT_TRIGGER, ADC_CHANNEL_RIGHT_TRIGGER, ADC_CHANNEL_BATTERY, ADC_CHANNEL_DPAD};

static TaskHandle_t s_task_handle;

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = EXAMPLE_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++)
    {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%" PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%" PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%" PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

uint8_t resultAvr[ADC_CHANNEL_COUNT][AVERAGE_LEN] = {0};
static adc_continuous_handle_t convert_adc_values(uint8_t arr[][AVERAGE_LEN], int rows)
{
    // 清空数组，使用cc填充
    for (int i = 0; i < rows; i++)
    {
        memset(arr[i], 0xcc, AVERAGE_LEN);
    }

    // 输出各个值
    // for (int i = 0; i < rows; i++) {
    //     for (int j = 0; j < AVERAGE_LEN; j++) {
    //         printf("i:%d,j:%d,val:%x\r\n",i,j,arr[i][j]);
    //     }
    // }
    uint32_t ret_num = 0;

    adc_continuous_handle_t handle = NULL;
    // 初始化8个通道
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    return handle;
}

//--------------------------------------------- GPIO ------------------------------------

// GPIO初始化
static void init_gpio(void)
{
    // 上拉输入按键。按下为0
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE; // Disable interrupt
    io_conf.mode = GPIO_MODE_INPUT;        // Set as input
    // 详情查看init.h
    io_conf.pin_bit_mask = BIT64(GPIO_INPUT_KEY_X) | BIT64(GPIO_INPUT_KEY_Y) |
                           BIT64(GPIO_INPUT_KEY_A) | BIT64(GPIO_INPUT_KEY_B) |
                           BIT64(GPIO_INPUT_LEFT_JOYSTICK_BTN) | BIT64(GPIO_INPUT_RIGHT_JOYSTICK_BTN) |
                           BIT64(GPIO_INPUT_LEFT_SHOULDER_BTN) | BIT64(GPIO_INPUT_RIGHT_SHOULDER_BTN) |
                           BIT64(GPIO_INPUT_SELECT_BTN) | BIT64(GPIO_INPUT_START_BTN) | BIT64(GPIO_INPUT_IKEY_BTN) | BIT64(GPIO_INPUT_IOS_BTN) | BIT64(GPIO_INPUT_WINDOWS_BTN);
    io_conf.pull_down_en = false; // Disable pull-down
    io_conf.pull_up_en = true;    // Enable pull-up
    gpio_config(&io_conf);

}

//-------------------------------------------------------------------------------------
void init_all(void)
{
    led_strip = configure_led();
    // init_adc();
    init_gpio();
    ADC_init_handle = convert_adc_values(resultAvr, ADC_CHANNEL_COUNT);
}