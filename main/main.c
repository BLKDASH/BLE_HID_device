#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_hidd_prf_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "driver/gpio.h"
#include "hid_dev.h"
#include "main.h"

#include "sdkconfig.h"
#include "iot_button.h"
#include "led_strip.h"
#include "hardware_init.h"

//todo:遗忘上一次连接的设备
//todo:修改ADC库和ADC校准库
//todo:修改为连续ADC


#define HID_BLE_TAG "BLEinfo"
#define HID_TASK_TAG "TASKinfo"

static uint16_t hid_conn_id = 0;
static bool sec_conn = false;

#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param);

#define HIDD_DEVICE_NAME "ESP32GamePad"
// #define HIDD_DEVICE_NAME            "MYGT Controller"


void app_main(void)
{
    init_all();
    if (ESP_OK == ble_init())
    {
        ble_sec_config();
    }
    ESP_LOGI("Main", "BLE HID Init OK");

    xTaskCreatePinnedToCore(blink_task, "blink_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(gpio_read_task, "gpio_toggle_task", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(adc_read_task, "adc_read_task", 4096, NULL, 7, NULL, 1);
    // 模拟手柄任务
    xTaskCreate(&gamepad_button_task, "gamepad_button_task", 4096, NULL, 9, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}






// 原始广播数据包
uint8_t hidd_adv_data_raw[] = {
    0x02, ESP_BLE_AD_TYPE_FLAG, 0x06,           // Flags: LE General Discoverable Mode, BR/EDR Not Supported
    0x03, ESP_BLE_AD_TYPE_16SRV_PART, 0x12, 0x18,     // 部分16位UUID
    0x0D, ESP_BLE_AD_TYPE_NAME_CMPL, 'E', 'S', 'P', '3', '2', 'G', 'a', 'm', 'e', 'P','a','d',
    // 0x0D, 0x09,                 // Length of Device Name + 1 byte for length field
    // 'E', 'S', 'P', '3', '2', 'g', 'a', 'm', 'e', 'P','a','d', // Device Name
    0x02, ESP_BLE_AD_TYPE_TX_PWR, 0xEB,           // TX Power Level (0x00 corresponds to -21 dBm)
};

static uint8_t raw_scan_rsp_data[] = {
    /* Complete Local Name */
    0x0D, ESP_BLE_AD_TYPE_NAME_CMPL, 'E', 'S', 'P', '3', '2', 'G', 'a', 'm', 'e', 'P','a','d',   // Length 13, Data Type ESP_BLE_AD_TYPE_NAME_CMPL, Data (ESP_GATTS_DEMO)
    0x03, ESP_BLE_AD_TYPE_16SRV_PART, 0x12, 0x18,
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
            if(ESP_OK==esp_ble_gap_config_adv_data_raw(hidd_adv_data_raw, sizeof(hidd_adv_data_raw)))
            {
                if(ESP_OK==esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data)))
                {ESP_LOGI("HIDDcallback","GAP Config Adv Data OK");}
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
        break;
    }
    case ESP_HIDD_EVENT_BLE_DISCONNECT:
    {
        sec_conn = false;
        ESP_LOGI("HIDDcallback", "ESP_HIDD_EVENT_BLE_DISCONNECT");
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
        sec_conn = true;
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
        break;
    default:
        break;
    }
}





















void blink_task(void *pvParameter)
{ 
    bool led_on_off = false;
    setLED(0, 0, 10, 0);
    setLED(1, 10, 0, 0);
    setLED(2, 10, 0, 10);
    setLED(3, 10, 10, 0);
    while(1)
    {
        if (led_on_off) {
            setLED(0, 0, 10, 0);
            //ESP_LOGI("main", "LED ON!");
        } 
        else 
        {
            if(sec_conn)
            {
            }
            else
            {
                setLED(0, 0, 0, 0);
            }
            //ESP_LOGI("main", "LED OFF!");
        }
        
        led_on_off = !led_on_off;
        vTaskDelay(pdMS_TO_TICKS(400)); 
    }
}

void gpio_read_task(void *pvParameter)
{
    int level_25, level_26, level_27, level_14;
    int level_15, level_19;
    int level_23, level_18;
    int level_4, level_2, level_13, level_0, level_21, level_22;

    while (1) {
        // Toggle GPIO32

        // Read input GPIO levels
        level_25 = gpio_get_level(GPIO_INPUT_KEY_X);
        level_26 = gpio_get_level(GPIO_INPUT_KEY_Y);
        level_27 = gpio_get_level(GPIO_INPUT_KEY_A);
        level_14 = gpio_get_level(GPIO_INPUT_KEY_B);


        level_15 = gpio_get_level(GPIO_INPUT_LEFT_JOYSTICK_BTN);
        level_19 = gpio_get_level(GPIO_INPUT_RIGHT_JOYSTICK_BTN);
        
        level_23 = gpio_get_level(GPIO_INPUT_LEFT_SHOULDER_BTN);
        level_18 = gpio_get_level(GPIO_INPUT_RIGHT_SHOULDER_BTN);

        level_4 = gpio_get_level(GPIO_INPUT_SELECT_BTN);
        level_2 = gpio_get_level(GPIO_INPUT_START_BTN);
        level_13 = gpio_get_level(GPIO_INPUT_HOME_BTN);
        level_0 = gpio_get_level(GPIO_INPUT_IKEY_BTN);
        level_21 = gpio_get_level(GPIO_INPUT_IOS_BTN);
        level_22 = gpio_get_level(GPIO_INPUT_WINDOWS_BTN);

        // Log levels
        ESP_LOGI("ioTask", "X: %d | Y: %d | A: %d | B: %d",
                 level_25, level_26, level_27, level_14);
        ESP_LOGI("ioTask", "LS: %d | RS: %d",
                 level_15, level_19);
        ESP_LOGI("ioTask", "Shoulders - Left: %d | Right: %d",
                 level_23, level_18);
        ESP_LOGI("ioTask", "Special Keys - SELECT: %d | START: %d | HOME: %d | IKEY: %d | IOS: %d | WINDOWS: %d",
                 level_4, level_2, level_13, level_0, level_21, level_22);

        

        // Delay 500ms
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void adc_read_task(void *pvParameter)
{
    while (1) {
        read_and_log_adc_values();
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}






void gamepad_button_task(void *pvParameters)
{

    while (1)
    {
        if (sec_conn)
        {
            vTaskDelay(pdMS_TO_TICKS(80));
            esp_hidd_send_gamepad_report(hid_conn_id);
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10000));
            ESP_LOGI(HID_TASK_TAG, "Waiting for connection...");
        }
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


