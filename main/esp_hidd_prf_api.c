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

// HID输入报文长度
#define HID_CC_IN_RPT_LEN 3

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
        ESP_LOGE(HID_LE_PRF_TAG, "HID device profile already initialized");
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
        ESP_LOGE(HID_LE_PRF_TAG, "HID device profile already initialized");
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

void esp_hidd_send_consumer_value(uint16_t conn_id, uint8_t key_cmd, bool key_pressed)
{
    // 定义buffer，初始化为0
    uint8_t buffer[HID_CC_IN_RPT_LEN] = {0};
    if (key_pressed)
    {
        // 如果按键按下，根据CMD构建报文，并返回至buffer中
        hid_consumer_build_report(buffer, key_cmd);
    }
    // 如果没有按下，那么buffer被重置为0
    ESP_LOGI("esp_hidd_send_consumer_value", "buffer[0] = %x, buffer[1] = %x, buffer[2] = %x, size = %d", buffer[0], buffer[1], buffer[2], sizeof(buffer));
    // BUFFER已填充完毕
    // 最后封包并发送
    hid_dev_send_report(hidd_le_env.gatt_if, conn_id,
                        HID_RPT_ID_CC_IN, HID_REPORT_TYPE_INPUT, HID_CC_IN_RPT_LEN, buffer);
    return;
}

void esp_hidd_send_keyboard_value(uint16_t conn_id, key_mask_t special_key_mask, uint8_t *keyboard_cmd, uint8_t num_key)
{
    if (num_key > HID_KEYBOARD_IN_RPT_LEN - 2)
    {
        ESP_LOGE(HID_LE_PRF_TAG, "%s(), the number key should not be more than %d", __func__, HID_KEYBOARD_IN_RPT_LEN);
        return;
    }

    uint8_t buffer[HID_KEYBOARD_IN_RPT_LEN] = {0};

    buffer[0] = special_key_mask;

    for (int i = 0; i < num_key; i++)
    {
        buffer[i + 2] = keyboard_cmd[i];
    }

    ESP_LOGD(HID_LE_PRF_TAG, "the key vaule = %d,%d,%d, %d, %d, %d,%d, %d", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
    hid_dev_send_report(hidd_le_env.gatt_if, conn_id,
                        HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT, HID_KEYBOARD_IN_RPT_LEN, buffer);
    return;
}

void esp_hidd_send_mouse_value(uint16_t conn_id, uint8_t mouse_button, int8_t mickeys_x, int8_t mickeys_y)
{
    uint8_t buffer[HID_MOUSE_IN_RPT_LEN];

    buffer[0] = mouse_button; // Buttons
    buffer[1] = mickeys_x;    // X
    buffer[2] = mickeys_y;    // Y
    buffer[3] = 0;            // Wheel
    buffer[4] = 0;            // AC Pan

    hid_dev_send_report(hidd_le_env.gatt_if, conn_id,
                        HID_RPT_ID_MOUSE_IN, HID_REPORT_TYPE_INPUT, HID_MOUSE_IN_RPT_LEN, buffer);
    return;
}

void esp_hidd_send_gamepad_report(uint16_t conn_id, uint8_t report_id, uint8_t report_type, uint8_t *data, uint8_t length)
{
    hid_dev_send_report(hidd_le_env.gatt_if, conn_id, report_id, report_type, length, data);
}