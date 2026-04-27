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
    std::vector<uint16_t> Float2Uint16(const std::vector<float> &floatArray) {
        std::vector<uint16_t> result(floatArray.size());
        for (size_t i = 0; i < floatArray.size(); ++i) {
            result[i] = static_cast<uint16_t>(floatArray[i]);
        }
        return result;
    }

    // 1. 解析原始数据中的强度和距离，并将飞行时间转换为距离
    void parseDepthData(const uint32_t *rawData, int rows, int cols, int16_t offset,
                        cv::Mat &intenMat, cv::Mat &distMat) {

        intenMat.create(rows, cols, CV_16UC1);
        distMat.create(rows, cols, CV_32FC1);

        const uint16_t *raw_ptr = reinterpret_cast<const uint16_t *>(rawData);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                size_t idx = r * cols + c;
                uint16_t inten = raw_ptr[2 * idx];                    // 强度在前
                uint16_t tof = raw_ptr[2 * idx + 1];                  // 飞行时间在后
                
                if (tof < 4000 || tof > 7900) {
                    intenMat.at<uint16_t>(r, c) = 0;
                    distMat.at<float>(r, c) = 0.0f;
                    continue;
                }
                
                intenMat.at<uint16_t>(r, c) = inten;
                float real_dist = ((16000.0f - 2.0f * tof) + offset) * 0.15f;
                distMat.at<float>(r, c) = real_dist;
            }
        }
    }

    // 2.0 根据强度阈值对强度图和距离图进行降噪（低于阈值的像素清零）
    void denoiseByIntensity(cv::Mat &intenMat, cv::Mat &distMat, uint8_t threshold) {
        for (int r = 0; r < intenMat.rows; ++r) {
            for (int c = 0; c < intenMat.cols; ++c) {
                if (intenMat.at<uint16_t>(r, c) < threshold) {
                    intenMat.at<uint16_t>(r, c) = 0;
                    distMat.at<float>(r, c) = 0.0f;
                }
            }
        }
    }

    // 2.1 根据距离值区间进行降噪（距离过近或过远的像素清零）
    void denoiseByDistance(cv::Mat &intenMat, cv::Mat &distMat, float minDist, float maxDist) {
        for (int r = 0; r < distMat.rows; ++r) {
            for (int c = 0; c < distMat.cols; ++c) {
                float dist = distMat.at<float>(r, c);
                if (dist < minDist || dist > maxDist) {
                    intenMat.at<uint16_t>(r, c) = 0;
                    distMat.at<float>(r, c) = 0.0f;
                }
            }
        }
    }

    // 2.2 使用 Open3D DBSCAN 进行降噪，去除噪声点后重排输出并零填充
    void denoiseByDbscanOpen3D(const cv::Mat &intenIn, const cv::Mat &distIn,
                               cv::Mat &intenOut, cv::Mat &distOut, float eps, int minSamples) {

        if (intenIn.empty() || distIn.empty() ||
            intenIn.size() != distIn.size() ||
            intenIn.type() != CV_16UC1 ||
            distIn.type() != CV_32FC1) {
            Logger::instance().error("Thread PointCloudProcess - denoiseByDbscanOpen3D invalid input matrix");
            intenOut = intenIn.clone();
            distOut = distIn.clone();
            return;
        }

        float resolvedEps = eps;
        int resolvedMinSamples = minSamples;

        struct ValidPoint {
            uint16_t intensity = 0;
            float distance = 0.0f;
            Eigen::Vector3d feature = Eigen::Vector3d::Zero();
        };

        std::vector<ValidPoint> validPoints;
        validPoints.reserve(static_cast<size_t>(intenIn.rows) * static_cast<size_t>(intenIn.cols));

        for (int r = 0; r < intenIn.rows; ++r) {
            for (int c = 0; c < intenIn.cols; ++c) {
                const uint16_t intensity = intenIn.at<uint16_t>(r, c);
                const float distance = distIn.at<float>(r, c);
                if (distance == 0.0f) {
                    continue;
                }

                ValidPoint vp;
                vp.intensity = intensity;
                vp.distance = distance;
                vp.feature = Eigen::Vector3d(
                        static_cast<double>(c),
                        static_cast<double>(r),
                        static_cast<double>(distance)
                );
                validPoints.push_back(std::move(vp));
            }
        }

        intenOut = cv::Mat::zeros(intenIn.size(), CV_16UC1);
        distOut = cv::Mat::zeros(distIn.size(), CV_32FC1);

        if (validPoints.empty()) {
            return;
        }

        open3d::geometry::PointCloud cloud;
        cloud.points_.reserve(validPoints.size());
        for (const auto &vp: validPoints) {
            cloud.points_.push_back(vp.feature);
        }

        Logger::instance().debug(("Thread PointCloudProcess - denoiseByDbscanOpen3D - Valid points count: " +
                                 std::to_string(validPoints.size())).c_str());

        const std::vector<int> labels = cloud.ClusterDBSCAN(
                static_cast<double>(resolvedEps), resolvedMinSamples, false);

        if (labels.size() != validPoints.size()) {
            Logger::instance().error("Thread PointCloudProcess - denoiseByDbscanOpen3D - DBSCAN labels size mismatch");
            intenOut = intenIn.clone();
            distOut = distIn.clone();
            return;
        }

        for (size_t i = 0; i < labels.size(); ++i) {
            if (labels[i] < 0) {
                continue;
            }

            const int outR = static_cast<int>(validPoints[i].feature[1]);
            const int outC = static_cast<int>(validPoints[i].feature[0]);
            intenOut.at<uint16_t>(outR, outC) = validPoints[i].intensity;
            distOut.at<float>(outR, outC) = validPoints[i].distance;
        }
    }

    //备份填充函数
    template<typename T>
    void FillSmallHoles(const T* src, T* dst, int rows, int cols, int max_hole_area)
    {
        cv::Mat mat(rows, cols, cv::DataType<T>::type, const_cast<T*>(src));
        cv::Mat filled = mat.clone();

        // 0值为空洞
        cv::Mat mask = (mat == 0);

        // 连通域分析，找出所有空洞
        cv::Mat labels, stats, centroids;
        int n_labels = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 4, CV_32S);

        for (int i = 1; i < n_labels; ++i) { // label 0 是背景
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area > 0 && area <= max_hole_area) {
                // 只对小空洞做填充
                cv::Mat hole_mask = (labels == i);

                // 找到空洞像素
                std::vector<cv::Point> hole_pixels;
                for (int y = 0; y < rows; ++y) {
                    for (int x = 0; x < cols; ++x) {
                        if (hole_mask.at<uchar>(y, x)) {
                            hole_pixels.emplace_back(x, y);
                        }
                    }
                }

                // 计算空洞边界的非零邻域均值
                double sum = 0;
                int count = 0;
                for (const auto& pt : hole_pixels) {
                    // 8邻域
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            int ny = pt.y + dy;
                            int nx = pt.x + dx;
                            if (ny >= 0 && ny < rows && nx >= 0 && nx < cols) {
                                if (!hole_mask.at<uchar>(ny, nx)) {
                                    T val = filled.at<T>(ny, nx);
                                    if (val != 0) {
                                        sum += val;
                                        count++;
                                    }
                                }
                            }
                        }
                    }
                }
                T fill_val = (count > 0) ? static_cast<T>(sum / count) : 0;

                // 填充整个空洞
                for (const auto& pt : hole_pixels) {
                    filled.at<T>(pt.y, pt.x) = fill_val;
                }
            }
        }
        memcpy(dst, filled.data, rows * cols * sizeof(T));
    }

    template<typename T>
    void FillHolesDilate(const T* src, T* dst, int rows, int cols, int kernal_size)
    {
        cv::Mat src_mat(rows, cols, cv::DataType<T>::type, const_cast<T*>(src));
        cv::Mat dilated;

        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernal_size, kernal_size));
        
        cv::dilate(src_mat, dilated, kernel);

        const T* dilated_data = reinterpret_cast<const T*>(dilated.data);
        for (int i = 0; i < rows * cols; ++i) {
            if (src[i] == static_cast<T>(0)) {
                dst[i] = dilated_data[i];
            } else if (src != dst) {
                dst[i] = src[i];
            }
        }
    }

    // 3. 对强度图和距离图执行膨胀操作，填补空洞
    void morphologicalFilter(const cv::Mat &intenIn, const cv::Mat &distIn,
                             cv::Mat &intenOut, cv::Mat &distOut, uint8_t kSize) {
        intenOut.create(intenIn.size(), CV_16UC1);
        distOut.create(distIn.size(), CV_32FC1);
        

        if (kSize > 5){
            FillSmallHoles<uint16_t>(reinterpret_cast<const uint16_t*>(intenIn.data),
                                 reinterpret_cast<uint16_t*>(intenOut.data),
                                 intenIn.rows, intenIn.cols, 64);
            FillSmallHoles<float>(reinterpret_cast<const float*>(distIn.data),
                                 reinterpret_cast<float*>(distOut.data),
                                 distIn.rows, distIn.cols, 64);
        }else{
            FillHolesDilate<uint16_t>(reinterpret_cast<const uint16_t*>(intenIn.data),
                                   reinterpret_cast<uint16_t*>(intenOut.data),
                                   intenIn.rows, intenIn.cols, kSize);
                                  
            FillHolesDilate<float>(reinterpret_cast<const float*>(distIn.data),
                                   reinterpret_cast<float*>(distOut.data),
                                   distIn.rows, distIn.cols, kSize);
        }
    }

    // 4. 将处理后的强度图和距离图打包输出到 UdpDataPacket
    void packDepthOutput(const cv::Mat &intenMat, const cv::Mat &distMat,
                         int rows, int cols, UdpDataPacket &udpPkt) {
        cv::Mat rotatedInten, rotatedDist;
        cv::rotate(intenMat, rotatedInten, cv::ROTATE_90_COUNTERCLOCKWISE);
        cv::flip(rotatedInten, rotatedInten, 1);
        cv::rotate(distMat, rotatedDist, cv::ROTATE_90_COUNTERCLOCKWISE);
        cv::flip(rotatedDist, rotatedDist, 1);

        int new_rows = rotatedInten.rows;
        int new_cols = rotatedInten.cols;

        size_t pixel_count = new_rows * new_cols;
        udpPkt.dist.resize(pixel_count, 0.0f);
        udpPkt.inten.resize(pixel_count, 0);
        udpPkt.raw.resize(pixel_count, 0);

        uint16_t *out_raw_ptr = reinterpret_cast<uint16_t *>(udpPkt.raw.data());
        
        for (int r = 0; r < new_rows; ++r) {
            for (int c = 0; c < new_cols; ++c) {
                size_t index = r * new_cols + c;
                uint16_t inten = rotatedInten.at<uint16_t>(r, c);
                float dist = rotatedDist.at<float>(r, c);
                out_raw_ptr[2 * index] = inten;
                out_raw_ptr[2 * index + 1] = static_cast<uint16_t>(dist * 10.0f);
                udpPkt.inten[index] = inten;
                udpPkt.dist[index] = dist;  // 还原为实际距离
            }
        }
    }

    // 传输未降噪数据，将飞行时间转化为距离
    void processUndenoisedDepthData(const uint32_t *rawData, int rows, int cols, int16_t offset, UdpDataPacket &udpPkt) {
        cv::Mat intenMat, distMat;
        parseDepthData(rawData, rows, cols, offset, intenMat, distMat);
        packDepthOutput(intenMat, distMat, rows, cols, udpPkt);
    }

    // 完整处理流水线：依次调用上述四个接口
    void processDenoisedDepthImage(const uint32_t *rawData, int rows, int cols, int16_t offset, UdpDataPacket &udpPkt) {
        cv::Mat intenMat, distMat;
        
        parseDepthData(rawData, rows, cols, offset, intenMat, distMat);

        Logger::instance().debug(("Thread PointCloudProcess - denoiseByDistance: " + std::to_string(g_histConfig.minDistance) + "m to "
        + std::to_string(g_histConfig.maxDistance) + "m").c_str());

        denoiseByDistance(intenMat, distMat, g_histConfig.minDistance, g_histConfig.maxDistance);

        if (g_histConfig.dbscanEps > 0 && g_histConfig.dbscanMinSamples > 0)
        {
            Logger::instance().debug(("Thread PointCloudProcess - denoiseByDbscanOpen3D - Running DBSCAN with eps: " +
                                     std::to_string(g_histConfig.dbscanEps) + ", minSamples: " +
                                     std::to_string(g_histConfig.dbscanMinSamples)).c_str());

            denoiseByDbscanOpen3D(intenMat, distMat, intenMat, distMat, g_histConfig.dbscanEps,
                                  g_histConfig.dbscanMinSamples);
        }

        cv::Mat intenFiltered, distFiltered;

#if 1
        if (g_histConfig.kernalSize > 0) {
            morphologicalFilter(intenMat, distMat, intenFiltered, distFiltered, g_histConfig.kernalSize);
        } else 
#endif        
        {
            intenFiltered = intenMat;
            distFiltered = distMat;
        }

//        cv::rotate(intenFiltered, intenFiltered, cv::ROTATE_90_CLOCKWISE); // 逆时针旋转90度
//        cv::flip(intenMat, intenMat, 1); // 水平翻转
//        cv::rotate(distFiltered, distFiltered, cv::ROTATE_90_CLOCKWISE); // 逆时针旋转90度
//        cv::flip(distMat, distMat, 1); // 水平翻转

        packDepthOutput(intenFiltered, distFiltered, rows, cols, udpPkt);
    }

    void thread_PointCloudProcess() {

        constexpr auto kComputeInterval = std::chrono::milliseconds(20);

        while (1)
        {

            auto start_time = std::chrono::high_resolution_clock::now();

            if (g_sysConfig.workMode == UartComm::WorkMode::STANDARD) {

                img::ImgMod depthImg = img::imgRead(depthImgChnAttr);

                if (depthImg.isEmptyFrame()) {
                    Logger::instance().debug("Thread PointCloudProcess - Failed to read point cloud from PCIe");
                    continue;
                } else {
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
                        uint32_t* rawData = reinterpret_cast<uint32_t *>(depthImg.ptr());
                        processDenoisedDepthImage(rawData, udpPkt.rows, udpPkt.cols, g_sysConfig.enDelay, udpPkt);
#endif
                        img::imgWrite(outModAttr, udpPkt.raw.data(),  2 * udpPkt.rows * udpPkt.cols * sizeof(uint16_t)); // 每个像素 4 字节 (强度+距离)

                        {
                            std::lock_guard<std::mutex> lock(g_udpMutex);
                            g_udpRing.push(std::move(udpPkt));
                        }

                        g_udpCV.notify_one();
                    }
                }

                auto computationTime = std::chrono::high_resolution_clock::now() - start_time;
                long long duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(computationTime).count();
                Logger::instance().debug(("Thread PointCloudProcess - Computation time: " + std::to_string(duration_ms) + "ms").c_str());

                if (computationTime < kComputeInterval)
                {
                    std::this_thread::sleep_for(kComputeInterval - computationTime);
                }
            }
        }
    }
}