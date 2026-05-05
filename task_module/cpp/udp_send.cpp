#include <cstring>
#include <ctime>
#include <cerrno>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <unistd.h>

#include "log.h"
#include "task_reg.h"
#include "udp_send.h"

namespace UdpComm {

    constexpr int kSendRetryMax = 3;
    constexpr useconds_t kSendRetryBackoffUs = 200;

    UdpSender::UdpSender(const std::string& ip, uint16_t port) {
        sockfd_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        if (sockfd_ < 0) {
            std::cerr << "socket creation failed" << std::endl;
        }
        memset(&dest_addr_, 0, sizeof(dest_addr_));
        dest_addr_.sin_family = AF_INET;
        dest_addr_.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &dest_addr_.sin_addr) <= 0) {
            std::cerr << "inet_pton failed: Invalid IP address: " << ip << std::endl;
            close(sockfd_);
        }

        // 非阻塞循环绑定，直到成功
        sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
        local_addr.sin_port = htons(9000); // 指定端口

        while (true) {
            int ret = bind(sockfd_, (struct sockaddr*)&local_addr, sizeof(local_addr));
            if (ret == 0) {
                std::cerr << "UDP socket bind success (non-blocking)" << std::endl;
                break;
            } else {
                std::cerr << "UDP socket bind failed, retrying..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        std::cerr << "UdpSender initialized: " << ip << ":" << port << std::endl;
    }

    UdpSender::~UdpSender() {
        if (sockfd_ >= 0) {
            close(sockfd_);
            Logger::instance().info("UdpSender socket closed");
        }
    }

    uint32_t UdpSender::getTimestamp() {
        // Equivalent to int(time.time()) in Python: seconds since epoch
        return static_cast<uint32_t>(std::time(nullptr));
    }

    uint8_t UdpSender::calcChecksum(const UdpFrame& frame) {
        // XOR from header to data (header, ctrl, transfer_id, fragment_idx, data)
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&frame);
        size_t len = offsetof(UdpFrame, checksum); // up to but not including checksum
        uint8_t sum = 0;
        for (size_t i = 0; i < len; ++i) {
            sum ^= ptr[i];
        }
        return sum & 0xFF;
    }

    bool UdpSender::sendFrame(const UdpFrame& frame, size_t* outRetryHits, size_t* outEagainHits) {
        for (int attempt = 0; attempt <= kSendRetryMax; ++attempt) {
            ssize_t sent = sendto(sockfd_, &frame, UDP_FRAME_TOTAL_LEN, 0,
                                  (struct sockaddr*)&dest_addr_, sizeof(dest_addr_));
            if (sent == UDP_FRAME_TOTAL_LEN) {
                Logger::instance().debug(("UDP frame sent, fragment_idx: " + std::to_string(frame.fragment_idx)).c_str());
                return true;
            }

            int err = errno;
            const bool retryable = (sent < 0) && (err == EAGAIN || err == EWOULDBLOCK);
            if (retryable && attempt < kSendRetryMax) {
                if (outRetryHits) {
                    ++(*outRetryHits);
                }
                if (outEagainHits) {
                    ++(*outEagainHits);
                }
                usleep(kSendRetryBackoffUs);
                continue;
            }

            Logger::instance().error(("Failed to send UDP frame, sent bytes: " + std::to_string(sent)).c_str());
            Logger::instance().error(("Expected bytes: " + std::to_string(UDP_FRAME_TOTAL_LEN)).c_str());
            Logger::instance().error(("UDP frame dropped, fragment_idx: " + std::to_string(frame.fragment_idx)).c_str());
            Logger::instance().error(("sendto errno=" + std::to_string(err) + ", msg=" + std::string(std::strerror(err))).c_str());
            return false;
        }

        return false;
    }

    bool UdpSender::sendData(const uint8_t* data, size_t length, uint32_t transfer_id, uint8_t task_type, int16_t pitch, int16_t yaw, SendDataStats* outStats) {
        SendDataStats localStats;
        if (!data || length == 0) {
            Logger::instance().debug("sendData called with empty data or zero length");
            if (outStats) {
                *outStats = localStats;
            }
            return false;
        }

        // 检查端口是否绑定成功
        if (sockfd_ < 0) {
            Logger::instance().debug("UDP socket not initialized or bind failed, cannot send data");
            if (outStats) {
                *outStats = localStats;
            }
            return false;
        }

        const size_t max_fragments = 128;
        const size_t fragment_size = UDP_FRAME_DATA_LEN;

        size_t total_fragments = (length + fragment_size - 1) / fragment_size;

        Logger::instance().debug((
            "UdpSender::sendData len=" + std::to_string(length) +
            ", fragment_size=" + std::to_string(fragment_size) +
            ", total_fragments=" + std::to_string(total_fragments) +
            ", transfer_id=" + std::to_string(transfer_id) +
            ", task_type=" + std::to_string(static_cast<int>(task_type))
        ).c_str());

        if (total_fragments > max_fragments) {
            Logger::instance().debug("Data too large, only sending first 128 fragments");
            total_fragments = max_fragments; // 最多128片
        }

        for (size_t idx = 0; idx < total_fragments; ++idx) {
            UdpFrame frame;
            frame.header = UDP_FRAME_HEADER;

            size_t offset = idx * fragment_size;
            size_t frag_len = std::min(fragment_size, length - offset);

            // ctrl: bit15 is_fragment, bit0-12 fragment length
            frame.ctrl = 0;
            if (total_fragments > 1) frame.ctrl |= (1 << 15); // is_fragment
            frame.ctrl |= (frag_len & 0x1FFF); // 13 bits for length

            // Set transfer_id (3 bytes) and task_type (1 byte)
            // Matches Python: struct.pack('>I', transfer_id)[1:4] => Big Endian, lower 3 bytes
            frame.transfer_id[0] = (transfer_id >> 16) & 0xFF;
            frame.transfer_id[1] = (transfer_id >> 8) & 0xFF;
            frame.transfer_id[2] = (transfer_id >> 0) & 0xFF;
            frame.task_type = task_type;
            
            frame.fragment_idx = static_cast<uint8_t>(idx);
            frame.pitch = pitch;
            frame.yaw = yaw;

            memset(frame.data, 0, fragment_size);
            memcpy(frame.data, data + offset, frag_len);

            frame.checksum = calcChecksum(frame);
            ++localStats.checksumCalculated;
            frame.tail = UDP_FRAME_TAIL;

            if (!sendFrame(frame, &localStats.retryHits, &localStats.eagainHits)) {
                ++localStats.frameSendFail;
                Logger::instance().error(("UDP send failed at fragment idx=" + std::to_string(idx) + ", dropping remaining data.").c_str());
                if (outStats) {
                    *outStats = localStats;
                }
                return false; // 只要有一帧发送失败，立即丢弃剩余数据
            }
            ++localStats.frameSendOk;
        }

        if (outStats) {
            *outStats = localStats;
        }
        
        return true;
    }

    void thread_UdpSend() {
        
        Logger::instance().info("Thread UdpSend - Starting");
        static uint32_t task_id = 0;

        auto reportStart = std::chrono::steady_clock::now();
        uint64_t dequeuedPackets = 0;
        uint64_t depthPackets = 0;
        uint64_t tofPackets = 0;
        uint64_t depthSendOk = 0;
        uint64_t depthSendFail = 0;
        uint64_t tofSendOk = 0;
        uint64_t tofSendFail = 0;
        uint64_t dataMismatchDrops = 0;
        uint64_t depthFragmentsAttempt = 0;
        uint64_t tofFragmentsAttempt = 0;
        uint64_t depthChecksumCalculated = 0;
        uint64_t tofChecksumCalculated = 0;
        uint64_t depthFragSendOk = 0;
        uint64_t depthFragSendFail = 0;
        uint64_t tofFragSendOk = 0;
        uint64_t tofFragSendFail = 0;
        uint64_t depthFragRetryHits = 0;
        uint64_t depthFragEagainHits = 0;
        uint64_t tofFragRetryHits = 0;
        uint64_t tofFragEagainHits = 0;
        
        while (1) {
            std::unique_lock<std::mutex> lock(g_udpMutex);

            if (!g_udpRing.hasData()) {
                g_udpCV.wait_for(lock, std::chrono::milliseconds(100), [&]{
                    return g_udpRing.hasData();
                });
            }

            UdpDataPacket pkt;
            if (!g_udpRing.popFront(pkt)) {
                lock.unlock();
                continue;
            }
            lock.unlock();

            ++dequeuedPackets;

            task_id = (task_id + 1) % 0xFFFFFF;

            if (pkt.type == UdpPacketType::TOF_IMAGE && !pkt.data.empty()) {
                ++tofPackets;
                const size_t tofFragments = (pkt.data.size() + UDP_FRAME_DATA_LEN - 1) / UDP_FRAME_DATA_LEN;
                tofFragmentsAttempt += tofFragments;
                SendDataStats sendStats;

                if (udp_Sender.sendData(pkt.data.data(), pkt.data.size(), task_id, 1, 0, 0, &sendStats)) {
                    ++tofSendOk;
                } else {
                    ++tofSendFail;
                }
                tofChecksumCalculated += sendStats.checksumCalculated;
                tofFragSendOk += sendStats.frameSendOk;
                tofFragSendFail += sendStats.frameSendFail;
                tofFragRetryHits += sendStats.retryHits;
                tofFragEagainHits += sendStats.eagainHits;
            }
            else if (pkt.type == UdpPacketType::DEPTH_IMAGE) {
                ++depthPackets;
                size_t pixel_count = pkt.rows * pkt.cols;
                if (pkt.dist.size() == pixel_count && pkt.inten.size() == pixel_count && pkt.raw.size() == pixel_count) {

                    size_t data_size = pixel_count * sizeof(uint32_t);
                    size_t raw_data_size = pixel_count * sizeof(int32_t);
                    size_t total_size = data_size + raw_data_size;
                    const size_t depthFragments = (raw_data_size + UDP_FRAME_DATA_LEN - 1) / UDP_FRAME_DATA_LEN;
                    depthFragmentsAttempt += depthFragments;
                    SendDataStats sendStats;

                    if (udp_Sender.sendData(reinterpret_cast<uint8_t*>(pkt.raw.data()), raw_data_size, task_id, 0, -1000, 1000, &sendStats)) {
                        ++depthSendOk;
                    } else {
                        ++depthSendFail;
                    }
                    depthChecksumCalculated += sendStats.checksumCalculated;
                    depthFragSendOk += sendStats.frameSendOk;
                    depthFragSendFail += sendStats.frameSendFail;
                    depthFragRetryHits += sendStats.retryHits;
                    depthFragEagainHits += sendStats.eagainHits;
                } else {
                    ++dataMismatchDrops;
                    Logger::instance().warning(("UDP data dropped due to size mismatch. Expected pixel_count: " + 
                        std::to_string(pixel_count) + ", dist.size: " + std::to_string(pkt.dist.size()) + 
                        ", inten.size: " + std::to_string(pkt.inten.size()) + 
                        ", raw.size: " + std::to_string(pkt.raw.size())).c_str());
                }
            }

            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - reportStart).count();
            if (elapsedMs >= 1000) {
                double sec = static_cast<double>(elapsedMs) / 1000.0;
                Logger::instance().info((
                    "UdpSendStats "
                    "dequeued=" + std::to_string(dequeuedPackets) +
                    ", depth_pkt=" + std::to_string(depthPackets) +
                    ", tof_pkt=" + std::to_string(tofPackets) +
                    ", depth_ok=" + std::to_string(depthSendOk) +
                    ", depth_fail=" + std::to_string(depthSendFail) +
                    ", tof_ok=" + std::to_string(tofSendOk) +
                    ", tof_fail=" + std::to_string(tofSendFail) +
                    ", mismatch_drop=" + std::to_string(dataMismatchDrops) +
                    ", depth_frag=" + std::to_string(depthFragmentsAttempt) +
                    ", tof_frag=" + std::to_string(tofFragmentsAttempt) +
                    ", depth_checksum_calc=" + std::to_string(depthChecksumCalculated) +
                    ", tof_checksum_calc=" + std::to_string(tofChecksumCalculated) +
                    ", depth_frag_send_ok=" + std::to_string(depthFragSendOk) +
                    ", depth_frag_send_fail=" + std::to_string(depthFragSendFail) +
                    ", tof_frag_send_ok=" + std::to_string(tofFragSendOk) +
                    ", tof_frag_send_fail=" + std::to_string(tofFragSendFail) +
                    ", depth_retry_hits=" + std::to_string(depthFragRetryHits) +
                    ", depth_eagain_hits=" + std::to_string(depthFragEagainHits) +
                    ", tof_retry_hits=" + std::to_string(tofFragRetryHits) +
                    ", tof_eagain_hits=" + std::to_string(tofFragEagainHits) +
                    ", depth_ok_fps=" + std::to_string(static_cast<double>(depthSendOk) / sec) +
                    ", tof_ok_fps=" + std::to_string(static_cast<double>(tofSendOk) / sec)
                ).c_str());

                reportStart = now;
                dequeuedPackets = 0;
                depthPackets = 0;
                tofPackets = 0;
                depthSendOk = 0;
                depthSendFail = 0;
                tofSendOk = 0;
                tofSendFail = 0;
                dataMismatchDrops = 0;
                depthFragmentsAttempt = 0;
                tofFragmentsAttempt = 0;
                depthChecksumCalculated = 0;
                tofChecksumCalculated = 0;
                depthFragSendOk = 0;
                depthFragSendFail = 0;
                tofFragSendOk = 0;
                tofFragSendFail = 0;
                depthFragRetryHits = 0;
                depthFragEagainHits = 0;
                tofFragRetryHits = 0;
                tofFragEagainHits = 0;
            }
        }
        Logger::instance().info("Thread UdpSend - Stopped");
    }
}