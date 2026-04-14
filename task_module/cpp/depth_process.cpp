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
#include <utility>

#include <opencv2/opencv.hpp>
#include <open3d/Open3D.h>


namespace PointCloud {
    // Helper to convert float vector to uint16 vector
    std::vector<uint16_t> Float2Uint16(const std::vector<float>& floatArray) {
        std::vector<uint16_t> result(floatArray.size());
        for (size_t i = 0; i < floatArray.size(); ++i) {
            result[i] = static_cast<uint16_t>(floatArray[i]);
        }
        return result;
    }

    // 1. 解析原始数据中的强度和距离，并将飞行时间转换为距离（*10 缩放存为 uint16）
    void parseDepthData(const uint32_t* rawData, int rows, int cols, int16_t offset,
                        cv::Mat& intenMat, cv::Mat& distMat) {
        intenMat.create(rows, cols, CV_16UC1);
        distMat.create(rows, cols, CV_16UC1);
        const uint16_t* raw_ptr = reinterpret_cast<const uint16_t*>(rawData);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                size_t idx = r * cols + c;
                intenMat.at<uint16_t>(r, c) = raw_ptr[2 * idx];      // 强度在前
                uint16_t tof = raw_ptr[2 * idx + 1];                  // 飞行时间在后
                float real_dist = ((16000.0f - 2.0f * tof) + offset) * 0.15f;
                distMat.at<uint16_t>(r, c) = static_cast<uint16_t>(real_dist * 10.0f);
            }
        }
    }

    // 2.0 根据强度阈值对强度图和距离图进行降噪（低于阈值的像素清零）
    void denoiseByIntensity(cv::Mat& intenMat, cv::Mat& distMat, uint8_t threshold) {
        for (int r = 0; r < intenMat.rows; ++r) {
            for (int c = 0; c < intenMat.cols; ++c) {
                if (intenMat.at<uint16_t>(r, c) < threshold) {
                    intenMat.at<uint16_t>(r, c) = 0;
                    distMat.at<uint16_t>(r, c)  = 0;
                }
            }
        }
    }

    // 2.1 根据距离值区间进行降噪（距离过近或过远的像素清零）
    void denoiseByDistance(cv::Mat& intenMat, cv::Mat& distMat, float minDist, float maxDist) {
        for (int r = 0; r < distMat.rows; ++r) {
            for (int c = 0; c < distMat.cols; ++c) {
                float dist = static_cast<float>(distMat.at<uint16_t>(r, c)) / 10.0f;
                if (dist < minDist || dist > maxDist) {
                    intenMat.at<uint16_t>(r, c) = 0;
                    distMat.at<uint16_t>(r, c)  = 0;
                }
            }
        }
    }

    // 2.2 使用 Open3D DBSCAN 进行降噪，去除噪声点后重排输出并零填充
    void denoiseByDbscanOpen3D(const cv::Mat& intenIn, const cv::Mat& distIn,
                               cv::Mat& intenOut, cv::Mat& distOut,
                               float eps, int minSamples) {
        if (intenIn.empty() || distIn.empty() ||
            intenIn.size() != distIn.size() ||
            intenIn.type() != CV_16UC1 ||
            distIn.type() != CV_16UC1) {
            Logger::instance().error("denoiseByDbscanOpen3D invalid input matrix");
            intenOut = intenIn.clone();
            distOut = distIn.clone();
            return;
        }

        float resolvedEps = eps;
        int resolvedMinSamples = minSamples;

        if (resolvedEps < 0.0f || resolvedMinSamples < 0) {
            std::lock_guard<std::mutex> lock(g_histConfig.mtx);
            if (resolvedEps < 0.0f) {
                resolvedEps = g_histConfig.dbscanEps;
            }
            if (resolvedMinSamples < 0) {
                resolvedMinSamples = static_cast<int>(g_histConfig.dbscanMinSamples);
            }
        }

        if (resolvedEps <= 0.0f || resolvedMinSamples <= 0) {
            Logger::instance().error("denoiseByDbscanOpen3D invalid eps or minSamples");
            intenOut = intenIn.clone();
            distOut = distIn.clone();
            return;
        }

        struct ValidPoint {
            uint16_t intensity = 0;
            uint16_t distance = 0;
            Eigen::Vector3d feature = Eigen::Vector3d::Zero();
        };

        std::vector<ValidPoint> validPoints;
        validPoints.reserve(static_cast<size_t>(intenIn.rows) * static_cast<size_t>(intenIn.cols));

        for (int r = 0; r < intenIn.rows; ++r) {
            for (int c = 0; c < intenIn.cols; ++c) {
                const uint16_t intensity = intenIn.at<uint16_t>(r, c);
                const uint16_t distance = distIn.at<uint16_t>(r, c);
                if (distance == 0) {
                    continue;
                }

                ValidPoint vp;
                vp.intensity = intensity;
                vp.distance = distance;
                vp.feature = Eigen::Vector3d(
                    static_cast<double>(c),
                    static_cast<double>(r),
                    static_cast<double>(distance) / 10.0
                );
                validPoints.push_back(std::move(vp));
            }
        }

        intenOut = cv::Mat::zeros(intenIn.size(), CV_16UC1);
        distOut = cv::Mat::zeros(distIn.size(), CV_16UC1);

        if (validPoints.empty()) {
            return;
        }

        open3d::geometry::PointCloud cloud;
        cloud.points_.reserve(validPoints.size());
        for (const auto& vp : validPoints) {
            cloud.points_.push_back(vp.feature);
        }

        const std::vector<int> labels = cloud.ClusterDBSCAN(
            static_cast<double>(resolvedEps), resolvedMinSamples, false);
        
        if (labels.size() != validPoints.size()) {
            Logger::instance().error("denoiseByDbscanOpen3D DBSCAN labels size mismatch");
            intenOut = intenIn.clone();
            distOut = distIn.clone();
            return;
        }

        size_t writeIndex = 0;
        const size_t maxWrite = static_cast<size_t>(intenOut.rows) * static_cast<size_t>(intenOut.cols);
        for (size_t i = 0; i < labels.size(); ++i) {
            if (labels[i] < 0) {
                continue;
            }
            if (writeIndex >= maxWrite) {
                break;
            }

            const int outR = static_cast<int>(writeIndex / static_cast<size_t>(intenOut.cols));
            const int outC = static_cast<int>(writeIndex % static_cast<size_t>(intenOut.cols));
            intenOut.at<uint16_t>(outR, outC) = validPoints[i].intensity;
            distOut.at<uint16_t>(outR, outC) = validPoints[i].distance;
            ++writeIndex;
        }
    }

    // 3. 对强度图和距离图执行形态学开运算（腐蚀+膨胀），去除孤立散点
    void morphologicalFilter(const cv::Mat& intenIn, const cv::Mat& distIn,
                             cv::Mat& intenOut, cv::Mat& distOut, uint8_t kSize) {
        if (kSize == 0) kSize = 1; // 防止核大小为 0
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kSize, kSize));
        cv::morphologyEx(intenIn, intenOut, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(distIn,  distOut,  cv::MORPH_OPEN, kernel);
    }

    // 4. 将处理后的强度图和距离图打包输出到 UdpDataPacket
    void packDepthOutput(const cv::Mat& intenMat, const cv::Mat& distMat,
                         int rows, int cols, UdpDataPacket& udpPkt) {
        size_t pixel_count = rows * cols;
        udpPkt.dist.resize(pixel_count, 0.0f);
        udpPkt.inten.resize(pixel_count, 0);
        udpPkt.raw.resize(pixel_count, 0);

        uint16_t* out_raw_ptr = reinterpret_cast<uint16_t*>(udpPkt.raw.data());
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                size_t index = r * cols + c;
                uint16_t inten = intenMat.at<uint16_t>(r, c);
                uint16_t dist  = distMat.at<uint16_t>(r, c);
                out_raw_ptr[2 * index]     = inten;
                out_raw_ptr[2 * index + 1] = dist;
                udpPkt.inten[index] = inten;
                udpPkt.dist[index]  = static_cast<float>(dist) / 10.0f;  // 还原为实际距离
            }
        }
    }

    // 传输未降噪数据，将飞行时间转化为距离
    void processUndenoisedDepthData(const uint32_t* rawData, int rows, int cols, int16_t offset, UdpDataPacket& udpPkt) {
        cv::Mat intenMat, distMat;
        parseDepthData(rawData, rows, cols, offset, intenMat, distMat);
        packDepthOutput(intenMat, distMat, rows, cols, udpPkt);
    }

    // 完整处理流水线：依次调用上述四个接口
    void processDenoisedDepthImage(const uint32_t* rawData, int rows, int cols, int16_t offset, UdpDataPacket& udpPkt) {
        cv::Mat intenMat, distMat;
        parseDepthData(rawData, rows, cols, offset, intenMat, distMat);

        // denoiseByIntensity(intenMat, distMat, g_histConfig.denoiseLevel);
        denoiseByDistance(intenMat, distMat, g_histConfig.minDistance, g_histConfig.maxDistance);
        denoiseByDbscanOpen3D(intenMat, distMat, intenMat, distMat, g_histConfig.dbscanEps, g_histConfig.dbscanMinSamples);

        cv::Mat intenFiltered, distFiltered;
        morphologicalFilter(intenMat, distMat, intenFiltered, distFiltered, g_histConfig.kernalSize);

        packDepthOutput(intenFiltered, distFiltered, rows, cols, udpPkt);
    }

    void thread_PointCloudProcess()
    {
        constexpr auto kComputeInterval = std::chrono::milliseconds(20);

        while (1)
        {

            auto start_time = std::chrono::high_resolution_clock::now();

            if (g_sysConfig.workMode == UartComm::WorkMode::STANDARD)
            {

                img::ImgMod depthImg = img::imgRead(depthImgChnAttr);

                if(depthImg.isEmptyFrame()){
                    Logger::instance().debug("Thread PointCloudProcess - Failed to read point cloud from PCIe");
                    continue;
                }
                else{
                    Logger::instance().debug("Thread PointCloudProcess - Successfully read point cloud from PCIe");
                    
                    // 发送 UDP 数据 (放入队列)
                    {
                        UdpDataPacket udpPkt;
                        udpPkt.type = UdpPacketType::DEPTH_IMAGE;
                        udpPkt.rows = 128;
                        udpPkt.cols = 128;

#if 0
                        // 调用 OpenCV 接口处理图像，并填充距离和强度
                        uint32_t* rawData = reinterpret_cast<uint32_t*>(depthImg.ptr());
                        processUndenoisedDepthData(rawData, udpPkt.rows, udpPkt.cols, g_sysConfig.enDelay, udpPkt);
#endif

#if 1
                        const uint32_t* rawData = reinterpret_cast<const uint32_t*>(depthImg.ptr());
                        processDenoisedDepthImage(rawData, udpPkt.rows, udpPkt.cols, g_sysConfig.enDelay, udpPkt);
#endif
                        img::imgWrite(outModAttr, udpPkt.raw.data(), 2 * udpPkt.rows * udpPkt.cols * sizeof(uint16_t)); // 每个像素 4 字节 (强度+距离)

                        {
                            std::lock_guard<std::mutex> lock(g_udpMutex);
                            g_udpRing.push(std::move(udpPkt));
                        }

                        g_udpCV.notify_one();
                    }
                }
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
