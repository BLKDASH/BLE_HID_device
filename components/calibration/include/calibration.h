#ifndef __CALIBRATION_H__
#define __CALIBRATION_H__

#define UINT32_ARRAY_LENGTH 8

esp_err_t store_adc_cali_data(uint8_t index, uint32_t value);
esp_err_t read_adc_cali_data(uint8_t index, uint32_t *out_value);

esp_err_t nvs_get_boot_count(uint64_t *out_count);
esp_err_t nvs_set_boot_count(uint64_t count);


#endif



