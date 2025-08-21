#include "processing.h"
#include <stdlib.h>
#include <string.h>
#include "hardware_init.h"
#include "main.h"
#include "esp_log.h"


extern MultiChannelBuffer *mcb;

// static const char *TAG = "processing";

// 初始化多通道缓冲区
MultiChannelBuffer* mcb_init(uint32_t buffer_size) {
    if (buffer_size == 0) return NULL;
    
    MultiChannelBuffer *mcb = (MultiChannelBuffer*)malloc(sizeof(MultiChannelBuffer));
    if (!mcb) return NULL;
    
    mcb->buffer_size = buffer_size;
    memset(mcb->channels, 0, sizeof(mcb->channels));
    
    // 初始化8个通道
    for (uint8_t i = 0; i < 8; i++) {
        mcb->channels[i].buffer = (uint32_t*)malloc(sizeof(uint32_t) * buffer_size);
        if (!mcb->channels[i].buffer) {
            // 内存分配失败时，释放已分配的资源
            for (uint8_t j = 0; j < i; j++) {
                free(mcb->channels[j].buffer);
            }
            free(mcb);
            return NULL;
        }
        
        mcb->channels[i].size = buffer_size;
        mcb->channels[i].count = 0;
        mcb->channels[i].head = 0;
        mcb->channels[i].tail = 0;
    }
    
    return mcb;
}

// 向指定通道插入数据（channel: 0-7）
void mcb_push(MultiChannelBuffer *mcb, uint8_t channel, uint32_t data) {
    // 检查参数有效性
    if (!mcb || channel >= 8) return;
    
    ChannelBuffer *cb = &mcb->channels[channel];
    
    // 存储数据
    cb->buffer[cb->head] = data;
    
    // 更新head指针
    cb->head = (cb->head + 1) % cb->size;
    
    // 处理缓冲区满的情况
    if (cb->count < cb->size) {
        cb->count++;
    } else {
        // 缓冲区已满，移动tail指针
        cb->tail = (cb->tail + 1) % cb->size;
    }
}

// 获取指定通道的平均值（channel: 0-7）
uint32_t mcb_get_average(MultiChannelBuffer *mcb, uint8_t channel) {
    // 检查参数有效性
    if (!mcb || channel >= 8 || mcb->channels[channel].count == 0) {
        return 0;
    }
    
    ChannelBuffer *cb = &mcb->channels[channel];
    uint32_t sum = 0;
    
    // 计算该通道所有有效数据的总和
    for (uint32_t i = 0; i < cb->count; i++) {
        uint32_t index = (cb->tail + i) % cb->size;
        sum += cb->buffer[index];
    }
    
    return sum / cb->count;
}

// 获取所有通道的平均值数组
void mcb_get_all_averages(MultiChannelBuffer *mcb, uint32_t averages[8]) {
    if (!mcb || !averages) return;
    int voltage_mv = 0;
    
    for (uint8_t i = 0; i < 8; i++) {
        adc_cali_raw_to_voltage(adc1_cali_handle, mcb_get_average(mcb, i), &voltage_mv);
        averages[i] = voltage_mv;
    }
}

// 获取指定通道的当前数据数量
uint32_t mcb_get_count(MultiChannelBuffer *mcb, uint8_t channel) {
    if (!mcb || channel >= 8) return 0;
    
    return mcb->channels[channel].count;
}

// 清空指定通道的数据
void mcb_clear_channel(MultiChannelBuffer *mcb, uint8_t channel) {
    if (!mcb || channel >= 8) return;
    
    ChannelBuffer *cb = &mcb->channels[channel];
    cb->count = 0;
    cb->head = 0;
    cb->tail = 0;
}

// 清空所有通道的数据
void mcb_clear_all(MultiChannelBuffer *mcb) {
    if (!mcb) return;
    
    for (uint8_t i = 0; i < 8; i++) {
        mcb->channels[i].count = 0;
        mcb->channels[i].head = 0;
        mcb->channels[i].tail = 0;
    }
}

// 销毁多通道缓冲区，释放内存
void mcb_destroy(MultiChannelBuffer *mcb) {
    if (mcb) {
        for (uint8_t i = 0; i < 8; i++) {
            free(mcb->channels[i].buffer);
            mcb->channels[i].buffer = NULL;
        }
        free(mcb);
    }
}

