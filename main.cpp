/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-01-27 09:00:23
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-27 15:41:55
 * @FilePath: /GMAPD_RK3588/main.cpp
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

using namespace std;

int main()
{   
    std::string g_outputDir = "/root/log/"; // 输出目录

    // 初始化 Logger
    Logger::init(g_outputDir);
    Logger::instance().info("Program started");
    Logger::instance().info(("Output directory initialized: " + g_outputDir).c_str());
    Logger::instance().info("System initialized");

    img::imgInit();

    // 电压初始化
    int status = InitVoltageCtrl();
    g_sysConfig.biasVoltage = 10.0f;
    
    if (status != 0) {
        Logger::instance().error("Failed to initialize board power control");
        return 1;
    }

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
