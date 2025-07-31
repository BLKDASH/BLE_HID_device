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
// ws2812 IO_IN
#define LED_STRIP_BLINK_GPIO  12
// LED nums
#define LED_STRIP_LED_NUMBERS 4
// 10MHz resolution, 1 tick = 0.1us
#define LED_STRIP_RMT_RES_HZ  (10 * 1000000)

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
    
    // Initialize the LED strip configuration structure
    led_strip_config_t strip_config = {
            .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO of ws2812_input
            .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs
            .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // Pixel format of ws2812
            .led_model = LED_MODEL_WS2812,            // LED strip model
            .flags.invert_out = false,                // not invert the output signal
        };

    // RMT configuration for ws2812
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
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
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    // 为每个通道配置
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_1, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_2, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_5, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &chan_cfg));

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
    // int voltage;
    
    // // 右上下
    // voltage = read_adc_channel_voltage(ADC_CHANNEL_0);
    // printf("RIGHT上下ADC CH0 (GPIO36): %d\n", voltage);
    
    // // 右左右
    // voltage = read_adc_channel_voltage(ADC_CHANNEL_1);
    // printf("RIGHT左右ADC CH1 (GPIO37): %d\n", voltage);
    
    // // 左上下
    // voltage = read_adc_channel_voltage(ADC_CHANNEL_2);
    // printf("LEFT上下ADC CH2 (GPIO38): %d\n", voltage);
    
    // // 左左右
    // voltage = read_adc_channel_voltage(ADC_CHANNEL_3);
    // printf("LEFT左右ADC CH3 (GPIO39): %d\n", voltage);
    
    // // 左板机
    // voltage = read_adc_channel_voltage(ADC_CHANNEL_4);
    // printf("LEFT板机ADC CH4 (GPIO32): %d\n", voltage);
    
    // // 右板机
    // voltage = read_adc_channel_voltage(ADC_CHANNEL_5);
    // printf("RIGHT板机ADC CH5 (GPIO33): %d\n", voltage);
    
    // // 电池电压
    // voltage = read_adc_channel_voltage(ADC_CHANNEL_6);
    // printf("BATTERY ADC CH6 (GPIO34): %d\n", voltage);
    
    // // 十字键
    // voltage = read_adc_channel_voltage(ADC_CHANNEL_7);
    // printf("十字键ADC CH7 (GPIO35): %d\n", voltage);

    float voltage0 = (float)read_adc_channel_voltage(ADC_CHANNEL_0) / 1000.0f;
    float voltage1 = (float)read_adc_channel_voltage(ADC_CHANNEL_1) / 1000.0f;
    float voltage2 = (float)read_adc_channel_voltage(ADC_CHANNEL_2) / 1000.0f;
    float voltage3 = (float)read_adc_channel_voltage(ADC_CHANNEL_3) / 1000.0f;
    float voltage4 = (float)read_adc_channel_voltage(ADC_CHANNEL_4) / 1000.0f;
    float voltage5 = (float)read_adc_channel_voltage(ADC_CHANNEL_5) / 1000.0f;
    float voltage6 = (float)read_adc_channel_voltage(ADC_CHANNEL_6) / 1000.0f;
    float voltage7 = (float)read_adc_channel_voltage(ADC_CHANNEL_7) / 1000.0f;

    printf("%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
           voltage0, voltage1, voltage2, voltage3,
           voltage4, voltage5, voltage6, voltage7);
}




//--------------------------------------------- GPIO------------------------------------



// GPIO初始化
void init_gpio(void)
{
    gpio_config_t io_conf = {};
    
    // Configure output pin (GPIO32)
    // io_conf.intr_type = GPIO_INTR_DISABLE;         // Disable interrupt
    // io_conf.mode = GPIO_MODE_OUTPUT;               // Set as output
    // //
    // io_conf.pin_bit_mask = BIT64(GPIO_OUTPUT_IO_32); // GPIO32
    // io_conf.pull_down_en = false;                  // Disable pull-down
    // io_conf.pull_up_en = false;                    // Disable pull-up
    // gpio_config(&io_conf);

    // Configure input pins (GPIO25, 26, 27, 14)
    io_conf.intr_type = GPIO_INTR_DISABLE;         // Disable interrupt
    io_conf.mode = GPIO_MODE_INPUT;                // Set as input
    // 25 26 27 14分别对应右上角KEY X Y A B（左 上 下 右）
    io_conf.pin_bit_mask = BIT64(GPIO_INPUT_IO_25) | BIT64(GPIO_INPUT_IO_26) |
                           BIT64(GPIO_INPUT_IO_27) | BIT64(GPIO_INPUT_IO_14) |
                           BIT64(GPIO_INPUT_IO_15) | BIT64(GPIO_INPUT_IO_19);
    io_conf.pull_down_en = false;                  // Disable pull-down
    io_conf.pull_up_en = true;                     // Enable pull-up


    gpio_config(&io_conf);
}



//-------------------------------------------------------------------------------------
void init_all(void)
{
    led_strip = configure_led();
    init_adc();
    init_gpio();
    
}