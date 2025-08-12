// API文件
#include "esp_hidd_prf_api.h"
#include "hidd_le_prf_int.h"
#include "hid_dev.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "main.h"

// HID keyboard input report length
#define HID_KEYBOARD_IN_RPT_LEN 8

// HID LED output report length
#define HID_LED_OUT_RPT_LEN 1

// HID mouse input report length
#define HID_MOUSE_IN_RPT_LEN 5

// HID cc 报文长度
#define HID_CC_IN_RPT_LEN 3

// 全局游戏手柄报告缓冲区定义
volatile uint8_t gamepad_report_buffer[HID_GAMEPAD_STICK_IN_RPT_LEN] = {128,128,128,128,255,0,};



/**
 * @brief 注册HID设备的回调函数并初始化GATT服务应用。
 *
 * 此函数用于注册HID设备所需的回调函数，并初始化BLE GATT服务应用。如果回调函数为NULL，
 * 则返回错误。函数还会检查注册回调是否成功，并注册两个GATT应用（BATTRAY_APP_ID和HIDD_APP_ID）。
 *
 * @param callbacks 指向esp_hidd_event_cb_t类型的指针，包含HID设备事件的回调函数。
 *
 * @return esp_err_t 返回操作结果的状态码。
 *         - ESP_OK: 操作成功。
 *         - ESP_FAIL: 提供的回调函数为NULL。
 *         - 其他错误码: 由hidd_register_cb和esp_ble_gatts_app_register返回。
 */
esp_err_t esp_hidd_register_callbacks(esp_hidd_event_cb_t callbacks)
{
    esp_err_t hidd_status;

    // 检查并注册回调函数
    if (callbacks != NULL)
    {
        hidd_le_env.hidd_cb = callbacks;
    }
    else
    {
        return ESP_FAIL;
    }

    // 调用hidd_register_cb注册回调，并检查返回状态
    if ((hidd_status = hidd_register_cb()) != ESP_OK)
    {
        return hidd_status;
    }

    // 注册BATTRAY_APP_ID对应的GATT服务应用
    esp_ble_gatts_app_register(BATTRAY_APP_ID);

    // 注册HIDD_APP_ID对应的GATT服务应用，并检查返回状态
    if ((hidd_status = esp_ble_gatts_app_register(HIDD_APP_ID)) != ESP_OK)
    {
        return hidd_status;
    }

    // 一共注册了两个应用，一个是电池，一个是HIDD
    return hidd_status;
}

/**
 * @brief 初始化HID设备配置文件。
 *
 * 此函数用于初始化HID设备配置文件。如果配置文件已经初始化，
 * 则记录错误并返回失败状态。
 *
 * @return
 *  - ESP_OK: 初始化成功。
 *  - ESP_FAIL: 配置文件已经初始化。
 */
esp_err_t esp_hidd_profile_init(void)
{
    // 检查HID设备配置文件是否已初始化
    if (hidd_le_env.enabled)
    {
        // ESP_LOGE(HID_LE_PRF_TAG, "HID device profile already initialized");
        return ESP_FAIL;
    }

    // 重置HID设备目标环境
    memset(&hidd_le_env, 0, sizeof(hidd_le_env_t));
    hidd_le_env.enabled = true;
    return ESP_OK;
}

esp_err_t esp_hidd_profile_deinit(void)
{
    uint16_t hidd_svc_hdl = hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_SVC];
    if (!hidd_le_env.enabled)
    {
        // ESP_LOGE(HID_LE_PRF_TAG, "HID device profile already initialized");
        return ESP_OK;
    }

    if (hidd_svc_hdl != 0)
    {
        esp_ble_gatts_stop_service(hidd_svc_hdl);
        esp_ble_gatts_delete_service(hidd_svc_hdl);
    }
    else
    {
        return ESP_FAIL;
    }

    /* register the HID device profile to the BTA_GATTS module*/
    esp_ble_gatts_app_unregister(hidd_le_env.gatt_if);

    return ESP_OK;
}

uint16_t esp_hidd_get_version(void)
{
    return HIDD_VERSION;
}










void esp_hidd_send_gamepad_report(uint16_t conn_id)
{



    hid_dev_send_report(hidd_le_env.gatt_if, conn_id,
                        HID_RPT_ID_GAMEPAD_STICK_IN, HID_REPORT_TYPE_INPUT, HID_GAMEPAD_STICK_IN_RPT_LEN, (uint8_t*)gamepad_report_buffer);

    return;
}