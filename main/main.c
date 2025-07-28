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
/*
简介：
该示例实现了 BLE HID（蓝牙低功耗人机接口设备）设备配置文件相关功能。此 HID 设备包含 4 种报告（Report）：
鼠标（Report 1）
键盘和 LED（Report 2）
消费类设备（如音量控制，Report 3）
厂商自定义设备（Report 4）
用户可以根据自己的应用场景选择不同的报告类型。BLE HID 配置文件继承了 USB HID 类的功能特性。

注意事项：
Windows 10 不支持厂商自定义报告（Vendor Report），因此 SUPPORT_REPORT_VENDOR 始终设置为 FALSE，该定义位于 hidd_le_prf_int.h 文件中。
在 iPhone 的 HID 加密期间不允许更新连接参数，因此从设备会在加密期间关闭自动更新连接参数的功能。
当我们的 HID 设备连接后，iPhone 会向 Report 特性配置描述符写入 1，即使 HID 加密尚未完成。实际上，应该在加密完成后才写入 1。为此，我们将 Report 特性配置描述符的权限修改为 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED。如果出现 GATT_INSUF_ENCRYPTION 错误，请忽略该错误。
 */

#define HID_BLE_TAG "BLEinfo"

// 128，API不允许16位
#define UUID_MOD 128

static uint16_t hid_conn_id = 0;
static bool sec_conn = false;

#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param);

#define HIDD_DEVICE_NAME "ESP32GamePad"
// #define HIDD_DEVICE_NAME            "MYGT Controller"
//  UUID为0x1812，注册为HID设备
#if (UUID_MOD == 128)
// 向外展示1812的HID标志
static uint8_t hidd_service_uuid[] =
    {
        /* LSB <--------------------------------------------------------------------------------> MSB */
        // first uuid, 16bit, [12],[13] is the value
        0xfb,
        0x34,
        0x9b,
        0x5f,
        0x80,
        0x00,
        0x00,
        0x80,
        0x00,
        0x10,
        0x00,
        0x00,
        0x12,
        0x18,
        0x00,
        0x00,
};
#endif

#if (UUID_MOD == 16)
static uint8_t hidd_service_uuid[] = {
    /* LSB <------> MSB */
    0x12,
    0x18,
};
// static uint16_t hidd_service_uuid16 = 0x1812;
#endif

// GATT 广播数据
static esp_ble_adv_data_t hidd_adv_data =
    {
        .set_scan_rsp = false,                          // 是否设置扫描回复数据
        .include_name = true,                           // 是否包含设备名
        .include_txpower = true,                        // 是否包含信号强度
        .min_interval = 0x0006,                         // 从设备连接的最小间隔时间，单位为 1.25ms，0x0006 对应 7.5ms。
        .max_interval = 0x0010,                         // 从设备连接的最大间隔时间，单位为 1.25ms，0x0010 对应 20ms。
        .appearance = 0x03c4,                           // 设备外观标识，0x03c0 表示 HID 通用设备。03c4表示HID游戏手柄
        .manufacturer_len = 0,                          // 厂商数据长度，0 表示没有厂商数据。
        .p_manufacturer_data = NULL,                    // 指向厂商数据的指针，NULL 表示无厂商数据。
        .service_data_len = 0,                          // 服务数据长度，0 表示没有服务数据。
        .p_service_data = NULL,                         // 指向服务数据的指针，NULL 表示无服务数据。
        .service_uuid_len = sizeof(hidd_service_uuid),  // 服务数据长度UUID
        .p_service_uuid = (uint8_t *)hidd_service_uuid, // uuid指针
        .flag = 0x7,                                    // 0b00000111
                                                        // bit 0: 有限发现模式
                                                        // bit 1: LE General Discoverable Mode同时支持通用发现模式
                                                        // bit 2: BR/EDR Not Supported（不支持普通蓝牙）
};

#define HID_SERVICE_UUID_16 0x1812

// 原始广播数据缓冲区
static uint8_t hidd_adv_data_raw[31] = {0}; // 蓝牙广播最大31字节
static uint8_t hidd_adv_data_raw_len = 0;

// 构建符合AD规范的原始广播数据
void build_hidd_adv_data_raw()
{
    uint8_t idx = 0;

    // 1.  Flags (0x01) - 0x07表示LE General Discoverable Mode且不支持BR/EDR
    hidd_adv_data_raw[idx++] = 0x02; // 长度: 2字节(类型+数据)
    hidd_adv_data_raw[idx++] = 0x01; // 类型: Flags
    hidd_adv_data_raw[idx++] = 0x07; // 数据: 0b00000111

    // 2. 完整本地名称 (0x09)
    const char *device_name = "ESP32-HID";              // 替换为你的设备名
    hidd_adv_data_raw[idx++] = strlen(device_name) + 1; // 长度: 名称长度+1(类型)
    hidd_adv_data_raw[idx++] = 0x09;                    // 类型: 完整本地名称
    memcpy(&hidd_adv_data_raw[idx], device_name, strlen(device_name));
    idx += strlen(device_name);

    // 3. 发射功率 (0x0A)
    hidd_adv_data_raw[idx++] = 0x02; // 长度: 2字节
    hidd_adv_data_raw[idx++] = 0x0A; // 类型: 发射功率
    hidd_adv_data_raw[idx++] = 0x00; // 功率值(0dBm，可根据实际调整)

    // 4. 16位服务UUID列表 (0x03)
    hidd_adv_data_raw[idx++] = 0x03;                              // 长度: 3字节(类型+2字节UUID)
    hidd_adv_data_raw[idx++] = 0x03;                              // 类型: 完整16位服务UUID列表
    hidd_adv_data_raw[idx++] = HID_SERVICE_UUID_16 & 0xFF;        // UUID低8位
    hidd_adv_data_raw[idx++] = (HID_SERVICE_UUID_16 >> 8) & 0xFF; // UUID高8位

    // 5. 连接间隔范围 (0x12)
    hidd_adv_data_raw[idx++] = 0x05; // 长度: 5字节(类型+4字节数据)
    hidd_adv_data_raw[idx++] = 0x12; // 类型: 连接间隔范围
    // 最小间隔(0x0006) - 小端模式
    hidd_adv_data_raw[idx++] = 0x06;
    hidd_adv_data_raw[idx++] = 0x00;
    // 最大间隔(0x0010) - 小端模式
    hidd_adv_data_raw[idx++] = 0x10;
    hidd_adv_data_raw[idx++] = 0x00;

    // 6. 外观特征 (0x19)
    hidd_adv_data_raw[idx++] = 0x03; // 长度: 3字节
    hidd_adv_data_raw[idx++] = 0x19; // 类型: 外观
    hidd_adv_data_raw[idx++] = 0xc4; // 外观值0x03c4低8位
    hidd_adv_data_raw[idx++] = 0x03; // 外观值0x03c4高8位

    // 确认总长度不超过31字节
    hidd_adv_data_raw_len = idx;
    assert(hidd_adv_data_raw_len <= 31);
}

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

// ESP32 BLE HID设备的事件回调函数，处理蓝牙连接、断开、数据写入等事件。
//  1. **ESP_HIDD_EVENT_REG_FINISH**：HID设备注册完成后设置设备名并配置广播数据。
//  2. **ESP_HIDD_EVENT_BLE_CONNECT**：设备连接时记录连接ID。
//  3. **ESP_HIDD_EVENT_BLE_DISCONNECT**：设备断开连接后重新开始广播。
//  4. **ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT**：接收到厂商报告数据时打印日志。
//  5. **ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT**：接收到LED报告数据时打印日志。
static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    // build_hidd_adv_data_raw();//构建自己的广播包raw
    switch (event)
    {
    case ESP_HIDD_EVENT_REG_FINISH:
    {
        if (param->init_finish.state == ESP_HIDD_INIT_OK)
        {
            // esp_bd_addr_t rand_addr = {0x04,0x11,0x11,0x11,0x11,0x05};
            esp_ble_gap_set_device_name(HIDD_DEVICE_NAME);
            // 该API不支持16位UUID
            if (ESP_OK == esp_ble_gap_config_adv_data(&hidd_adv_data))
            {
                ESP_LOGI("HIDD_CALLBACK", "GAP Config Adv Data OK");
            }
            // if(ESP_OK==esp_ble_gap_config_adv_data_raw(hidd_adv_data_raw, hidd_adv_data_raw_len))
            // {
            //     ESP_LOGI("HIDD_CALLBACK","GAP Config Adv Data OK");
            // }
            else
            {
                ESP_LOGI("HIDD_CALLBACK", "GAP Config Adv Data Failed");
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
        ESP_LOGI(HID_BLE_TAG, "ESP_HIDD_EVENT_BLE_CONNECT");
        // 记录连接id，后续要使用
        hid_conn_id = param->connect.conn_id;
        break;
    }
    case ESP_HIDD_EVENT_BLE_DISCONNECT:
    {
        sec_conn = false;
        ESP_LOGI(HID_BLE_TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
        // 断连后重新advertising
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
    }
    case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT:
    {
        ESP_LOGI(HID_BLE_TAG, "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT", __func__);
        ESP_LOG_BUFFER_HEX(HID_BLE_TAG, param->vendor_write.data, param->vendor_write.length);
        break;
    }
    case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT:
    {
        ESP_LOGI(HID_BLE_TAG, "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
        ESP_LOG_BUFFER_HEX(HID_BLE_TAG, param->led_write.data, param->led_write.length);
        break;
    }
    default:
        break;
    }
    return;
}

// GAP回调函数
// 三种GAP事件处理函数，分别处理连接、加密、认证事件。
//  ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT
//  广播数据设置完成后启动 BLE 广播。
// ESP_GAP_BLE_SEC_REQ_EVT
// 收到安全请求时，打印设备地址并发送安全响应。
// ESP_GAP_BLE_AUTH_CMPL_EVT
// 认证完成后，记录安全连接标志，打印远程设备地址、地址类型、配对状态及失败原因
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: // 设置广播数据成功事件，When advertising data set complete, the event comes
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

/**
 * @brief HID演示任务函数
 *
 * 该任务负责定时发送HID音量控制指令，首先发送音量增大指令，
 * 然后在适当延迟后发送音量减小指令。只有在安全连接状态下才会发送指令。
 *
 * @param pvParameters 任务参数（未使用）
 */
void hid_demo_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    while (1)
    {
        // 当前为安全连接时执行HID控制逻辑
        if (sec_conn)
        {
            ESP_LOGI(HID_BLE_TAG, "Send the volume");
            // 间隔5s
            vTaskDelay(pdMS_TO_TICKS(5000));
            // 发送音量增大
            esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_UP, true);
            vTaskDelay(pdMS_TO_TICKS(200));
            // 关闭音量增大
            esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_UP, false);
            // 间隔5s
            vTaskDelay(pdMS_TO_TICKS(5000));
            // 发送音量减小
            esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_DOWN, true);
            vTaskDelay(pdMS_TO_TICKS(200));
            // 关闭音量减小
            esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_DOWN, false);
        }
        else
        {
            // 等待连接
            vTaskDelay(pdMS_TO_TICKS(2000));
            ESP_LOGI("HID_DEMO", "Waiting for connection...");
        }
    }
}

void mouse_move_task(void *pvParameters)
{
    // 初始偏移量
    int8_t x = 5;
    int8_t y = 5;

    while (1)
    {
        // 仅在有连接的情况下发送
        if (sec_conn)
        {
            ESP_LOGI(HID_BLE_TAG, "Sending mouse move: x=%d, y=%d", x, y);
            esp_hidd_send_mouse_value(hid_conn_id, 0x00, x, y);
        }

        // 每3秒发送一次
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/**
 * @brief 模拟手柄按键任务函数
 *
 * 该任务负责定时发送HID特性值以模拟手柄按键按下。
 * 只有在安全连接状态下才会发送指令。
 *
 * @param pvParameters 任务参数（未使用）
 */
void gamepad_button_task(void *pvParameters)
{
    // 示例输入报告
    uint8_t feature_value[11] = {4, 128, 128, 128, 128, 255, 0, 0, 0, 1, 0};

    while (1)
    {
        if (sec_conn)
        {
            ESP_LOGI(HID_BLE_TAG, "Simulating gamepad button press");
            // 使用自定义函数发送输入报告
            // esp_hidd_send_custom_report(hid_conn_id, HID_RPT_ID_MOUSE_IN, HID_REPORT_TYPE_INPUT, feature_value, sizeof(feature_value));
        }

        // 每5秒发送一次
        vTaskDelay(pdMS_TO_TICKS(5000));
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
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    return ESP_OK;
}

void app_main(void)
{

    // ble_init();

    if (ESP_OK == ble_init())
    {
        ble_sec_config();
    }
    ESP_LOGI("Main", "BLE HID Init OK");
    // 创建调整音量任务
    xTaskCreate(&hid_demo_task, "hid_task", 2048, NULL, 5, NULL);
    // 创建鼠标移动任务
    // xTaskCreate(&mouse_move_task, "mouse_move_task", 2048, NULL, 5, NULL);
    // 模拟手柄任务
    // xTaskCreate(&gamepad_button_task, "gamepad_button_task", 4096, NULL, 5, NULL);

    // ESP_LOGI("SIZEUUID:","%d",sizeof(hidd_service_uuid));
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
