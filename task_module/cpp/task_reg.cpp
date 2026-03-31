/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-17 19:36:19
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-31 09:54:30
 * @FilePath: /GMAPD_UART/task_module/cpp/task_reg.cpp
 * @Description: 通信线程 点云处理线程 tof处理 udp发送 PCIE发送 线程注册
 */
#include "task_reg.h"

// 1. C 标准库 (C++封装)
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>

// 2. C++ 标准库
#include <algorithm>
#include <array>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// 3. 本项目其他头文件
#include "apd_control.h"
#include "cooler_control.h"
#include "log.h"
#include "depth_process.h"
#include "tof_process.h"
#include "preprocess_uart_slave.h"
#include "uart_read.h"

// UDP 发送器实例
std::string ip_address = "192.168.73.111";
uint16_t udp_port = 10000;
UdpComm::UdpSender udp_Sender(ip_address, udp_port);

std::mutex g_stateMutex;
std::condition_variable g_stateCV;
std::atomic<bool> g_shutdown(false);

// 推流环形缓冲区及同步原语
constexpr size_t kPacketBufferSize = 5;

LatestRingBuffer<UdpDataPacket, kPacketBufferSize> g_udpRing;
std::mutex g_udpMutex;
std::condition_variable g_udpCV;

LatestRingBuffer<PcieDataPacket, kPacketBufferSize> g_pcieRing;
std::mutex g_pcieMutex;
std::condition_variable g_pcieCV;

// 系统参数
UartComm::SystemConfig g_sysConfig;
UartComm::MotionData g_motionData;
UartComm::HistConfig g_histConfig;

void register_threads()
{
    // 时序初始化
    // int status = ApdControl(1, 2000, 40, 19600, 200, 0, 3200, 1, 3, 1600);
    int status = ApdControl(1, 2000, 40, 37592, 200, 0, 3200, 2, 3, 1600);
    
    if (status != 0) {
        Logger::instance().error("Failed to initialize APD time sequence");
        return ;
    }

    Pcie_Init(0, 0);

    Logger::instance().info("Registering and starting threads");
    
    std::string coolerDevicePath = "/dev/ttyS2"; 
    Cooler::initCooler(coolerDevicePath, 4800);

    std::string devicePath = "/dev/ttyS3";
    std::thread uartComm(PreprocessUart::thread_Preprocess_Communication, 
                         devicePath, 
                         115200, 
                         200);
                         
    uartComm.detach();
    
    std::thread udpSend(UdpComm::thread_UdpSend);
    udpSend.detach();

    std::thread depthProcess(PointCloud::thread_PointCloudProcess);
    depthProcess.detach();

    std::thread tofProcess(TofProcesser::thread_ComputeDistance);
    tofProcess.detach();


    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(1));
}