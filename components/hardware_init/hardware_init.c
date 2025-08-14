#include <stdio.h>
#include <string.h>
#include "hardware_init.h"
#include "esp_log.h"
#include "iot_button.h"
#include "driver/gpio.h"

#include "led_strip.h"

#include "esp_adc/adc_continuous.h"

#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_hidd_prf_api.h"
#include "hidd_le_prf_int.h"
#include "hid_dev.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "button_gpio.h"

// 声明事件组句柄
EventGroupHandle_t ble_event_group = NULL;

// LED-----------------------------------------------------------------------------------------

// New LED configuration options
#define LED_DEFAULT_BRIGHTNESS 0  // Default brightness level (percentage)
#define LED_DEFAULT_COLOR_RED 0   // Default red value
#define LED_DEFAULT_COLOR_GREEN 0 // Default green value
#define LED_DEFAULT_COLOR_BLUE 0  // Default blue value

// Global LED handle
led_strip_handle_t led_strip = NULL;

// XYAB按键事件组
EventGroupHandle_t xyab_button_event_group = NULL;
// 其他按键事件组
EventGroupHandle_t other_button_event_group = NULL;

static const char *TAG = "hardware_init";
static led_strip_handle_t configure_led(void)
{
    // RMT 模式
    // led_strip_config_t strip_config = {
    //     .strip_gpio_num = LED_STRIP_BLINK_GPIO,                      // The GPIO of ws2812_input
    //     .max_leds = LED_STRIP_LED_NUMBERS,                           // The number of LEDs
    //     .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // Pixel format of ws2812
    //     .led_model = LED_MODEL_WS2812,                               // LED strip model
    //     .flags.invert_out = false,                                   // not invert the output signal
    // };

    // // RMT configuration for ws2812
    // led_strip_rmt_config_t rmt_config = {
    //     .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
    //     .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
    //     .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
    // };

    // // LED Strip object handle
    // led_strip_handle_t led_strip;
    // ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    // ESP_LOGI(TAG, "Created LED strip object with RMT backend");

    // SPI 模式
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO, // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,      // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,          // LED strip model
        // set the color order of the strip: GRB
        .color_component_format = {
            .format = {
                .r_pos = 1,          // red is the second byte in the color data
                .g_pos = 0,          // green is the first byte in the color data
                .b_pos = 2,          // blue is the third byte in the color data
                .num_components = 3, // total 3 color components
            },
        },
        .flags = {
            .invert_out = false, // don't invert the output signal
        }};


    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = SPI2_HOST, 
        .flags = {
            .with_dma = false,
        }};


    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with SPI backend");
    return led_strip;
}

esp_err_t setLED(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err;

    err = led_strip_set_pixel(led_strip, index, red, green, blue);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set LED pixel, error: %d", err);
        return err;
    }

    return ESP_OK;
}

esp_err_t flashLED(void)
{
    esp_err_t err;
    err = led_strip_refresh(led_strip);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to refresh LED strip, error: %d", err);
        return err;
    }
    return ESP_OK;
}

// ADC ----------------------------------------------------------------------------------------------

adc_continuous_handle_t ADC_init_handle = NULL;
// ADC校准句柄
adc_cali_handle_t adc1_cali_handle = NULL;
static bool adc_calibration_enabled = false;

static adc_channel_t channel[8] = {ADC_CHANNEL_RIGHT_UP_DOWN, ADC_CHANNEL_RIGHT_LEFT_RIGHT, ADC_CHANNEL_LEFT_UP_DOWN, ADC_CHANNEL_LEFT_LEFT_RIGHT, ADC_CHANNEL_LEFT_TRIGGER, ADC_CHANNEL_RIGHT_TRIGGER, ADC_CHANNEL_BATTERY, ADC_CHANNEL_DPAD};

// ADC校准初始化函数
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = EXAMPLE_ADC_BIT_WIDTH,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = EXAMPLE_ADC_BIT_WIDTH,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

// ADC校准去初始化函数
// static void adc_calibration_deinit(adc_cali_handle_t handle)
// {
// #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
//     ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
//     ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
// #elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
//     ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
//     ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
// #endif
// }

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        //.max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000, // 10kHz 采样
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
        // ESP_LOGI(TAG, "adc_pattern[%d].atten is :%" PRIx8, i, adc_pattern[i].atten);
        // ESP_LOGI(TAG, "adc_pattern[%d].channel is :%" PRIx8, i, adc_pattern[i].channel);
        // ESP_LOGI(TAG, "adc_pattern[%d].unit is :%" PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    // 初始化ADC校准
    adc_calibration_enabled = adc_calibration_init(EXAMPLE_ADC_UNIT, EXAMPLE_ADC_ATTEN, &adc1_cali_handle);

    *out_handle = handle;
}




extern bool adc_running;

// 启动ADC采集
esp_err_t start_adc_sampling(void)
{
    if (ADC_init_handle == NULL)
    {
        // 初始化ADC
        continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &ADC_init_handle);
    }
    if (ADC_init_handle != NULL)
    {
        // 启动ADC连续读取
        esp_err_t err = adc_continuous_start(ADC_init_handle);
        if (err == ESP_OK)
        {
            adc_running = true;
        }
        return err;
    }

    return ESP_OK;
}

// 停止ADC采集
esp_err_t stop_adc_sampling(void)
{
    if (ADC_init_handle != NULL && adc_running)
    {
        esp_err_t err = adc_continuous_stop(ADC_init_handle);
        if (err == ESP_OK)
        {
            adc_running = false;
        }
        return err;
    }

    return ESP_OK;
}

// 添加函数用于反初始化ADC
esp_err_t deinit_adc(void)
{
    if (ADC_init_handle != NULL)
    {
        if (adc_running)
        {
            adc_continuous_stop(ADC_init_handle);
        }
        esp_err_t err = adc_continuous_deinit(ADC_init_handle);
        ADC_init_handle = NULL;
        adc_running = false;
        
        // // 反初始化ADC校准
        // if (adc_calibration_enabled && adc1_cali_handle) {
        //     adc_calibration_deinit(adc1_cali_handle);
        //     adc1_cali_handle = NULL;
        //     adc_calibration_enabled = false;
        // }
        
        return err;
    }

    return ESP_OK;
}

// GPIO -----------------------------------------------------------------------------------------------------
// XYAB按键回调函数
static void key_x_pressed_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupSetBits(xyab_button_event_group, XYAB_KEY_X_PRESSED);
    }
}

static void key_x_released_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupClearBits(xyab_button_event_group, XYAB_KEY_X_PRESSED);
    }
}

static void key_y_pressed_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupSetBits(xyab_button_event_group, XYAB_KEY_Y_PRESSED);
    }
}

static void key_y_released_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupClearBits(xyab_button_event_group, XYAB_KEY_Y_PRESSED);
    }
}

static void key_a_pressed_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupSetBits(xyab_button_event_group, XYAB_KEY_A_PRESSED);
    }
}

static void key_a_released_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupClearBits(xyab_button_event_group, XYAB_KEY_A_PRESSED);
    }
}

static void key_b_pressed_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupSetBits(xyab_button_event_group, XYAB_KEY_B_PRESSED);
    }
}

static void key_b_released_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupClearBits(xyab_button_event_group, XYAB_KEY_B_PRESSED);
    }
}

static void left_shoulder_btn_pressed_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupSetBits(xyab_button_event_group, LEFT_SHOULDER_BTN_PRESSED);
    }
}

static void left_shoulder_btn_released_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupClearBits(xyab_button_event_group, LEFT_SHOULDER_BTN_PRESSED);
    }
}

static void right_shoulder_btn_pressed_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupSetBits(xyab_button_event_group, RIGHT_SHOULDER_BTN_PRESSED);
    }
}

static void right_shoulder_btn_released_cb(void *arg, void *usr_data)
{
    if (xyab_button_event_group != NULL)
    {
        xEventGroupClearBits(xyab_button_event_group, RIGHT_SHOULDER_BTN_PRESSED);
    }
}

// 其他按键回调函数
static void left_joystick_btn_pressed_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupSetBits(other_button_event_group, LEFT_JOYSTICK_BTN_PRESSED);
    }
}

static void left_joystick_btn_released_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupClearBits(other_button_event_group, LEFT_JOYSTICK_BTN_PRESSED);
    }
}

static void right_joystick_btn_pressed_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupSetBits(other_button_event_group, RIGHT_JOYSTICK_BTN_PRESSED);
    }
}

static void right_joystick_btn_released_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupClearBits(other_button_event_group, RIGHT_JOYSTICK_BTN_PRESSED);
    }
}

static void select_btn_pressed_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupSetBits(other_button_event_group, SELECT_BTN_PRESSED);
    }
}

static void select_btn_released_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupClearBits(other_button_event_group, SELECT_BTN_PRESSED);
    }
}

static void start_btn_pressed_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupSetBits(other_button_event_group, START_BTN_PRESSED);
    }
}

static void start_btn_released_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupClearBits(other_button_event_group, START_BTN_PRESSED);
    }
}

static void ikey_btn_pressed_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupSetBits(other_button_event_group, IKEY_BTN_PRESSED);
    }
}

static void ikey_btn_released_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupClearBits(other_button_event_group, IKEY_BTN_PRESSED);
    }
}

static void ios_btn_pressed_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupSetBits(other_button_event_group, IOS_BTN_PRESSED);
    }
}

static void ios_btn_released_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupClearBits(other_button_event_group, IOS_BTN_PRESSED);
    }
}

static void windows_btn_pressed_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupSetBits(other_button_event_group, WINDOWS_BTN_PRESSED);
    }
}

static void windows_btn_released_cb(void *arg, void *usr_data)
{
    if (other_button_event_group != NULL)
    {
        xEventGroupClearBits(other_button_event_group, WINDOWS_BTN_PRESSED);
    }
}

// button初始化
static void init_gpio(void)
{
    // 创建XYAB按键事件组
    if (xyab_button_event_group == NULL)
    {
        xyab_button_event_group = xEventGroupCreate();
        if (xyab_button_event_group == NULL)
        {
            ESP_LOGE(TAG, "Failed to create XYAB button event group");
            return;
        }
    }

    // 创建其他按键事件组
    if (other_button_event_group == NULL)
    {
        other_button_event_group = xEventGroupCreate();
        if (other_button_event_group == NULL)
        {
            ESP_LOGE(TAG, "Failed to create other button event group");
            return;
        }
    }

    // 初始化XYAB按键
    const button_config_t btn_cfg = {0};

    // 初始化按键X
    const button_gpio_config_t btn_gpio_cfg_x = {
        .gpio_num = GPIO_INPUT_KEY_X,
        .active_level = 0, // 按下为低电平
    };
    button_handle_t gpio_btn_x = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_x, &gpio_btn_x);
    if (ret != ESP_OK || gpio_btn_x == NULL)
    {
        ESP_LOGE(TAG, "Failed to create button X");
    }
    else
    {
        iot_button_register_cb(gpio_btn_x, BUTTON_PRESS_DOWN, NULL, key_x_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_x, BUTTON_PRESS_UP, NULL, key_x_released_cb, NULL);
    }

    // 初始化按键Y
    const button_gpio_config_t btn_gpio_cfg_y = {
        .gpio_num = GPIO_INPUT_KEY_Y,
        .active_level = 0, // 按下为低电平
    };
    button_handle_t gpio_btn_y = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_y, &gpio_btn_y);
    if (ret != ESP_OK || gpio_btn_y == NULL)
    {
        ESP_LOGE(TAG, "Failed to create button Y");
    }
    else
    {
        iot_button_register_cb(gpio_btn_y, BUTTON_PRESS_DOWN, NULL, key_y_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_y, BUTTON_PRESS_UP, NULL, key_y_released_cb, NULL);
    }

    // 初始化按键A
    const button_gpio_config_t btn_gpio_cfg_a = {
        .gpio_num = GPIO_INPUT_KEY_A,
        .active_level = 0, // 按下为低电平
    };
    button_handle_t gpio_btn_a = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_a, &gpio_btn_a);
    if (ret != ESP_OK || gpio_btn_a == NULL)
    {
        ESP_LOGE(TAG, "Failed to create button A");
    }
    else
    {
        iot_button_register_cb(gpio_btn_a, BUTTON_PRESS_DOWN, NULL, key_a_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_a, BUTTON_PRESS_UP, NULL, key_a_released_cb, NULL);
    }

    // 初始化按键B
    const button_gpio_config_t btn_gpio_cfg_b = {
        .gpio_num = GPIO_INPUT_KEY_B,
        .active_level = 0, // 按下为低电平
    };
    button_handle_t gpio_btn_b = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_b, &gpio_btn_b);
    if (ret != ESP_OK || gpio_btn_b == NULL)
    {
        ESP_LOGE(TAG, "Failed to create button B");
    }
    else
    {
        iot_button_register_cb(gpio_btn_b, BUTTON_PRESS_DOWN, NULL, key_b_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_b, BUTTON_PRESS_UP, NULL, key_b_released_cb, NULL);
    }

    // 左肩键
    const button_gpio_config_t btn_gpio_cfg_left_shoulder = {
        .gpio_num = GPIO_INPUT_LEFT_SHOULDER_BTN,
        .active_level = 0,
    };
    button_handle_t gpio_btn_left_shoulder = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_left_shoulder, &gpio_btn_left_shoulder);
    if (ret != ESP_OK || gpio_btn_left_shoulder == NULL)
    {
        ESP_LOGE(TAG, "Failed to create left shoulder button");
    }
    else
    {
        iot_button_register_cb(gpio_btn_left_shoulder, BUTTON_PRESS_DOWN, NULL, left_shoulder_btn_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_left_shoulder, BUTTON_PRESS_UP, NULL, left_shoulder_btn_released_cb, NULL);
    }

    // 右肩键
    const button_gpio_config_t btn_gpio_cfg_right_shoulder = {
        .gpio_num = GPIO_INPUT_RIGHT_SHOULDER_BTN,
        .active_level = 0,
    };
    button_handle_t gpio_btn_right_shoulder = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_right_shoulder, &gpio_btn_right_shoulder);
    if (ret != ESP_OK || gpio_btn_right_shoulder == NULL)
    {
        ESP_LOGE(TAG, "Failed to create right shoulder button");
    }
    else
    {
        iot_button_register_cb(gpio_btn_right_shoulder, BUTTON_PRESS_DOWN, NULL, right_shoulder_btn_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_right_shoulder, BUTTON_PRESS_UP, NULL, right_shoulder_btn_released_cb, NULL);
    }

    // 初始化其他按键
    // 左摇杆按键
    const button_gpio_config_t btn_gpio_cfg_left_joystick = {
        .gpio_num = GPIO_INPUT_LEFT_JOYSTICK_BTN,
        .active_level = 0,
    };
    button_handle_t gpio_btn_left_joystick = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_left_joystick, &gpio_btn_left_joystick);
    if (ret != ESP_OK || gpio_btn_left_joystick == NULL)
    {
        ESP_LOGE(TAG, "Failed to create left joystick button");
    }
    else
    {
        iot_button_register_cb(gpio_btn_left_joystick, BUTTON_PRESS_DOWN, NULL, left_joystick_btn_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_left_joystick, BUTTON_PRESS_UP, NULL, left_joystick_btn_released_cb, NULL);
    }

    // 右摇杆按键
    const button_gpio_config_t btn_gpio_cfg_right_joystick = {
        .gpio_num = GPIO_INPUT_RIGHT_JOYSTICK_BTN,
        .active_level = 0,
    };
    button_handle_t gpio_btn_right_joystick = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_right_joystick, &gpio_btn_right_joystick);
    if (ret != ESP_OK || gpio_btn_right_joystick == NULL)
    {
        ESP_LOGE(TAG, "Failed to create right joystick button");
    }
    else
    {
        iot_button_register_cb(gpio_btn_right_joystick, BUTTON_PRESS_DOWN, NULL, right_joystick_btn_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_right_joystick, BUTTON_PRESS_UP, NULL, right_joystick_btn_released_cb, NULL);
    }

    // SELECT按键
    const button_gpio_config_t btn_gpio_cfg_select = {
        .gpio_num = GPIO_INPUT_SELECT_BTN,
        .active_level = 0,
    };
    button_handle_t gpio_btn_select = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_select, &gpio_btn_select);
    if (ret != ESP_OK || gpio_btn_select == NULL)
    {
        ESP_LOGE(TAG, "Failed to create select button");
    }
    else
    {
        iot_button_register_cb(gpio_btn_select, BUTTON_PRESS_DOWN, NULL, select_btn_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_select, BUTTON_PRESS_UP, NULL, select_btn_released_cb, NULL);
    }

    // START按键
    const button_gpio_config_t btn_gpio_cfg_start = {
        .gpio_num = GPIO_INPUT_START_BTN,
        .active_level = 0,
    };
    button_handle_t gpio_btn_start = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_start, &gpio_btn_start);
    if (ret != ESP_OK || gpio_btn_start == NULL)
    {
        ESP_LOGE(TAG, "Failed to create start button");
    }
    else
    {
        iot_button_register_cb(gpio_btn_start, BUTTON_PRESS_DOWN, NULL, start_btn_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_start, BUTTON_PRESS_UP, NULL, start_btn_released_cb, NULL);
    }

    // IKEY按键
    const button_gpio_config_t btn_gpio_cfg_ikey = {
        .gpio_num = GPIO_INPUT_IKEY_BTN,
        .active_level = 0,
    };
    button_handle_t gpio_btn_ikey = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_ikey, &gpio_btn_ikey);
    if (ret != ESP_OK || gpio_btn_ikey == NULL)
    {
        ESP_LOGE(TAG, "Failed to create ikey button");
    }
    else
    {
        iot_button_register_cb(gpio_btn_ikey, BUTTON_PRESS_DOWN, NULL, ikey_btn_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_ikey, BUTTON_PRESS_UP, NULL, ikey_btn_released_cb, NULL);
    }

    // IOS按键
    const button_gpio_config_t btn_gpio_cfg_ios = {
        .gpio_num = GPIO_INPUT_IOS_BTN,
        .active_level = 0,
    };
    button_handle_t gpio_btn_ios = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_ios, &gpio_btn_ios);
    if (ret != ESP_OK || gpio_btn_ios == NULL)
    {
        ESP_LOGE(TAG, "Failed to create ios button");
    }
    else
    {
        iot_button_register_cb(gpio_btn_ios, BUTTON_PRESS_DOWN, NULL, ios_btn_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_ios, BUTTON_PRESS_UP, NULL, ios_btn_released_cb, NULL);
    }

    // Windows按键
    const button_gpio_config_t btn_gpio_cfg_windows = {
        .gpio_num = GPIO_INPUT_WINDOWS_BTN,
        .active_level = 0,
    };
    button_handle_t gpio_btn_windows = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg_windows, &gpio_btn_windows);
    if (ret != ESP_OK || gpio_btn_windows == NULL)
    {
        ESP_LOGE(TAG, "Failed to create windows button");
    }
    else
    {
        iot_button_register_cb(gpio_btn_windows, BUTTON_PRESS_DOWN, NULL, windows_btn_pressed_cb, NULL);
        iot_button_register_cb(gpio_btn_windows, BUTTON_PRESS_UP, NULL, windows_btn_released_cb, NULL);
    }
}
// BLE --------------------------------------------------------------------------------------------------------------------

#define HID_BLE_TAG "BLEinfo"
#define HIDD_DEVICE_NAME "ESP32GamePad"
uint16_t hid_conn_id = 0;
bool sec_conn = false;

// 原始广播数据包
static uint8_t hidd_adv_data_raw[] = {
    0x02, ESP_BLE_AD_TYPE_FLAG, 0x06,             // Flags: LE General Discoverable Mode, BR/EDR Not Supported
    0x03, ESP_BLE_AD_TYPE_16SRV_PART, 0x12, 0x18, // 部分16位UUID
    0x0D, ESP_BLE_AD_TYPE_NAME_CMPL, 'E', 'S', 'P', '3', '2', 'G', 'a', 'm', 'e', 'P', 'a', 'd',
    // 0x0D, 0x09,                 // Length of Device Name + 1 byte for length field
    // 'E', 'S', 'P', '3', '2', 'g', 'a', 'm', 'e', 'P','a','d', // Device Name
    0x02, ESP_BLE_AD_TYPE_TX_PWR, 0xEB, // TX Power Level (0x00 corresponds to -21 dBm)
};

static uint8_t raw_scan_rsp_data[] = {
    /* Complete Local Name */
    0x0D, ESP_BLE_AD_TYPE_NAME_CMPL, 'E', 'S', 'P', '3', '2', 'G', 'a', 'm', 'e', 'P', 'a', 'd', // Length 13, Data Type ESP_BLE_AD_TYPE_NAME_CMPL, Data (ESP_GATTS_DEMO)
    0x03, ESP_BLE_AD_TYPE_16SRV_PART, 0x12, 0x18,                                                // 部分16位UUID
};

// 广播参数
static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min = 0x20, // 设置广播间隔的最小和最大值
    .adv_int_max = 0x30,
    .adv_type = ADV_TYPE_IND,              // 广播类型为可连接的无定向广播
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC, // 使用固定地址广播（设备蓝牙MAC地址）
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map = ADV_CHNL_ALL,                            // 广播频道为所有频道（37、38、39）
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY, // 广播过滤策略：不过滤
};

// GATT回调
static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    // build_hidd_adv_data_raw();//构建自己的广播包raw
    switch (event)
    {
    case ESP_HIDD_EVENT_REG_FINISH:
    {
        if (param->init_finish.state == ESP_HIDD_INIT_OK)
        {
            esp_ble_gap_set_device_name(HIDD_DEVICE_NAME);
            if (ESP_OK == esp_ble_gap_config_adv_data_raw(hidd_adv_data_raw, sizeof(hidd_adv_data_raw)))
            {
                if (ESP_OK == esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data)))
                {
                    ESP_LOGI("HIDDcallback", "GAP Config Adv Data OK");
                }
            }
            else
            {
                ESP_LOGI("HIDDcallback", "GAP Config Adv Data Failed");
            }
        }
        break;
    }
    case ESP_BAT_EVENT_REG:
    {
        break;
    }
    case ESP_HIDD_EVENT_DEINIT_FINISH:
        break;
    // HIDD连接事件
    case ESP_HIDD_EVENT_BLE_CONNECT:
    {
        ESP_LOGI("HIDDcallback", "ESP_HIDD_EVENT_BLE_CONNECT");
        // 记录连接id，后续要使用
        hid_conn_id = param->connect.conn_id;
        // 在连接建立时停止ADC采样
        ESP_LOGI("HIDDcallback", "Stopping ADC sampling during connection");

        // stop_adc_sampling();
        break;
    }
    case ESP_HIDD_EVENT_BLE_DISCONNECT:
    {
        sec_conn = false;
        ESP_LOGI("HIDDcallback", "ESP_HIDD_EVENT_BLE_DISCONNECT");
        current_device_state = DEVICE_STATE_DISCONNECTED;
        // 断连后重新advertising
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
    }
    case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT:
    {
        ESP_LOGI("HIDDcallback", "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT", __func__);
        ESP_LOG_BUFFER_HEX("HIDDcallback", param->vendor_write.data, param->vendor_write.length);
        break;
    }
    case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT:
    {
        ESP_LOGI("HIDDcallback", "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
        ESP_LOG_BUFFER_HEX("HIDDcallback", param->led_write.data, param->led_write.length);
        break;
    }
    default:
        break;
    }
    return;
}

// GAP回调
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: // 设置广播数据成功事件，When advertising data set complete, the event comes
        ESP_LOGI(HID_BLE_TAG, "ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT");
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: // 设置广播数据成功事件，When advertising data set complete, the event comes
        ESP_LOGI(HID_BLE_TAG, "ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT");
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT: // 安全请求事件，BLE security request
        for (int i = 0; i < ESP_BD_ADDR_LEN; i++)
        {
            ESP_LOGD(HID_BLE_TAG, "%x:", param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: // 认证完成事件
        ESP_LOGW("GAP", "Auth OK");
        current_device_state = DEVICE_STATE_CONNECTED;
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t)); // 复制地址
        ESP_LOGI(HID_BLE_TAG, "remote BD_ADDR: %08x%04x",
                 (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                 (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(HID_BLE_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(HID_BLE_TAG, "pair status = %s", param->ble_security.auth_cmpl.success ? "success" : "fail");
        if (!param->ble_security.auth_cmpl.success)
        {
            ESP_LOGE(HID_BLE_TAG, "fail reason = 0x%x", param->ble_security.auth_cmpl.fail_reason);
        }
        sec_conn = true;
        // 连接稳定后重新启动ADC采样
        ESP_LOGI(HID_BLE_TAG, "Starting ADC sampling after connection established");
        // start_adc_sampling();
        // 添加短暂延迟，确保系统状态完全稳定后再允许ADC访问
        vTaskDelay(pdMS_TO_TICKS(50));

        break;
    default:
        break;
    }
}

esp_err_t ble_init(void)
{
    // 初始化FLASH，NVS 初始化（自带函数）：用于存储蓝牙配对信息等。
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 如果没有可用空间，初始化失败则擦除
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化蓝牙控制器
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));  // 经典蓝牙内存释放
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT(); // 指向蓝牙控制器配置结构体的指针，用于bt_cfg初始化参数
    // 使用默认参数进行初始化控制器
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK)
    {
        ESP_LOGE("bleInit", "%s initialize controller failed", __func__);
        return ESP_FAIL;
    }
    // 使能BLE蓝牙控制器
    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK)
    {
        ESP_LOGE("bleInit", "%s enable controller failed", __func__);
        return ESP_FAIL;
    }
    // 初始化蓝牙开发框架bluedroid
    if ((ret = esp_bluedroid_init()) != ESP_OK)
    {
        ESP_LOGE("bleInit", "%s init bluedroid failed", __func__);
        return ESP_FAIL;
    }
    // 启动bluedroid
    if ((ret = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE("bleInit", "%s init bluedroid failed", __func__);
        return ESP_FAIL;
    }
    // 初始化HID设备
    if ((ret = esp_hidd_profile_init()) != ESP_OK)
    {
        ESP_LOGE("bleInit", "HID init failed");
    }

    // 注册GAP事件回调函数，当 BLE GAP 层发生某些事件（例如连接、断开连接、扫描结果等）时，系统会调用 gap_event_handler 函数，并将相应的事件信息传递给它
    // 传入了函数指针，用于自定义gap event的回调函数
    esp_ble_gap_register_callback(gap_event_handler);
    // 使用GATT注册HID设备事件回调函数
    esp_hidd_register_callbacks(hidd_event_callback);

    return ESP_OK;
}

// 配置蓝牙安全参数
esp_err_t ble_sec_config(void)
{
    /* 安全参数配置*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;                // 认证后与对端设备绑定（保存配对信息，便于以后快速连接）
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;                      // 表示设备没有输入和输出能力，无法通过按键、显示等方式进行交互认证
    uint8_t key_size = 16;                                         // 密钥长度
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK; // 加密密钥（用于数据加密）与身份密钥（用于设备身份识别）。
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    /* 设置安全参数*/
    // 没有IO，因此无需响应
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    return ESP_OK;
}

//-------------------------------------------------------------------------------------

volatile device_state_t current_device_state = DEVICE_STATE_INIT;
void init_all(void)
{
    led_strip = configure_led();
    if (led_strip != NULL)
    {
        ESP_LOGI("init", "ws1812 Init OK");
    }
    // init_adc();
    init_gpio();
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &ADC_init_handle);
    if (ESP_OK == ble_init())
    {
        ble_sec_config();
    }
    ESP_LOGI("init", "BLE HID Init OK");
}