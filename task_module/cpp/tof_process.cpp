/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-01-27 09:00:23
 * @LastEditors: Do not edit
 * @LastEditTime: 2026-05-18 16:43:23
 * @FilePath: \GMAPD_UART\task_module\cpp\tof_process.cpp
 * @Description: tof 处理线程
 */

#include "tof_process.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>

#include "apd_control.h"
#include "img.h"
#include "preprocess_uart_slave.h"
#include "task_reg.h"

namespace TofProcesser {

    namespace {

        constexpr float kDelayUpdateDistanceThreshold = 100.0f;
        constexpr auto kComputeInterval = std::chrono::milliseconds(500);
        constexpr auto kFrameWaitTimeout = std::chrono::milliseconds(200);

    } // namespace

    bool calculateMedianDistance(const float *distanceData,
                                 std::size_t distanceCount,
                                 int rows,
                                 int cols,
                                 std::size_t roiSize,
                                 float &targetDistance) {
        targetDistance = 0.0f;
        if (distanceData == nullptr || rows <= 0 || cols <= 0 || roiSize == 0) {
            return false;
        }

        const std::size_t rowCount = static_cast<std::size_t>(rows);
        const std::size_t colCount = static_cast<std::size_t>(cols);

        if (rowCount > distanceCount / colCount || roiSize > rowCount || roiSize > colCount) {
            return false;
        }

        const std::size_t startRow = (rowCount - roiSize) / 2;
        const std::size_t startCol = (colCount - roiSize) / 2;

        if (rowCount > DistanceFrameShared::kPixelCount / colCount) {
            return false;
        }

        std::array<float, DistanceFrameShared::kPixelCount> validDistances{};
        std::size_t validCount = 0;

        for (std::size_t row = startRow; row < startRow + roiSize; ++row) {
            const std::size_t rowOffset = row * colCount;
            for (std::size_t col = startCol; col < startCol + roiSize; ++col) {
                const float distance = distanceData[rowOffset + col];
                if (std::isfinite(distance) && distance > 0.0f) {
                    validDistances[validCount++] = distance;
                }
            }
        }

        if (validCount == 0) {
            return false;
        }

        const std::size_t middle = validCount / 2;
        std::nth_element(validDistances.begin(), validDistances.begin() + static_cast<std::ptrdiff_t>(middle),
                         validDistances.begin() + static_cast<std::ptrdiff_t>(validCount));
        const float upperMiddle = validDistances[middle];

        if ((validCount % 2) != 0) {
            targetDistance = upperMiddle;
            return true;
        }

        const float lowerMiddle = *std::max_element(validDistances.begin(), validDistances.begin() + static_cast<std::ptrdiff_t>(middle));
        targetDistance = static_cast<float>((static_cast<double>(lowerMiddle) + static_cast<double>(upperMiddle)) * 0.5);

        return true;
    }

    void thread_TofProcess() {
        uint64_t lastFrameSequence = 0;
        uint64_t trackingGeneration = g_trackingGeneration.load();
        float lastAppliedDistance = 0.0f;
        bool hasAppliedTrackingDistance = false;

        while (1) {

            if (g_sysConfig.workMode == UartComm::WorkMode::TEST)
            {
                // 测试模式发送原始数据
                constexpr std::size_t kFramePixelCount = 128 * 128;
                constexpr std::size_t kFrameBytes = kFramePixelCount * sizeof(uint16_t);
                hasAppliedTrackingDistance = false;
                trackingGeneration = g_trackingGeneration.load();

                img::ImgMod tofImg = img::imgRead(tofImgChnAttr);
                if (!tofImg.isEmptyFrame()) {
                    UdpDataPacket packet;
                    packet.data.resize(kFrameBytes);
                    packet.type = UdpPacketType::TOF_IMAGE;
                    std::memcpy(packet.data.data(), tofImg.ptr(), kFrameBytes);

                    {
                        std::lock_guard<std::mutex> lock(g_udpMutex);
                        g_udpRing.push(std::move(packet));
                    }
                    g_udpCV.notify_one();
                }
            }
            // 标准模式测距+跟踪
            else if (g_sysConfig.workMode == UartComm::WorkMode::STANDARD)
            {
                auto start_time = std::chrono::steady_clock::now();

                if (!g_trackingEnabled.load()) {
                    hasAppliedTrackingDistance = false;
                    trackingGeneration = g_trackingGeneration.load();
                } else {
                    const uint64_t currentTrackingGeneration = g_trackingGeneration.load();
                    if (currentTrackingGeneration != trackingGeneration) {
                        trackingGeneration = currentTrackingGeneration;
                        hasAppliedTrackingDistance = false;
                    }

                    DistanceFrameShared localFrame;
                    bool hasNewFrame = false;
                    {
                        std::unique_lock<std::mutex> lock(g_distanceFrameMutex);
                        g_distanceFrameCV.wait_for(lock, kFrameWaitTimeout, [&] {
                            return g_distanceFrameShared.valid && g_distanceFrameShared.frameSeq != lastFrameSequence;
                        });

                        if (g_distanceFrameShared.valid && g_distanceFrameShared.frameSeq != lastFrameSequence) {
                            localFrame = g_distanceFrameShared;
                            lastFrameSequence = localFrame.frameSeq;
                            hasNewFrame = true;
                        }
                    }

                    if (hasNewFrame) {
                        float targetDistance = 0.0f;
                        const std::size_t trackingRoiSize = g_trackingRoiSize.load();
                        if (calculateMedianDistance(localFrame.dist.data(), localFrame.dist.size(), localFrame.rows, localFrame.cols, trackingRoiSize, targetDistance)) {
                            const bool shouldUpdateDelay = !hasAppliedTrackingDistance || std::fabs(targetDistance - lastAppliedDistance) >= kDelayUpdateDistanceThreshold;

                            if (PreprocessUart::updateTrackingDistance(targetDistance, shouldUpdateDelay)) {
                                if (shouldUpdateDelay) {
                                    lastAppliedDistance = targetDistance;
                                    hasAppliedTrackingDistance = true;
                                }
                            }

                            Logger::instance().info(("Thread TofProcess - Tracking distance: " + std::to_string(targetDistance) +
                                                     "m, ROI size: " + std::to_string(trackingRoiSize) +
                                                     ", lastAppliedDistance: " + std::to_string(lastAppliedDistance) + "m, shouldUpdateDelay: " + std::to_string(shouldUpdateDelay)).c_str());
                        } else {
                            Logger::instance().debug("Thread TofProcess - No valid distance in tracking ROI");
                        }
                    }
                }

                auto computationTime = std::chrono::steady_clock::now() - start_time;
                if (computationTime < kComputeInterval)
                {
                    std::this_thread::sleep_for(kComputeInterval - computationTime);
                }
            }
        }
    }

} // namespace TofProcesser
