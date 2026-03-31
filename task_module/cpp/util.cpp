/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-01-23 09:31:42
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-26 19:39:02
 * @FilePath: /K253154_Preprocess_RK3588_0123/task_module/cpp/util.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "util.h"
#include <cstring>

// 根据距离，计算需要的延迟时间
int ComputeDelay(float TargetDistance, int BinWidth, int Gatecount) {
    // int bincount = (TargetDistance * 2) / (0.3 * BinWidth); // 计算需要的bin数
    // int timeDelay = bincount - (bincount % Gatecount); // 计算实际延迟bin数
    // return timeDelay * 2;
    int delay = TargetDistance / 0.15f ;
    return delay;
}

// 创建128x128的uint16矩阵并初始化为0
uint16_t *createUint16Matrix(int rows, int cols)
{
    uint16_t *matrix = new uint16_t[rows * cols];
    memset(matrix, 0, rows * cols * sizeof(uint16_t));
    return matrix;
}

// 创建128x128的uint16矩阵并初始化为0
float *createFloatMatrix(int rows, int cols)
{
    float *matrix = new float[rows * cols];
    memset(matrix, 0, rows * cols * sizeof(float));
    return matrix;
}

uint16_t * float2Uint16(const float *floatArray, size_t size){
    // 创建新数组（每个元素32位）
    uint16_t *combined = new uint16_t[size];

    for (size_t i = 0; i < size; ++i)
    {
        // 浮点数处理：保留1位小数→乘以10→截断小数部分→转为16位整数
        uint16_t floatValue = static_cast<uint16_t>(trunc(floatArray[i] * 10.0f));
        combined[i] = floatValue;
    }
    return combined; 
}

uint32_t *interleaveArrays(const float *floatArray, const uint16_t *intArray, size_t size)
{
    // 创建新数组（每个元素32位）
    uint32_t *combined = new uint32_t[size];

    for (size_t i = 0; i < size; ++i)
    {
        // 浮点数处理：保留1位小数→乘以10→截断小数部分→转为16位整数
        uint16_t floatValue = static_cast<uint16_t>(trunc(floatArray[i] * 10.0f));

        // 拼接：高位为深度，低位为强度
        combined[i] = (static_cast<uint32_t>(floatValue) << 16) | intArray[i];
    }

    return combined; // 返回新数组首地址
}