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


namespace PointCloud {
    // Helper to convert float vector to uint16 vector
    std::vector<uint16_t> Float2Uint16(const std::vector<float>& floatArray) {
        std::vector<uint16_t> result(floatArray.size());
        for (size_t i = 0; i < floatArray.size(); ++i) {
            result[i] = static_cast<uint16_t>(floatArray[i]);
        }
        return result;
    }

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
                        
                        size_t pixel_count = 128 * 128;

                        // 给udpPkt填充数据，距离和强度填0，原始数据填充PCIe读取的深度图数据
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
            // Logger::instance().info(("Thread PointCloudProcess - Computation time: " + std::to_string(duration_ms) + "ms").c_str());

            if (computationTime < kComputeInterval)
            {
                std::this_thread::sleep_for(kComputeInterval - computationTime);
            }
        }
    }
}