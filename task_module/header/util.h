/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-01-27 09:00:23
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-20 20:44:01
 * @FilePath: /GMAPD_RK3588/task_module/header/util.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <cstdint>
#include <cmath>
#include "log.h"

int ComputeDelay(float TargetDistance, int BinWidth, int Gatecount);
uint16_t * createUint16Matrix(int rows, int cols);
float * createFloatMatrix(int rows, int cols);

uint32_t * interleaveArrays(const float* floatArray, const uint16_t* intArray, size_t size);
uint16_t * float2Uint16(const float *floatArray, size_t size);