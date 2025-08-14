#ifndef __CALIBRATION_H__
#define __CALIBRATION_H__

#define UINT32_ARRAY_LENGTH 8

// 单个摇杆的校准数据结构体
typedef struct {
    uint32_t center_x;  // 中心点X值
    uint32_t center_y;  // 中心点Y值
    uint32_t min_x;     // 最小X值
    uint32_t max_x;     // 最大X值
    uint32_t min_y;     // 最小Y值
    uint32_t max_y;     // 最大Y值
    uint32_t trigger;   // 扳机值
} joystick_calibration_data_t;


esp_err_t store_adc_cali_data(uint8_t index, uint32_t value);
esp_err_t read_adc_cali_data(uint8_t index, uint32_t *out_value);

esp_err_t nvs_get_boot_count(uint64_t *out_count);
esp_err_t nvs_set_boot_count(uint64_t count);

esp_err_t store_joystick_calibration_data(uint8_t joystick_id, const joystick_calibration_data_t* cal_data);
esp_err_t read_joystick_calibration_data(uint8_t joystick_id, joystick_calibration_data_t* out_data);


#endif



