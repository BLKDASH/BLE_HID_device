#include <stdio.h>
#include <string.h>
#include "nvs_flash.h" // NVS闪存初始化相关
#include "nvs.h"       // NVS核心操作接口
#include "calibration.h"
#include "esp_log.h"

static const char *TAG = "calibration";

// 命名空间定义（分离存储，避免键冲突）
static const char *UINT32_NAMESPACE = "uint32_storage";   // 存储8个uint32_t
static const char *BOOT_COUNT_NAMESPACE = "boot_counter"; // 存储开机次数
static const char *CALIBRATION_NAMESPACE = "calibration"; // 存储摇杆校准数据

// 无需 init，蓝牙初始化后，就已经 init
esp_err_t store_adc_cali_data(uint8_t index, uint32_t value)
{
    if (index >= UINT32_ARRAY_LENGTH)
    {
        ESP_LOGE(TAG, "索引超出范围（0~7）: %hhu", index);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(UINT32_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "打开命名空间失败（%s）: %s", UINT32_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    // 根据 index 生成键名
    char key[10];
    snprintf(key, sizeof(key), "val%d", index);

    err = nvs_set_u32(handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "存储%s失败: %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // 提交更改
    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "提交%s失败: %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    ESP_LOGI(TAG, "已存储%s: %lu", key, value);
    nvs_close(handle);
    return ESP_OK;
}

// 读取单个uint32_t值（按索引）
esp_err_t read_adc_cali_data(uint8_t index, uint32_t *out_value)
{
    if (index >= UINT32_ARRAY_LENGTH || out_value == NULL)
    {
        ESP_LOGE(TAG, "索引超出范围或指针为空");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(UINT32_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "打开命名空间失败(%s): %s", UINT32_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    // 根据索引生成键名
    char key[10];
    snprintf(key, sizeof(key), "val%d", index);

    err = nvs_get_u32(handle, key, out_value);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "读取%s成功: %lu", key, *out_value);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "%s尚未初始化(首次运行?)", key);
    }
    else
    {
        ESP_LOGE(TAG, "读取%s失败: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

// 获取并递增开机次数
esp_err_t nvs_get_boot_count(uint64_t *out_count)
{
    if (out_count == NULL)
    {
        ESP_LOGE(TAG, "指针为空");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(BOOT_COUNT_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "打开命名空间失败（%s）: %s", BOOT_COUNT_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    // 读取当前次数（如果未初始化则从0开始）
    uint64_t count = 0;
    err = nvs_get_u64(handle, "boot_count", &count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "读取开机次数失败: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // 递增次数（首次运行会从1开始）
    *out_count = ++count;

    // 存储新次数
    err = nvs_set_u64(handle, "boot_count", count);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "存储开机次数失败: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // 提交更改
    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "提交开机次数失败: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // ESP_LOGI(TAG, "当前开机次数: %llu", count);
    nvs_close(handle);
    return ESP_OK;
}

// 手动设置开机次数（用于重置）
esp_err_t nvs_set_boot_count(uint64_t count)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(BOOT_COUNT_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "打开命名空间失败（%s）: %s", BOOT_COUNT_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u64(handle, "boot_count", count);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "手动设置开机次数失败: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "提交手动设置失败: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    ESP_LOGI(TAG, "已手动设置开机次数为: %llu", count);
    nvs_close(handle);
    return ESP_OK;
}

// 存储摇杆校准数据
// ID0：左摇杆
// ID1：右摇杆
esp_err_t store_joystick_calibration_data(uint8_t joystick_id, const joystick_calibration_data_t *cal_data)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CALIBRATION_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "打开命名空间失败(%s): %s", CALIBRATION_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    // 摇杆生成键名，joystick_0或1
    char base_key[10];
    snprintf(base_key, sizeof(base_key), "js_%d", joystick_id);

    // 存储中心点
    // 生成具体键名：joystick_0_center_x
    char center_x_key[30];
    char center_y_key[30];
    snprintf(center_x_key, sizeof(center_x_key), "%s_center_x", base_key);
    snprintf(center_y_key, sizeof(center_y_key), "%s_center_y", base_key);

    err = nvs_set_u32(handle, center_x_key, cal_data->center_x);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "存储%s失败: %s", center_x_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_set_u32(handle, center_y_key, cal_data->center_y);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "存储%s失败: %s", center_y_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // 存储最小值
    char min_x_key[30];
    char min_y_key[30];
    snprintf(min_x_key, sizeof(min_x_key), "%s_min_x", base_key);
    snprintf(min_y_key, sizeof(min_y_key), "%s_min_y", base_key);

    err = nvs_set_u32(handle, min_x_key, cal_data->min_x);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "存储%s失败: %s", min_x_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_set_u32(handle, min_y_key, cal_data->min_y);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "存储%s失败: %s", min_y_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // 存储最大值
    char max_x_key[30];
    char max_y_key[30];
    snprintf(max_x_key, sizeof(max_x_key), "%s_max_x", base_key);
    snprintf(max_y_key, sizeof(max_y_key), "%s_max_y", base_key);

    err = nvs_set_u32(handle, max_x_key, cal_data->max_x);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "存储%s失败: %s", max_x_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_set_u32(handle, max_y_key, cal_data->max_y);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "存储%s失败: %s", max_y_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // 提交更改
    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "提交摇杆校准数据失败: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    ESP_LOGI(TAG, "已存储摇杆%d的校准数据", joystick_id);
    nvs_close(handle);
    return ESP_OK;
}

// 读取摇杆校准数据
esp_err_t read_joystick_calibration_data(uint8_t joystick_id, joystick_calibration_data_t *out_data)
{
    if (joystick_id >= 2 || out_data == NULL)
    {
        ESP_LOGE(TAG, "无效的摇杆ID或空指针");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CALIBRATION_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "打开命名空间失败(%s): %s", CALIBRATION_NAMESPACE, esp_err_to_name(err));

        // 如果是首次运行，设置默认值
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            out_data->center_x = 600;
            out_data->center_y = 600;
            out_data->min_x = 0;
            out_data->min_y = 0;
            out_data->max_x = 1400;
            out_data->max_y = 1400;
            ESP_LOGW(TAG, "摇杆%d尚未校准，使用默认值", joystick_id);
        }
        return err;
    }

    // 生成键名
    char base_key[10];
    snprintf(base_key, sizeof(base_key), "js_%d", joystick_id);

    // 读取中心点
    char center_x_key[30];
    char center_y_key[30];
    snprintf(center_x_key, sizeof(center_x_key), "%s_center_x", base_key);
    snprintf(center_y_key, sizeof(center_y_key), "%s_center_y", base_key);

    err = nvs_get_u32(handle, center_x_key, &out_data->center_x);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "读取%s失败: %s", center_x_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_get_u32(handle, center_y_key, &out_data->center_y);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "读取%s失败: %s", center_y_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // 读取最小值
    char min_x_key[30];
    char min_y_key[30];
    snprintf(min_x_key, sizeof(min_x_key), "%s_min_x", base_key);
    snprintf(min_y_key, sizeof(min_y_key), "%s_min_y", base_key);

    err = nvs_get_u32(handle, min_x_key, &out_data->min_x);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "读取%s失败: %s", min_x_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_get_u32(handle, min_y_key, &out_data->min_y);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "读取%s失败: %s", min_y_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // 读取最大值
    char max_x_key[30];
    char max_y_key[30];
    snprintf(max_x_key, sizeof(max_x_key), "%s_max_x", base_key);
    snprintf(max_y_key, sizeof(max_y_key), "%s_max_y", base_key);

    err = nvs_get_u32(handle, max_x_key, &out_data->max_x);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "读取%s失败: %s", max_x_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_get_u32(handle, max_y_key, &out_data->max_y);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "读取%s失败: %s", max_y_key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    ESP_LOGI(TAG, "读取摇杆%d校准数据成功", joystick_id);

    nvs_close(handle);
    return ESP_OK;
}