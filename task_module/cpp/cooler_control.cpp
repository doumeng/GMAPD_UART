#include "cooler_control.h"

// 1. C 标准库 (C++封装)
#include <cstdio>
#include <cstring>

// 2. C++ 标准库
#include <chrono>
#include <queue>
#include <thread>
#include <vector>
#include <iostream>
#include <cmath>

#include "cooler_protocol.h"
#include "log.h"
#include "serial.h"
#include "serial_utils.h"

namespace Cooler {

    static serial_t* g_serial = nullptr;
    static std::mutex g_serialMutex;

    static uint16_t parse_u16(const std::vector<uint8_t>& data, size_t offset) {
        if (offset + 1 < data.size()) {
            return (static_cast<uint16_t>(data[offset + 1]) << 8) | data[offset];
        }
        return 0;
    }

    static uint32_t parse_u32(const std::vector<uint8_t>& data, size_t offset) {
        if (offset + 3 < data.size()) {
            return (static_cast<uint32_t>(data[offset + 3]) << 24) |
                (static_cast<uint32_t>(data[offset + 2]) << 16) |
                (static_cast<uint32_t>(data[offset + 1]) << 8) |
                data[offset];
        }
        return 0;
    }

    static void push_u16(std::vector<uint8_t>& data, uint16_t val) {
        data.push_back(val & 0xFF);
        data.push_back((val >> 8) & 0xFF);
    }

    bool initCooler(const std::string &devicePath, uint32_t baudrate) {
        std::lock_guard<std::mutex> lock(g_serialMutex);
        if (g_serial) {
            SerialUtils::close_serial_port(g_serial);
            g_serial = nullptr;
        }
        g_serial = SerialUtils::open_serial_port(devicePath.c_str(), baudrate, 8, 0, 1);
        if (!g_serial) {
            Logger::instance().error("Cooler: Failed to open serial port");
            return false;
        }
        Logger::instance().info("Cooler serial initialized");
        return true;
    }

    void deinitCooler() {
        std::lock_guard<std::mutex> lock(g_serialMutex);
        if (g_serial) {
            SerialUtils::close_serial_port(g_serial);
            g_serial = nullptr;
        }
    }

    static uint8_t send_receive_cooler_cmd(uint8_t cmd_code, const std::vector<uint8_t>& params) {
        std::lock_guard<std::mutex> lock(g_serialMutex);
        if (!g_serial) {
            Logger::instance().error("Cooler serial not initialized!");
            return 0;
        }

        auto frame = pack_frame(cmd_code, params);
        SerialUtils::write_frame(g_serial, frame);

        std::string frame_str;
        for (auto b : frame) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", b);
            frame_str += buf;
        }
        Logger::instance().debug(("Sent command to cooler: " + frame_str).c_str());
        
        // 50ms 超时接收
        auto rx_frame = SerialUtils::read_frame_by_boundary(g_serial, START_BYTE, END_BYTE, 50);
        if (rx_frame.empty()) {
            Logger::instance().error("Cooler command timeout!");
            return 0;
        }

        std::string rx_frame_str;
        for (auto b : rx_frame) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", b);
            rx_frame_str += buf;
        }
        Logger::instance().debug(("Received response from cooler: " + rx_frame_str).c_str());
        
        uint8_t rx_cmd_code;
        std::vector<uint8_t> rx_params;
        if (!unpack_frame(rx_frame, rx_cmd_code, rx_params)) {
             Logger::instance().error("Failed to unpack cooler response!");
             return 0;
        }

        return 1;
    }

    uint16_t getCoolerTemperature() {
        std::lock_guard<std::mutex> lock(g_serialMutex);
        if (!g_serial) {
            Logger::instance().error("Cooler serial not initialized!");
            return 0;
        }

        std::vector<uint8_t> params = {0x00, 0x01}; // 索引0(冷头温度), 读取1个参数
        auto frame = pack_frame(static_cast<uint8_t>(CmdType::READ_DISP_PARAM), params);
        SerialUtils::write_frame(g_serial, frame);
        
        std::string frame_str;
        for (auto b : frame) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", b);
            frame_str += buf;
        }
        Logger::instance().debug(("Sent command to cooler: " + frame_str).c_str());
        
        // 50ms 超时接收
        auto rx_frame = SerialUtils::read_frame_by_boundary(g_serial, START_BYTE, END_BYTE, 50);
        if (rx_frame.empty()) {
            Logger::instance().error("Read cooler temperature timeout!");
            return 0;
        }

        uint8_t rx_cmd_code;
        std::vector<uint8_t> rx_params;
        if (!unpack_frame(rx_frame, rx_cmd_code, rx_params)) {
             Logger::instance().error("Failed to unpack cooler temperature response!");
             return 0;
        }

        std::string rx_frame_str;
        for (auto b : rx_frame) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", b);
            rx_frame_str += buf;
        }
        Logger::instance().debug(("Received response from cooler: " + rx_frame_str).c_str());

        if (rx_cmd_code == static_cast<uint8_t>(CmdType::RET_DISP_PARAM)) {
            if (rx_params.size() >= 4 && rx_params[0] == 0x00) {
                uint16_t raw_temp = rx_params[2] | (static_cast<uint16_t>(rx_params[3]) << 8);
                return raw_temp;
            }
        }

        Logger::instance().error("Unexpected response for cooler temperature!");
        return 0;
    }

    uint8_t startCooler() {
        return send_receive_cooler_cmd(static_cast<uint8_t>(CmdType::WRITE_CTRL), {0x01, 0x01});
    }

    uint8_t stopCooler() {
        return send_receive_cooler_cmd(static_cast<uint8_t>(CmdType::WRITE_CTRL), {0x01, 0x00});
    }

    uint8_t saveConfig(){
        return send_receive_cooler_cmd(static_cast<uint8_t>(CmdType::WRITE_CTRL), {0x03, 0x00});
    }

    uint8_t clearError() {
        return send_receive_cooler_cmd(static_cast<uint8_t>(CmdType::WRITE_CTRL), {0x01, 0x02});
    }

    uint8_t setTargetTemp(uint16_t tempK) {
        std::vector<uint8_t> params = {0x01}; 
        uint16_t t_val = static_cast<uint16_t>(tempK);
        push_u16(params, t_val);
        return send_receive_cooler_cmd(static_cast<uint8_t>(CmdType::WRITE_CFG_PARAM), params);
    }

    void runInteractiveTest() {
        Logger::instance().info("\n=== 制冷机交互测试程序 ===");
        bool running = true;
        uint8_t status = 1;
        while (running) {
            Logger::instance().info("\n1. 启动制冷机\n"
                      "2. 停止制冷机\n"
                      "3. 清除错误\n"
                      "4. 设置目标温度\n"
                      "5. 读取当前状态\n"
                      "0. 退出测试\n"
                      "请输入指令 (0-5): ");
            int cmd;
            if (!(std::cin >> cmd)) {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                continue;
            }

            status = 1;

            switch (cmd) {
                case 1:
                    Logger::instance().info("执行: 启动制冷机");
                    status = startCooler();
                    if (status != 0) {
                        Logger::instance().error("Failed to start cooler!");
                    }else{
                        Logger::instance().info("Cooler started successfully!");
                    }
                    break;
                case 2:
                    Logger::instance().info("执行: 停止制冷机");
                    status = stopCooler();
                    if (status != 0) {
                        Logger::instance().error("Failed to stop cooler!");
                    }else{
                        Logger::instance().info("Cooler stopped successfully!");
                    }
                    break;
                case 3:
                    Logger::instance().info("执行: 清除错误");
                    status = clearError();
                    if (status != 0) {
                        Logger::instance().error("Failed to clear cooler error!");
                    }else{
                        Logger::instance().info("Cooler error cleared successfully!");
                    }
                    break;
                case 4: {
                    float tempK;
                    Logger::instance().info("请输入目标温度(K)例如 120: ");
                    std::cin >> tempK;
                    Logger::instance().info(("执行: 设置目标温度 " + std::to_string(tempK) + " K").c_str());
                    status = setTargetTemp(tempK);
                    if (status != 0) {
                        Logger::instance().error("Failed to set target temperature!");
                    }else{
                        Logger::instance().info("Target temperature set successfully!");
                    }
                    break;
                }
                case 5: {
                    uint16_t coolerTemp = getCoolerTemperature();
                    if (coolerTemp == 0) {
                        Logger::instance().error("Failed to read cooler temperature!");
                    } else {
                        Logger::instance().info(("冷头温度: " + std::to_string(coolerTemp) + " K").c_str());
                    }
                    break;
                }
                case 0:
                    Logger::instance().info("退出交互测试...");
                    running = false;
                    break;
                default:
                    Logger::instance().info("无效指令!");
                    break;
                }

            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
} // namespace Cooler