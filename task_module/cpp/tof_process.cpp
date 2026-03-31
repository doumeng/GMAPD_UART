/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-01-27 09:00:23
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-23 16:23:37
 * @FilePath: /GMAPD_RK3588/task_module/cpp/compute_distance.cpp
 * @Description: tof 处理线程
 */

#include "tof_process.h"

namespace TofProcesser {
    // // 计算直方图并返回统计结果 (支持14位图像)
    // HistogramResult ComputeHistogram(const cv::Mat& image, int startValue, int endValue) {
    //     // 检查是否为空图像
    //     if (image.empty()) {
    //         return {-1, -1, 0.0f};
    //     }

    //     // 设置直方图参数（只统计1000-50000范围内的像素值）
    //     const int histSize = static_cast<int>((endValue - startValue + 1) / 1000);
    //     const float range[] = {startValue, endValue+1};   // 上限值不包含
    //     const float* histRange = {range};

    //     // 计算直方图
    //     cv::Mat hist;
    //     cv::calcHist(&image, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange, true);

    //     // 使用内置函数找到最大值及其位置
    //     cv::Point maxLoc;
    //     double maxValue;
    //     cv::minMaxLoc(hist, nullptr, &maxValue, nullptr, &maxLoc);

    //     // 计算非零像素占比，作为场景稀疏度指标
    //     double nonZeroPixels = cv::countNonZero(image);
    //     double totalPixels = static_cast<double>(image.total());
    //     float occupancyRatio = (totalPixels > 0) ? static_cast<float>(nonZeroPixels / totalPixels) : 0.0f;

    //     // maxLoc.y 对应的是 bin 索引，实际像素值为 bin + startValue
    //     return {maxLoc.y + startValue, static_cast<int>(maxValue), occupancyRatio};
    // }

    // 根据距离，计算需要的延迟时间
    // int ComputeDelay(float TargetDistance, int BinWidth, int Gatecount) {
    //     int bincount = (TargetDistance * 2) / (0.3 * BinWidth); // 计算需要的bin数
    //     int timeDelay = bincount - (bincount % Gatecount); // 计算实际延迟bin数
    //     return timeDelay * 2;
    // }


    void thread_ComputeDistance()
    {
        constexpr auto kComputeInterval = std::chrono::milliseconds(20); // 模拟1秒读取一次

        constexpr size_t kMaxTofFrameCount = 200;
        u_char *src = new u_char[kMaxTofFrameCount * 16384 * 2];

        while (1)
        {
            auto computationStart = std::chrono::steady_clock::now();

#if 0
            // TODO:MIPI接口读取数据
            if (g_sysConfig.workMode == UartComm::WorkMode::TEST)
            {
                img::ImgMod depthImg = img::imgRead(depthImgChnAttr);

                if(depthImg.isEmptyFrame()){
                    Logger::instance().debug("Thread tof process - Failed to read point cloud from mipi");
                    continue;
                }
                else
                {
                    Logger::instance().debug("Thread ComputeDistance - Successfully read raw data from MIPI");
                    
                    {          
                        UdpDataPacket pkt;
                        size_t dataLen = 2 * 2 * 16384 * sizeof(unsigned char);
                        pkt.data.resize(dataLen);
                        pkt.type = UdpPacketType::RAW_BYTES;

                        memcpy(pkt.data.data(), src + 2 * 16384 * sizeof(unsigned char), dataLen);

                        {
                            std::lock_guard<std::mutex> lock(g_udpMutex);
                            g_udpRing.push(std::move(pkt));
                        }
                        g_udpCV.notify_one();
                    }
                }
            }
            else if (g_sysConfig.workMode == UartComm::WorkMode::STANDARD)
            {
                bool hasNewData = false;
                {
                    std::unique_lock<std::shared_mutex> lock(g_sharedMat.matMutex);
                    if (g_sharedMat.newDataAvailable)
                    {
                        memcpy(memBuffer.data(), g_sharedMat.sharedMat, memBuffer.size() * sizeof(uint16_t));
                        g_sharedMat.newDataAvailable = false;
                        hasNewData = true;
                    }
                }

                if (!hasNewData)
                {
                    continue; 
                }
            
                cv::Mat MatToProcess(128, 128, CV_16UC1, memBuffer.data());

                if (!MatToProcess.empty())
                {
                    // 计算直方图并获取结果
                    HistogramResult result = ComputeHistogram(MatToProcess, 1000, 50000);
                    float computedistance = result.maxPixelValue / 10.0f;

                    {
                        g_sharedData.distance = computedistance;
                        g_sharedData.occupancyRatio = result.occupancyRatio;
                        g_sharedData.dataUpdated = true;
                    }

                    Logger::instance().info(("Thread ComputeDistance - Distance: " + std::to_string(computedistance) + " m, occupancy: " + std::to_string(result.occupancyRatio)).c_str());
                }
            }

#endif

#if 1
            if (g_sysConfig.workMode == UartComm::WorkMode::TEST){
                // 将src中的值修改为0-8000范围内的随机数（uint_16），模拟tof
                for (size_t i = 0; i < kMaxTofFrameCount * 16384; ++i) {
                    uint16_t randomValue = static_cast<uint16_t>(rand() % 8001); // 生成0-8000范围内的随机数
                    src[i * 2] = randomValue & 0xFF; // 低8位
                    src[i * 2 + 1] = (randomValue >> 8) & 0xFF; // 高8位
                }
                
                {          
                    UdpDataPacket pkt;
                    size_t dataLen = 2 * 2 * 16384 * sizeof(unsigned char);
                    pkt.data.resize(dataLen);
                    pkt.type = UdpPacketType::RAW_BYTES;

                    memcpy(pkt.data.data(), src + 2 * 16384 * sizeof(unsigned char), dataLen);

                    {
                        std::lock_guard<std::mutex> lock(g_udpMutex);
                        g_udpRing.push(std::move(pkt));
                    }
                    g_udpCV.notify_one();
                }
                Logger::instance().debug("Thread tof process - send tof test data to udp");
            }
#endif

            auto computationTime = std::chrono::steady_clock::now() - computationStart;
            long long duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(computationTime).count();
            Logger::instance().debug(("Thread ComputeDistance - Computation time: " + std::to_string(duration_ms) + "ms").c_str());

            if (computationTime < kComputeInterval)
            {
                std::this_thread::sleep_for(kComputeInterval - computationTime);
            }
        }

        delete[] src;
    }
}
