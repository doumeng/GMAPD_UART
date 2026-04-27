/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-01-27 09:00:23
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-23 16:23:37
 * @FilePath: /GMAPD_RK3588/task_module/cpp/compute_distance.cpp
 * @Description: tof 处理线程
 */

#include "tof_process.h"
#include "apd_control.h"
#include "img.h"

namespace TofProcesser {

    float calculateDistanceFromTof(const uint16_t* tofData, size_t numPixels, int startValue, int endValue, int binWidth) {
        if(!tofData || numPixels == 0) {
            return 0.0f;
        }

        if (binWidth <= 0) binWidth = 1;
        int numBins = (endValue - startValue) / binWidth + 1;
        std::vector<int> hist(numBins, 0);

        int maxFreq = 0;
        int bestBinIdx = -1;

        for (size_t i = 0; i < numPixels; ++i) {
            uint16_t pixel = tofData[i];

            if (pixel >= startValue && pixel <= endValue) {
                int binIdx = (pixel - startValue) / binWidth;
                if (binIdx >= 0 && binIdx < numBins) {
                    hist[binIdx]++;
                    if (hist[binIdx] > maxFreq) {
                        maxFreq = hist[binIdx];
                        bestBinIdx = binIdx;
                    }
                }
            }
        }
        
        float targetDistance = 0.0f;

        if (bestBinIdx != -1) {
            int bestBinValue = 16000 - (startValue + bestBinIdx * binWidth) * 2;
            targetDistance = (bestBinValue + g_sysConfig.enDelay) * 0.15f;
        }

        return targetDistance;
    }

    void thread_TofProcess()
    {
        constexpr auto kComputeInterval = std::chrono::milliseconds(20);

        while (1)
        {
            auto start_time = std::chrono::steady_clock::now();

            size_t numPixels = 16384 * 20; // 20帧，每帧16384像素 
            std::vector<uint16_t> tofDataCopy(numPixels);
            
            {
                img::ImgMod tofImg = img::imgRead(tofImgChnAttr);

                if(tofImg.isEmptyFrame()) {
                    continue;
                }
                std::memcpy(tofDataCopy.data(), tofImg.ptr(), numPixels * sizeof(uint16_t));
            }

            if (g_sysConfig.workMode == UartComm::WorkMode::TEST)
            {   
                {
                    UdpDataPacket pkt;
                    size_t dataLen = 2 * 128 * 128 * sizeof(uint16_t);
                    pkt.data.resize(dataLen);
                    pkt.type = UdpPacketType::TOF_IMAGE;

                    memcpy(pkt.data.data(), reinterpret_cast<unsigned char*>(tofDataCopy.data()), dataLen);

                    {
                        std::lock_guard<std::mutex> lock(g_udpMutex);
                        g_udpRing.push(std::move(pkt));
                    }

                    g_udpCV.notify_one();
                }
            }
    
            else if (g_sysConfig.workMode == UartComm::WorkMode::STANDARD)
            {
                float targetDistance = calculateDistanceFromTof(tofDataCopy.data(), numPixels);
                {
                    g_histConfig.distance = targetDistance;
                    g_histConfig.maxDistance = targetDistance + 250;
                    g_histConfig.minDistance = (targetDistance < 250) ? 0 : (targetDistance - 250);
                }
                Logger::instance().debug(("Thread TofProcess - Mode: STANDARD, Target Distance: " + std::to_string(targetDistance) + "m").c_str());
            }

            auto computationTime = std::chrono::steady_clock::now() - start_time;
            long long duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(computationTime).count();
            Logger::instance().debug(("Thread TofProcess - Computation time: " + std::to_string(duration_ms) + "ms").c_str());

            if (computationTime < kComputeInterval)
            {
                std::this_thread::sleep_for(kComputeInterval - computationTime);
            }
        }
    }
}
