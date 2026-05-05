/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-01-27 09:00:23
 * @LastEditors: Do not edit
 * @LastEditTime: 2026-05-05 23:31:27
 * @FilePath: \GMAPD_UART\main.cpp
 * @Description: 程序入口，负责系统初始化及log系统初始化
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fstream>
#include <time.h>

#include "task_reg.h"
#include "log.h"
#include "pwProcess.h"
#include "apd_control.h"
#include "apd_voltage_table.h"
#include "cooler_control.h"
#include "util.h"
using namespace std;

int main()
{   
    std::string g_outputDir = "/root/log/"; // 输出目录

    // 初始化 Logger
    Logger::init(g_outputDir);
    Logger::instance().info("Program started");
    Logger::instance().info(("Output directory initialized: " + g_outputDir).c_str());
    Logger::instance().info("System initialized");

    // 预处理模块初始化
    img::imgInit();

    // 电压初始化
    int status = InitVoltageCtrl();
    g_sysConfig.biasVoltage = 10.0f;
    
    if (status != 0) {
        Logger::instance().error("Failed to initialize board power control");
        return 1;
    }

    // APD时序初始化
    status = ApdControl(1, 2000, 40, 19600, 200, 0, 1600, 40, 4, 1600);
    // status = ApdControl(1, 2000, 40, 19600, 200, 0, 1600, 1, 3, 2688);
    // status = ApdControl(1, 2000, 40, 37592, 200, 0, 3200, 2, 3, 1600);
    
    if (status != 0) {
        Logger::instance().error("Failed to initialize APD time sequence");
        return 1;
    }

    Pcie_Init(0, 0);
    Mipi_Init(0);
    initTemperatureSensors();

// --- 互动测试：手动输入电压值，获取 SPI 和 Level ---
#if 0
    // ApdVoltageConfig::interactiveVoltageTest();
    std::string coolerDevicePath = "/dev/ttyS2"; 
    Cooler::initCooler(coolerDevicePath, 4800);
    Cooler::runInteractiveTest();
#endif

// ----------------------------------------------------
#if 1
    try {
        // 注册并启动线程
        register_threads();
        Logger::instance().info("Program completed successfully");

    } catch (const std::exception& e) {
        Logger::instance().error("Program failed with exception");
        Logger::instance().error(e.what());
        return 1;
    }
#endif

    return 0;
}
