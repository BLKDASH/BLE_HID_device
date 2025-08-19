#ifndef __PROCESSING_H__
#define __PROCESSING_H__

#include <stdint.h>

// 单个通道的环形缓冲区结构体
typedef struct {
    uint32_t *buffer;       // 存储数据的数组
    uint32_t size;       // 缓冲区容量
    uint32_t count;      // 当前有效数据数量
    uint32_t head;       // 指向最新数据的索引
    uint32_t tail;       // 指向最旧数据的索引
} ChannelBuffer;

// 8通道管理器
typedef struct {
    ChannelBuffer channels[8];  // 8个数据通道
    uint32_t buffer_size;       // 每个缓冲区的容量
} MultiChannelBuffer;


MultiChannelBuffer* mcb_init(uint32_t buffer_size);

void mcb_push(MultiChannelBuffer *mcb, uint8_t channel, uint32_t data);

uint32_t mcb_get_average(MultiChannelBuffer *mcb, uint8_t channel);

void mcb_get_all_averages(MultiChannelBuffer *mcb, uint32_t averages[8]);

uint32_t mcb_get_count(MultiChannelBuffer *mcb, uint8_t channel);

void mcb_clear_channel(MultiChannelBuffer *mcb, uint8_t channel);

void mcb_clear_all(MultiChannelBuffer *mcb);

void mcb_destroy(MultiChannelBuffer *mcb);


#endif