/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-20 21:19:44
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-24 11:06:36
 * @FilePath: /GMAPD_UART/task_module/cpp/point_cloud_process.cpp
 * @Description: 处理深度图和强度图
 */
#include "depth_process.h"

#include "log.h"
#include "apd_control.h"
#include "util.h"

#include <cstring>
#include <algorithm>
#include <chrono>
#include <mutex>
// #include <opencv2/opencv.hpp>


namespace PointCloud {
    // Helper to convert float vector to uint16 vector
    std::vector<uint16_t> Float2Uint16(const std::vector<float>& floatArray) {
        std::vector<uint16_t> result(floatArray.size());
        for (size_t i = 0; i < floatArray.size(); ++i) {
            result[i] = static_cast<uint16_t>(floatArray[i]);
        }
        return result;
    }

#if 0
    void processDepthImage(const uint32_t* rawData, int rows, int cols, UdpDataPacket& udpPkt) {
        size_t pixel_count = rows * cols;
        
        cv::Mat intenMat(rows, cols, CV_16UC1);
        cv::Mat distMat(rows, cols, CV_16UC1);

        const uint16_t* raw_ptr = reinterpret_cast<const uint16_t*>(rawData);

        // 1. 解析出强度及距离数据，先是强度，然后是距离
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                size_t idx = r * cols + c;
                intenMat.at<uint16_t>(r, c) = raw_ptr[2 * idx];       // 强度在前
                distMat.at<uint16_t>(r, c)  = raw_ptr[2 * idx + 1];   // 距离在后
            }
        }

        // 2. 根据强度去噪（阈值通过通信程序获取 g_histConfig.denoiseLevel）
        uint8_t currDenoiseLevel = g_histConfig.denoiseLevel;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (intenMat.at<uint16_t>(r, c) < currDenoiseLevel) {
                    intenMat.at<uint16_t>(r, c) = 0;
                    distMat.at<uint16_t>(r, c) = 0;
                }
            }
        }

        // 3. 使用腐蚀膨胀操作(形态学开运算)去除散点
        uint8_t kSize = g_histConfig.kernalSize;
        if (kSize == 0) kSize = 1; // 防止核大小为0
        
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kSize, kSize));
        cv::Mat intenFiltered, distFiltered;
        // MORPH_OPEN 先腐蚀后膨胀，去除背景上的孤立散点
        cv::morphologyEx(intenMat, intenFiltered, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(distMat, distFiltered, cv::MORPH_OPEN, kernel);

        // 4. 将处理后的图像输出
        udpPkt.dist.resize(pixel_count, 0.0f);
        udpPkt.inten.resize(pixel_count, 0);
        udpPkt.raw.resize(pixel_count, 0);

        uint16_t* out_raw_ptr = reinterpret_cast<uint16_t*>(udpPkt.raw.data());

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                size_t index = r * cols + c;
                out_raw_ptr[2 * index]     = intenFiltered.at<uint16_t>(r, c);
                out_raw_ptr[2 * index + 1] = distFiltered.at<uint16_t>(r, c);
                
                udpPkt.inten[index] = intenFiltered.at<uint16_t>(r, c);
                udpPkt.dist[index]  = static_cast<float>(distFiltered.at<uint16_t>(r, c));
            }
        }
    }
# endif

    void thread_PointCloudProcess()
    {
        constexpr auto kComputeInterval = std::chrono::milliseconds(20);

        while (1)
        {

            auto start_time = std::chrono::high_resolution_clock::now();

#if 1   
            if (g_sysConfig.workMode == UartComm::WorkMode::STANDARD)
            {

                img::ImgMod depthImg = img::imgRead(depthImgChnAttr);

                if(depthImg.isEmptyFrame()){
                    Logger::instance().debug("Thread PointCloudProcess - Failed to read point cloud from PCIe");
                    continue;
                }
                else{
                    Logger::instance().debug("Thread PointCloudProcess - Successfully read point cloud from PCIe");
                    // TODO: 添加处理点云的代码
                    
                    // 发送 UDP 数据 (放入队列)
                    {
                        UdpDataPacket udpPkt;
                        udpPkt.type = UdpPacketType::POINT_CLOUD_PROCESS;
                        udpPkt.rows = 128;
                        udpPkt.cols = 128;
                        
                        // 调用 OpenCV 接口处理图像，并填充距离和强度
                        // uint32_t* rawData = reinterpret_cast<uint32_t*>(depthImg.ptr());
                        // processDepthImage(rawData, udpPkt.rows, udpPkt.cols, udpPkt);

                        // 给udpPkt填充数据，距离和强度填0，原始数据填充PCIe读取的深度图数据
                        size_t pixel_count = 128 * 128;

                        udpPkt.dist.resize(pixel_count, 0.0f);
                        udpPkt.inten.resize(pixel_count, 0);
                        udpPkt.raw.resize(pixel_count, 0);

                        // 将depthImg.ptr()的数据复制到udpPkt.raw中
                        std::memcpy(udpPkt.raw.data(), depthImg.ptr(), pixel_count * sizeof(uint32_t));
                        
                        {
                            std::lock_guard<std::mutex> lock(g_udpMutex);
                            g_udpRing.push(std::move(udpPkt));

                        }

                        g_udpCV.notify_one();
                        
                    }

                }
#endif

#if 0
            // 发送 UDP 数据 (放入队列)
            {
                UdpDataPacket udpPkt;
                udpPkt.type = UdpPacketType::POINT_CLOUD_PROCESS;
                udpPkt.rows = 128;
                udpPkt.cols = 128;
                
                size_t pixel_count = 128 * 128;

                udpPkt.dist.resize(pixel_count, 100.0f);
                udpPkt.inten.resize(pixel_count, 255);
                udpPkt.raw.resize(pixel_count, 255);

                {
                    std::lock_guard<std::mutex> lock(g_udpMutex);
                    g_udpRing.push(std::move(udpPkt));
                }

                g_udpCV.notify_one();
            }
#endif
            auto computationTime = std::chrono::high_resolution_clock::now() - start_time;
            long long duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(computationTime).count();
            Logger::instance().info(("Thread PointCloudProcess - Computation time: " + std::to_string(duration_ms) + "ms").c_str());

            if (computationTime < kComputeInterval)
            {
                std::this_thread::sleep_for(kComputeInterval - computationTime);
            }
        }
    }
}
}