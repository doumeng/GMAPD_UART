/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-17 19:36:19
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-21 15:14:39
 * @FilePath: /GMAPD_UART/task_module/header/task_reg.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#pragma once

// 1. C/C++ 标准库
#include <atomic>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>

// 2. 本项目其他头文件
#include "buffer.h"
#include "uart_read.h"
#include "udp_send.h"
#include "util.h"


extern UartComm::SystemConfig g_sysConfig;
extern UartComm::MotionData g_motionData;
extern UartComm::HistConfig g_histConfig;

extern LatestRingBuffer<UdpDataPacket, 5> g_udpRing;
extern std::mutex g_udpMutex;
extern std::condition_variable g_udpCV;

struct DistanceFrameShared {
	static constexpr std::size_t kRows = 128;
	static constexpr std::size_t kCols = 128;
	static constexpr std::size_t kPixelCount = kRows * kCols;

	std::array<float, kPixelCount> dist{};
	int rows = static_cast<int>(kRows);
	int cols = static_cast<int>(kCols);
	uint64_t frameSeq = 0;
	bool valid = false;
};

extern DistanceFrameShared g_distanceFrameShared;
extern std::mutex g_distanceFrameMutex;
extern std::condition_variable g_distanceFrameCV;
extern std::atomic_bool g_trackingEnabled;
extern std::atomic<std::uint64_t> g_trackingGeneration;
extern std::atomic<std::size_t> g_trackingRoiSize;

extern UdpComm::UdpSender udp_Sender;

// 线程注册接口
void register_threads();
