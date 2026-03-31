#include "preprocess_uart_slave.h"

// 1. C++ 标准库
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// 2. 本项目其他头文件
#include "apd_control.h"
#include "apd_voltage_table.h"
#include "cooler_control.h"
#include <atomic>

std::atomic<uint16_t> g_cachedCoolerTemp{0};
#include "preprocess_uart_protocol.h"
#include "serial_utils.h"
#include "task_reg.h"
#include "util.h"

#include "log.h"

namespace PreprocessUart {

    namespace {
        std::string byteToHex(uint8_t value) {
            std::ostringstream oss;
            oss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(value);
            return oss.str();
        }

        enum class ApdPowerState {
            IDLE,
            // 0xC7 上电的阶段
            STARTING_1V8_5V,
            STARTING_30V,
            STARTING_50V,
            STARTING_55V,
            STARTING_58V,
            STARTING_SUCCESS,
            // 0xC8 下电的阶段
            SHUTDOWN_50V,
            SHUTDOWN_30V,
            SHUTDOWN_10V,
            SHUTDOWN_SUCCESS
        };

        ApdPowerState currentApdState = ApdPowerState::IDLE;
        auto lastApdStateTime = std::chrono::steady_clock::now();
        uint8_t activeMachineCmd = 0;
        uint8_t gCmdStatus = 0;

        struct CmdResponse {
            uint8_t resp1 = 0;
            uint8_t resp2 = 0;
            uint8_t resp3 = 0;
        };

        void processApdStateMachine() {
            auto now = std::chrono::steady_clock::now();
            int elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastApdStateTime).count();

            switch (currentApdState) {
                case ApdPowerState::IDLE:
                case ApdPowerState::STARTING_SUCCESS:
                case ApdPowerState::SHUTDOWN_SUCCESS:
                    activeMachineCmd = 0;
                    break;
                    
                // ---------------- 0xC7 APD START -----------------
                case ApdPowerState::STARTING_1V8_5V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Start: Stage 30V");
                        
                        ApdGatherEn(1);
                        usleep(1000000); // 100ms 延时，确保电压稳定

                        SecondVoltageCtrl();
                        
                        usleep(1000000); // 100ms 延时，确保电压稳定

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_30V;
                    }
                    break;
                case ApdPowerState::STARTING_30V:
                    if (elapsedMs >= 100) {
                        // 调试阶段：跳过 50V, 55V, 58V，直接进入下个必要流程
                        Logger::instance().info("PreprocessUart APD Start: Stage -5V check (Debug Mode, max 30V)");

                        uint16_t spiValue = 0;
                        uint8_t levelValue = 0;
                        float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(30, spiValue, levelValue);
                        
                        std::ostringstream msg;
                        msg << "PreprocessUart CMD 0xC7 - APD偏压设置, 请求电压=" << 30
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        {
                            HighVoltageCtrl(spiValue, levelValue); // 直接设置到 30V，跳过中间阶段
                            usleep(1000000); // 1s 延时，确保电压稳定
                            g_sysConfig.biasVoltage = 30.0f;
                        }
                                   
                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_50V;
                    }
                    break;
                case ApdPowerState::STARTING_50V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Start: Stage 50V");

                        uint16_t spiValue = 0;
                        uint8_t levelValue = 0;
                        float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(50, spiValue, levelValue);
                        
                        std::ostringstream msg;
                        msg << "PreprocessUart CMD 0xC7 - APD偏压设置, 请求电压=" << 50
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        {
                            HighVoltageCtrl(spiValue, levelValue);
                            usleep(1000000); // 1s 延时，确保电压稳定
                            g_sysConfig.biasVoltage = 50.0f;
                        }

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_55V;
                    }
                    break;
                case ApdPowerState::STARTING_55V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Start: Stage 55V");
                        uint16_t spiValue = 0;
                        uint8_t levelValue = 0;
                        float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(55, spiValue, levelValue);
                        
                        std::ostringstream msg;
                        msg << "PreprocessUart CMD 0xC7 - APD偏压设置, 请求电压=" << 55
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        {
                            HighVoltageCtrl(spiValue, levelValue);
                            usleep(1000000); // 1s 延时，确保电压稳定
                            g_sysConfig.biasVoltage = 55.0f;
                        }

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_58V;
                    }
                    break;
                case ApdPowerState::STARTING_58V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Start: Stage 58V check");

                        // uint16_t spiValue = 0;
                        // uint8_t levelValue = 0;
                        // float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(58, spiValue, levelValue);
                        
                        // std::ostringstream msg;
                        // msg << "PreprocessUart CMD 0xC7 - APD偏压设置, 请求电压=" << 58
                        //     << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                        //     << ", Level=" << static_cast<int>(levelValue);
                        // Logger::instance().info(msg.str().c_str());

                        // {
                        //     HighVoltageCtrl(spiValue, levelValue);
                        //     usleep(1000000); // 1s 延时，确保电压稳定
                        //     g_sysConfig.biasVoltage = 58.0f;
                        // }

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_SUCCESS;
                    }
                    break;

                // ---------------- 0xC8 APD SHUTDOWN -----------------
                case ApdPowerState::SHUTDOWN_50V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Shutdown: Stage 50V");

                        uint16_t spiValue = 0;
                        uint8_t levelValue = 0;
                        float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(50, spiValue, levelValue);
                        
                        std::ostringstream msg;
                        msg << "PreprocessUart CMD 0xC8 - APD偏压设置, 请求电压=" << 50
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        {
                            HighVoltageCtrl(spiValue, levelValue);
                            usleep(1000000); // 1s 延时，确保电压稳定
                            g_sysConfig.biasVoltage = 50.0f;
                        }

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::SHUTDOWN_30V;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_30V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Shutdown: Stage 30V");

                        uint16_t spiValue = 0;
                        uint8_t levelValue = 0;
                        float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(30, spiValue, levelValue);
                        
                        std::ostringstream msg;
                        msg << "PreprocessUart CMD 0xC8 - APD偏压设置, 请求电压=" << 30
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        {
                            HighVoltageCtrl(spiValue, levelValue);
                            usleep(1000000); // 1s 延时，确保电压稳定
                            g_sysConfig.biasVoltage = 30.0f;
                        }

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::SHUTDOWN_10V;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_10V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Shutdown Complete");

                        uint16_t spiValue = 0;
                        uint8_t levelValue = 0;
                        float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(10, spiValue, levelValue);
                        
                        std::ostringstream msg;
                        msg << "PreprocessUart CMD 0xC8 - APD偏压设置, 请求电压=" << 10
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        {
                            HighVoltageCtrl(spiValue, levelValue);
                            usleep(1000000); // 1s 延时，确保电压稳定
                            g_sysConfig.biasVoltage = 10.0f;
                        }

                        ApdGatherEn(0);
                        
                        usleep(1000000); // 1s 延时，确保电压稳定

                        ShutdownVoltageCtrl();

                        currentApdState = ApdPowerState::SHUTDOWN_SUCCESS;
                    }
                    break;
            }
        }
        
        // 处理0xC1命令 一个生成版本号的函数，输入年月日，输出一个3个字节的版本号，第一个字节为年份的后两位，第二个字节为月份，第三个字节为日期
        uint32_t generateVersionNumber(int year, int month, int day) {
            uint32_t version = 0;
            version |= (year % 100) << 16; // 年份的后两位放在高字节
            version |= (month & 0xFF) << 8;   // 月份放在中间字节
            version |= (day & 0xFF);         // 日期放在低字节
            return version;
        }

        // 处理0xC4命令，设置APD状态，paramLow的低3位分别表示触发方式、测试点使能和输出数据通道
        uint8_t ApdStateSetting(const CommandFrame &cmd){
            uint8_t triggerMode = cmd.paramLow & 0x01; // 低3位
            uint8_t testPoint = (cmd.paramLow >> 1) & 0x01; // 第2位
            uint8_t outputMode = (cmd.paramLow >> 2) & 0x01; // 其余6位

            if (triggerMode == 0) {
                // 外触发
                if (TriggerModeCtrl(1)){
                    Logger::instance().error("Failed to set APD trigger mode");
                    return 1;
                }
                g_sysConfig.triggerMode = UartComm::TriggerMode::EXTERNAL;

            } else {
                // 内触发
                if (TriggerModeCtrl(0))
                {
                    Logger::instance().error("Failed to set APD trigger mode");
                    return 1;
                }
                if (CycleCtrl(40000))
                {
                    Logger::instance().error("Failed to set APD cycle control");
                    return 1;
                }
                g_sysConfig.triggerMode = UartComm::TriggerMode::INTERNAL;
            }

            if (testPoint == 0) {
                // 测试点关
                if (ApdTestEn(0)){
                    Logger::instance().error("Failed to set test point mode");
                    return 1;
                }
                
            } else {
                //测试点开
                if (ApdTestEn(1)){
                    Logger::instance().error("Failed to set test point mode");
                    return 1;
                }
            }

            if (outputMode == 0) {
                // 输出depth
                if (PcieChlCtrl(1))
                {
                    Logger::instance().error("Failed to set PCIe channel control");
                    return 1;
                }
                g_sysConfig.workMode = UartComm::WorkMode::STANDARD;
            } else {
                // 输出tof
                if (PcieChlCtrl(0))
                {
                    Logger::instance().error("Failed to set PCIe channel control");
                    return 1;
                }
                g_sysConfig.workMode = UartComm::WorkMode::TEST;
            }

            return 0;
        }

        // 处理0xC5命令，设置算法参数，paramLow的低4位表示构建帧数，高4位表示降噪等级；paramHigh的低4位表示步长，高4位表示差分阈值；paramLast表示卷积核大小
        uint8_t AlgoParaSetting(const CommandFrame &cmd){
            int status = 0;
            uint8_t constructFrame = cmd.paramLow & 0x0F; // 低4位
            uint8_t denoiseLevel = (cmd.paramLow >> 4) & 0x0F; // 高4位

            uint8_t strideLength = cmd.paramHigh & 0x0F; // 低4位
            uint8_t diffThreshold = (cmd.paramHigh >> 4) & 0x0F; // 高4位

            uint8_t kernalSize = cmd.paramLast;

            g_histConfig.stride = strideLength;
            g_histConfig.threshold = diffThreshold;
            g_histConfig.kernalSize = kernalSize;
            g_histConfig.denoiseLevel = denoiseLevel;
            g_histConfig.frameNum = constructFrame;
            
            // TODO:调整统计帧数
            {
                ApdGatherEn(0);
                usleep(100000);

                if (ApdConstructFrameCtrl(constructFrame)){
                    Logger::instance().error("Failed to set APD construct frame control");
                    status = 1;
                }
                usleep(100000);
                ApdGatherEn(1);
            }

            if (StrideLengthCtrl(strideLength)){
                Logger::instance().error("Failed to set stride length");
                status = 1;
            }

            if (DiffThreholdCtrl(diffThreshold)){
                Logger::instance().error("Failed to set difference threshold");
                status = 1;
            }

            return status;
        }

        // 处理0xC6命令，设置延迟，paramLow和paramHigh组合成一个16位的延迟值，单位为毫秒
        uint8_t DelaySetting(const CommandFrame &cmd){
            uint16_t distance = static_cast<uint16_t>(cmd.paramLow) |  (static_cast<uint16_t>(cmd.paramHigh) << 8);
            uint16_t speed = static_cast<uint16_t>(cmd.paramLow) |  (static_cast<uint16_t>(cmd.paramHigh) << 8);
            
            int delay = ComputeDelay(distance, 2, 1000);

            Logger::instance().info(("PreprocessUart CMD 0xC6 - Setting delay, TargetDistance=" + std::to_string(distance) + "m, ComputedDelay=" + std::to_string(delay) + "ns").c_str());

            EnDelayCtrl(delay);
            RecDelayCtrl(delay + 1);
            return 0;
        }

        CmdResponse getResponseExecutionResult(uint8_t cmd) {
            CmdResponse res;
            if (cmd == 0xC1) {
                uint32_t version = generateVersionNumber(2026, 3, 19);
                res.resp1 = (version >> 16) & 0xFF; // Year
                res.resp2 = (version >> 8) & 0xFF;  // Month
                res.resp3 = version & 0xFF;         // Day

                // ApdGatherEn(1);

                return res;
            }
            else if (cmd == 0xC2) {
                if (gCmdStatus > 0) {
                    res.resp1 = 0x00; 
                } else {
                    res.resp1 = 0x01; 
                }

                return res; 
            }
            else if (cmd == 0xC3)
            {
                if (gCmdStatus > 0) {
                    res.resp1 = 0x00; // D7=1 执行成功
                } else {
                    res.resp1 = 0x01; // D6=1 执行失败
                }
                return res; 
            }
            else if (cmd == 0xC4)
            {
                if (gCmdStatus == 0) {
                    res.resp1 = 0x00; // D7=1 执行成功
                } else {
                    res.resp1 = 0x01; // D6=1 执行失败
                }

                return res; 
            }
            else if (cmd == 0xC5)
            {
                if (gCmdStatus == 0) {
                    res.resp1 = 0x00; // D7=1 执行成功
                } else {
                    res.resp1 = 0x01; // D6=1 执行失败
                }

                return res; 
            }
            else if (cmd == 0xC6)
            {
                if (gCmdStatus == 0) {
                    res.resp1 = 0x00; // D7=1 执行成功
                } else {
                    res.resp1 = 0x01; // D6=1 执行失败
                }

                return res; 
            }
            else if (cmd == 0xC7) {
                if (currentApdState >= ApdPowerState::STARTING_1V8_5V && currentApdState <= ApdPowerState::STARTING_58V) {
                    res.resp1 = 0x40; // D6=1 上电中
                    return res;
                } else if (currentApdState == ApdPowerState::STARTING_SUCCESS) {
                    // 也可根据硬件实际检查返回 0x20 (失败)
                    res.resp1 = 0x80; // D7=1 上电成功
                    return res;
                }
                return res; 
            } 
            else if (cmd == 0xC8) {
                if (currentApdState >= ApdPowerState::SHUTDOWN_50V && currentApdState <= ApdPowerState::SHUTDOWN_30V) {
                    res.resp1 = 0x40; 
                    return res;
                } else if (currentApdState == ApdPowerState::SHUTDOWN_SUCCESS) {
                    res.resp1 = 0x80; 
                    return res;
                }
                return res;
            }
            else if (cmd == 0xC9) {
                if (gCmdStatus > 0) {
                    res.resp1 = 0x00; 
                } else {
                    res.resp1 = 0x01; 
                }
                return res;
            }
            else if (cmd == 0xCA) {
                if (gCmdStatus == 0) {
                    res.resp1 = 0x00; 
                } else {
                    res.resp1 = 0x01; 
                }
                return res;
            }
            return res; 
        }

        void handleCommandTrigger(const CommandFrame &cmd) {
            switch (cmd.cmd) {
                case 0xC1:
                    Logger::instance().info("PreprocessUart CMD 0xC1 - 组件编号读取");
                    break;
                case 0xC2: 
                {
                    Logger::instance().info("PreprocessUart CMD 0xC2 - 制冷机上电");

                    gCmdStatus = Cooler::startCooler();

                    if (gCmdStatus == 0) {
                        Logger::instance().error("Failed to start cooler!");
                    } else {
                        Logger::instance().info("Cooler start command executed successfully!");
                    }

                    break;
                
                }
                case 0xC3: 
                {
                    const uint16_t tempRaw = static_cast<uint16_t>(cmd.paramLow) |
                                             (static_cast<uint16_t>(cmd.paramHigh) << 8);
                    
                    Cooler::setTargetTempAsync(tempRaw, [](bool success, uint8_t) {
                        if (!success) {
                            Logger::instance().error("Failed to set cooler temperature!");
                            gCmdStatus = 0;
                        } else {
                            Logger::instance().info("Cooler temperature setting command executed successfully!");
                            std::thread([]() {
                                usleep(50000);
                                Cooler::saveConfigAsync([](bool succ2, uint8_t) {
                                    gCmdStatus = succ2 ? 1 : 0;
                                    if (!succ2) Logger::instance().error("Failed to save temperature!");
                                    else Logger::instance().info("Cooler temperature save command executed successfully!");
                                });
                            }).detach();
                        }
                    });

                    std::string msg = "PreprocessUart CMD 0xC3 - 温度控制, 目标温度=" + std::to_string(tempRaw/10.f);
                    Logger::instance().info(msg.c_str());
                    
                    break;
                }
                case 0xC4: 
                {
                    gCmdStatus = ApdStateSetting(cmd);

                    std::string msg = "PreprocessUart CMD 0xC4:"+  std::to_string(cmd.paramLow) + std::string((gCmdStatus == 0) ? " 执行成功" : " 执行失败");
                    Logger::instance().info(msg.c_str());
                    break;
                }
                case 0xC5: 
                {
                    gCmdStatus = AlgoParaSetting(cmd);

                    std::string msg = "PreprocessUart CMD 0xC5 - 算法参数配置, byte12=0x" + byteToHex(cmd.paramLow) +
                                      ", byte13=0x" + byteToHex(cmd.paramHigh);
                    Logger::instance().info(msg.c_str());
                    break;
                }
                case 0xC6: 
                {
                    const uint16_t distance = static_cast<uint16_t>(cmd.paramLow) |  (static_cast<uint16_t>(cmd.paramHigh) << 8);
                    gCmdStatus = DelaySetting(cmd);
                    std::string msg = "PreprocessUart CMD 0xC6 - 弹体信息发送, 距离字段=" + std::to_string(distance);
                    Logger::instance().info(msg.c_str());
                    break;
                }
                case 0xC7:
                {
                    Logger::instance().info("PreprocessUart CMD 0xC7 - APD启动 [State Machine Initiated, Stage 1.8V, 5V]");
                    activeMachineCmd = 0xC7;
                    currentApdState = ApdPowerState::STARTING_1V8_5V;
                    lastApdStateTime = std::chrono::steady_clock::now();
                    break;
                }
                case 0xC8:
                {
                    Logger::instance().info("PreprocessUart CMD 0xC8 - 探测器下电 [State Machine Initiated, Stage -5V]");
                    activeMachineCmd = 0xC8;
                    currentApdState = ApdPowerState::SHUTDOWN_50V;
                    lastApdStateTime = std::chrono::steady_clock::now();
                    break;
                }
                case 0xC9: 
                {
                    Logger::instance().info("PreprocessUart CMD 0xC9 - 制冷机下电");

                    Cooler::stopCoolerAsync([](bool success, uint8_t) {
                        gCmdStatus = success ? 1 : 0;
                        if (!success) Logger::instance().error("Failed to stop cooler!");
                        else Logger::instance().info("Cooler stop command executed successfully!");
                    });
                    
                    break;
                }
                case 0xCA: 
                {
                    // 通讯协议要求：12 字节表示整数位，13 字节表示小数位，例如 60.9V，12 字节表示 60, 13 字节表示 9
                    float targetVoltage = static_cast<float>(cmd.paramLow) + (static_cast<float>(cmd.paramHigh) / 10.0f);
                    
                    uint16_t spiValue = 0;
                    uint8_t levelValue = 0;
                    float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(targetVoltage, spiValue, levelValue);
                    std::string msg = "PreprocessUart CMD 0xCA - APD偏压设置, 请求电压=" + std::to_string(targetVoltage) 
                        + "V, 匹配表电压=" + std::to_string(matchedVol) + "V, SPI值=" + std::to_string(spiValue) 
                        + ", Level=" + std::to_string(static_cast<int>(levelValue));

                    gCmdStatus = HighVoltageCtrl(spiValue, levelValue); // 直接设置到 20V，跳过中间阶段
                    
                    g_sysConfig.biasVoltage = targetVoltage;
                    
                    Logger::instance().info(msg.c_str());
                    
                    break;
                }
                default:
                    Logger::instance().info(("PreprocessUart 收到未知指令: 0x" + byteToHex(cmd.cmd)).c_str());
                    break;
            }
        }
    } // namespace

    void thread_Preprocess_Communication(const std::string &devicePath,
                                         uint32_t baudrate,
                                         int periodMs) {
        Logger::instance().info(("PreprocessUart thread started on " + devicePath).c_str());

        serial_t* serial = SerialUtils::open_serial_port(devicePath.c_str(), baudrate, 8, 0, 1);
        if (!serial) {
            Logger::instance().error("PreprocessUart Failed to open serial port");
            return;
        }

        uint8_t pendingReplyCmd = 0;
        uint32_t txSequence = 1;
        auto last_temp_req = std::chrono::steady_clock::now();
        
        std::atomic<bool> isReading{false};
        std::vector<uint8_t> latestRxData;
        std::mutex rxMutex;

        while (1) {
            auto startTime = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(startTime - last_temp_req).count() > 500) {
                last_temp_req = startTime;
                Cooler::getCoolerTemperatureAsync([](bool success, uint16_t temp) {
                    if (success) g_cachedCoolerTemp.store(temp);
                });
            }

            // 1. 接收指令帧 (异步处理，不阻塞主循环)
            if (!isReading.exchange(true)) {
                // 如果当前没有正在读取，则发起一次异步读取
                SerialUtils::read_fixed_length_async(serial, kFrameSize, periodMs - 2, 
                    [&isReading, &latestRxData, &rxMutex](bool success, const std::vector<uint8_t>& data) {
                        if (success) {
                            std::lock_guard<std::mutex> lock(rxMutex);
                            latestRxData = data;
                        }
                        isReading = false;
                    });
            }

            std::vector<uint8_t> rxData;
            {
                std::lock_guard<std::mutex> lock(rxMutex);
                if (!latestRxData.empty()) {
                    rxData = std::move(latestRxData);
                    latestRxData.clear();
                }
            }

            if (rxData.size() == kFrameSize) {
                std::array<uint8_t, kFrameSize> rxArray;
                std::copy(rxData.begin(), rxData.end(), rxArray.begin());
                Logger::instance().debug(("PreprocessUart Received frame: " + frameToHex(rxArray)).c_str());

                CommandFrame cmdFrame;
                std::string reason;
                if (decodeCommandFrame(rxArray, cmdFrame, &reason)) {
                    handleCommandTrigger(cmdFrame);
                    pendingReplyCmd = cmdFrame.cmd; // 缓存供本阶段及后续应答
                } else {
                    Logger::instance().warning(("PreprocessUart Invalid frame: " + reason).c_str());
                }
            }

            // 2. Update state machine
            processApdStateMachine();

            // 3. 发送周期应答（优先使用正在运行的长耗时状态机指令，如果无则使用最后一次的指令CMD，再无则为0）
            uint8_t cmdToReply = (activeMachineCmd != 0) ? activeMachineCmd : pendingReplyCmd;
            
            CmdResponse execResult;
            if (cmdToReply != 0) {
                execResult = getResponseExecutionResult(cmdToReply);
                Logger::instance().info(("PreprocessUart Preparing reply for CMD 0x" + byteToHex(cmdToReply)).c_str());
            }

            // 获取电压及温度，根据电压值和读取温敏电阻并解析；

            uint8_t tempLow = 0, tempHigh = 0, voltLow = 0, voltHigh = 0;

            decodeVoltage(g_sysConfig.biasVoltage, voltLow, voltHigh);
            uint16_t tempValue = g_cachedCoolerTemp.load();
            decodeTemperature(tempValue, tempLow, tempHigh);

            ReplyFrame reply = buildReplyFrame(txSequence++, cmdToReply, execResult.resp1, execResult.resp2, execResult.resp3, tempLow, tempHigh, voltLow, voltHigh); 
            std::vector<uint8_t> txData(reply.raw.begin(), reply.raw.end());
            
            Logger::instance().debug(("PreprocessUart Sending reply frame: " + frameToHex(reply.raw)).c_str());
           
            SerialUtils::write_frame(serial, txData);

            // 4. 严格同步到周期
            auto endWorkTime = std::chrono::steady_clock::now();
            int workTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endWorkTime - startTime).count();
            int sleepTime = periodMs - workTimeMs;
            if (sleepTime > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
            }
        }

        SerialUtils::close_serial_port(serial);
        Logger::instance().info("PreprocessUart thread stopped");
    }

} // namespace PreprocessUart
