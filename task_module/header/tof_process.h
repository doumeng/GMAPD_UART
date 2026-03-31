/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-02-02 09:07:25
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-23 10:47:41
 * @FilePath: /GMAPD_RK3588/task_module/header/compute_distance.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <vector>
#include <cstring>
#include <memory>
#include <tuple>
#include <thread>


#include "log.h"
#include "task_reg.h"

namespace TofProcesser {
    // 直方图统计结果结构体
    // struct HistogramResult {
    //     int maxPixelValue;  // 出现频数最高的像素值
    //     int maxFrequency;   // 该像素值出现的次数
    //     float occupancyRatio; // 非零像元比例
    // };

    // HistogramResult ComputeHistogram(
    //     const cv::Mat& image, 
    //     int startValue, 
    //     int endValue);

    // int ComputeDelay(
    //     float TargetDistance, 
    //     int BinWidth,  
    //     int Gatecount);
        

    void thread_ComputeDistance();
}
