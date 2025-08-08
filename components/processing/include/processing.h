#ifndef __PROCESSING_H__
#define __PROCESSING_H__

#include <stdint.h>

// 单个通道的环形缓冲区结构体（内部使用，外部不需要直接操作）
typedef struct {
    uint32_t *buffer;       // 存储数据的数组
    uint32_t size;       // 缓冲区容量
    uint32_t count;      // 当前有效数据数量
    uint32_t head;       // 指向最新数据的索引
    uint32_t tail;       // 指向最旧数据的索引
} ChannelBuffer;

// 多通道管理器（8个通道）
typedef struct {
    ChannelBuffer channels[8];  // 8个数据通道
    uint32_t buffer_size;       // 每个缓冲区的容量
} MultiChannelBuffer;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化多通道缓冲区
 * @param buffer_size 每个通道的缓冲区容量
 * @return 成功返回缓冲区指针，失败返回NULL
 */
MultiChannelBuffer* mcb_init(uint32_t buffer_size);

/**
 * @brief 向指定通道插入数据
 * @param mcb 多通道缓冲区指针
 * @param channel 通道号（0-7）
 * @param data 要插入的数据
 */
void mcb_push(MultiChannelBuffer *mcb, uint8_t channel, uint32_t data);

/**
 * @brief 获取指定通道的平均值
 * @param mcb 多通道缓冲区指针
 * @param channel 通道号（0-7）
 * @return 通道数据的平均值，无效情况返回0.0f
 */
uint32_t mcb_get_average(MultiChannelBuffer *mcb, uint8_t channel);

/**
 * @brief 获取所有通道的平均值
 * @param mcb 多通道缓冲区指针
 * @param averages 存储平均值的数组（至少8个元素）
 */
void mcb_get_all_averages(MultiChannelBuffer *mcb, uint32_t averages[8]);

/**
 * @brief 获取指定通道的当前数据数量
 * @param mcb 多通道缓冲区指针
 * @param channel 通道号（0-7）
 * @return 数据数量，无效情况返回0
 */
uint32_t mcb_get_count(MultiChannelBuffer *mcb, uint8_t channel);

/**
 * @brief 清空指定通道的数据
 * @param mcb 多通道缓冲区指针
 * @param channel 通道号（0-7）
 */
void mcb_clear_channel(MultiChannelBuffer *mcb, uint8_t channel);

/**
 * @brief 清空所有通道的数据
 * @param mcb 多通道缓冲区指针
 */
void mcb_clear_all(MultiChannelBuffer *mcb);

/**
 * @brief 销毁多通道缓冲区，释放内存
 * @param mcb 多通道缓冲区指针
 */
void mcb_destroy(MultiChannelBuffer *mcb);


#endif