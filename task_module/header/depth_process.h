/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-02-02 09:07:25
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-20 21:25:57
 * @FilePath: /GMAPD_RK3588/task_module/header/point_cloud_process.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <tuple>
#include <cstdint>

#include <opencv2/opencv.hpp>

#include "util.h"     // 包含二维数组分配和释放的函数声明
#include "log.h"      // 日志记录功能
#include "task_reg.h" // 包含共享数据结构和全局变量声明

namespace PointCloud {

    // 1. 解析原始数据中的强度和距离，并将飞行时间转换为距离
    void parseDepthData(const uint32_t* rawData, int rows, int cols, int16_t offset,
                        cv::Mat& intenMat, cv::Mat& distMat);

    // 2. 根据强度阈值对强度图和距离图进行降噪（低于阈值的像素清零）
    void denoiseByIntensity(cv::Mat& intenMat, cv::Mat& distMat, uint8_t threshold);

    // 2.2 使用 Open3D DBSCAN 对深度和强度进行聚类降噪
    // eps/minSamples < 0 时，自动回退为全局配置 g_histConfig 中对应参数
    void denoiseByDbscanOpen3D(const cv::Mat& intenIn, const cv::Mat& distIn,
                               cv::Mat& intenOut, cv::Mat& distOut,
                               float eps = -1.0f, int minSamples = -1);

    // 3. 对强度图和距离图执行形态学开运算（腐蚀+膨胀），去除孤立散点
    void morphologicalFilter(const cv::Mat& intenIn, const cv::Mat& distIn,
                             cv::Mat& intenOut, cv::Mat& distOut, uint8_t kSize);

    // 4. 将处理后的强度图和距离图打包输出到 UdpDataPacket
    void packDepthOutput(const cv::Mat& intenMat, const cv::Mat& distMat,
                         int rows, int cols, UdpDataPacket& udpPkt);

    // 完整处理流水线：解析并进行降噪
    void processDenoisedDepthImage(const uint32_t* rawData, int rows, int cols, int16_t offset, UdpDataPacket& udpPkt);

    void thread_PointCloudProcess();

}
