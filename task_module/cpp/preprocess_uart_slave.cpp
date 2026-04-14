#include "preprocess_uart_slave.h"

// 1. C++ 标准库
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// 2. 本项目其他头文件
#include "apd_control.h"
#include "apd_voltage_table.h"
#include "cooler_control.h"
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
            STARTING_1V8_5V_WAIT,
            STARTING_30V,
            STARTING_30V_WAIT,
            STARTING_50V,
            STARTING_50V_WAIT,
            STARTING_55V,
            STARTING_55V_WAIT,
            STARTING_58V,
            STARTING_58V_WAIT,
            STARTING_SUCCESS,
            // 0xC8 下电的阶段
            SHUTDOWN_50V,
            SHUTDOWN_50V_WAIT,
            SHUTDOWN_30V,
            SHUTDOWN_30V_WAIT,
            SHUTDOWN_10V,
            SHUTDOWN_10V_WAIT,
            SHUTDOWN_10V_FINAL_WAIT,
            SHUTDOWN_SUCCESS
        };

        ApdPowerState currentApdState = ApdPowerState::IDLE;
        auto lastApdStateTime = std::chrono::steady_clock::now();
        uint8_t gCmdStatus = 0;

        struct CmdResponse {
            uint8_t resp1 = 0;
            uint8_t resp2 = 0;
            uint8_t resp3 = 0;
        };

        struct PreprocessStateCache {
            uint16_t apd_bias = 0;
            uint8_t ctl_para = 0;
            uint8_t algo_frame_denoise = 0;
            uint8_t algo_stride_diff = 0;
            uint8_t algo_kernal = 0;
            uint8_t power_on_off = 0;
            uint8_t last_received_power_on_off = 0xFF;
            int power_on_off_count = 0;
            uint16_t distance = 0;
            uint16_t velocity = 0;
            uint16_t temp_ctl = 0;
        };

        PreprocessStateCache g_stateCache;
        uint8_t g_apd_bias_status = 0;
        uint8_t g_ctl_para_status = 0;
        uint8_t g_algo_para_status = 0;
        uint8_t g_power_status = 0;

        uint8_t generateVersionNumber(uint8_t major, uint8_t minor) {
            return (major << 4) | (minor & 0x0F);
        }

        uint8_t AlgoParaSetting(uint8_t algo_frame_denoise, uint8_t algo_stride_diff, uint8_t algo_kernal) {
            int status = 0;
            uint8_t constructFrame = algo_frame_denoise & 0x0F; // 低4位
            uint8_t denoiseLevel = (algo_frame_denoise >> 4) & 0x0F; // 高4位

            uint8_t strideLength = algo_stride_diff & 0x0F; // 低4位
            uint8_t diffThreshold = (algo_stride_diff >> 4) & 0x0F; // 高4位

            uint8_t kernalSize = algo_kernal;

            g_histConfig.stride = strideLength;
            g_histConfig.threshold = diffThreshold;
            g_histConfig.kernalSize = kernalSize;
            g_histConfig.denoiseLevel = denoiseLevel;
            g_histConfig.frameNum = constructFrame;

            g_histConfig.dbscanEps = denoiseLevel;
            g_histConfig.dbscanMinSamples = constructFrame;

#if 0
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
#endif

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

        uint8_t DelaySetting(uint16_t distance) {
            int delay = ComputeDelay(distance, 2, 1000) / 5;

            Logger::instance().info(("PreprocessUart Setting delay, TargetDistance=" + std::to_string(distance) + "m, ComputedDelay=" + std::to_string(delay) + "ns").c_str());

            EnDelayCtrl(delay);
            RecDelayCtrl(delay + 1);
            {
                g_sysConfig.enDelay = delay * 5; // 转换回实际延迟值
            }
            return 0;
        }

        uint8_t ApdStateSetting(uint8_t ctl_para) {
            uint8_t triggerMode = ctl_para & 0x01; // 低1位
            uint8_t testPoint = (ctl_para >> 1) & 0x01; // 第2位
            uint8_t outputMode = (ctl_para >> 2) & 0x01; // 其余6位

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
                if (CycleCtrl(20000))
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
                g_sysConfig.workMode = UartComm::WorkMode::TEST;
            }

            return 0;
        }

        void processApdStateMachine() {
            auto now = std::chrono::steady_clock::now();
            int elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastApdStateTime).count();

            switch (currentApdState) {
                case ApdPowerState::IDLE:
                case ApdPowerState::STARTING_SUCCESS:
                case ApdPowerState::SHUTDOWN_SUCCESS:
                    break;
                    
                // ---------------- 0xC7 APD START -----------------
                case ApdPowerState::STARTING_1V8_5V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Start: Stage 1V8_5V");

                        SecondVoltageCtrl();

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_1V8_5V_WAIT;
                    }
                    break;
                case ApdPowerState::STARTING_1V8_5V_WAIT:
                    if (elapsedMs >= 10000) { // 10s 延时，确保电压稳定
                        
                        ApdGatherEn(1);
                        
                        ApdStateSetting(0); // 下发默认参数，开始输出图像
                        g_stateCache.ctl_para = 0;

                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::STARTING_30V;
                    }
                    break;
                case ApdPowerState::STARTING_30V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Start: Stage 30V");

                        uint16_t spiValue = 0;
                        uint8_t levelValue = 0;
                        float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(30, spiValue, levelValue);
                        
                        std::ostringstream msg;
                        msg << "PreprocessUart CMD - APD偏压设置, 请求电压=" << 30
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        HighVoltageCtrl(spiValue, levelValue); 
                                   
                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::STARTING_30V_WAIT;
                    }
                    break;
                case ApdPowerState::STARTING_30V_WAIT:
                    if (elapsedMs >= 1000) { // 1s 延时
                        g_sysConfig.biasVoltage = 30.0f;
                        lastApdStateTime = std::chrono::steady_clock::now();
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
                        msg << "PreprocessUart CMD - APD偏压设置, 请求电压=" << 50
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        HighVoltageCtrl(spiValue, levelValue);

                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::STARTING_50V_WAIT;
                    }
                    break;
                case ApdPowerState::STARTING_50V_WAIT:
                    if (elapsedMs >= 1000) { // 1s 延时
                        g_sysConfig.biasVoltage = 50.0f;
                        lastApdStateTime = std::chrono::steady_clock::now();
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
                        msg << "PreprocessUart CMD - APD偏压设置, 请求电压=" << 55
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        HighVoltageCtrl(spiValue, levelValue);

                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::STARTING_55V_WAIT;
                    }
                    break;
                case ApdPowerState::STARTING_55V_WAIT:
                    if (elapsedMs >= 1000) { // 1s 延时
                        g_sysConfig.biasVoltage = 55.0f;
                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::STARTING_58V;
                    }
                    break;
                case ApdPowerState::STARTING_58V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("PreprocessUart APD Start: Stage 58V");

                        uint16_t spiValue = 0;
                        uint8_t levelValue = 0;
                        float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(58, spiValue, levelValue);
                        
                        std::ostringstream msg;
                        msg << "PreprocessUart CMD - APD偏压设置, 请求电压=" << 58
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        HighVoltageCtrl(spiValue, levelValue);

                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::STARTING_58V_WAIT;
                    }
                    break;
                case ApdPowerState::STARTING_58V_WAIT:
                    if (elapsedMs >= 1000) { // 1s 延时
                        g_sysConfig.biasVoltage = 58.0f;
                        lastApdStateTime = std::chrono::steady_clock::now();
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
                        msg << "PreprocessUart CMD - APD偏压设置, 请求电压=" << 50
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        HighVoltageCtrl(spiValue, levelValue);

                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::SHUTDOWN_50V_WAIT;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_50V_WAIT:
                    if (elapsedMs >= 1000) {
                        g_sysConfig.biasVoltage = 50.0f;
                        lastApdStateTime = std::chrono::steady_clock::now();
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
                        msg << "PreprocessUart CMD - APD偏压设置, 请求电压=" << 30
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        HighVoltageCtrl(spiValue, levelValue);

                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::SHUTDOWN_30V_WAIT;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_30V_WAIT:
                    if (elapsedMs >= 1000) {
                        g_sysConfig.biasVoltage = 30.0f;
                        lastApdStateTime = std::chrono::steady_clock::now();
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
                        msg << "PreprocessUart CMD - APD偏压设置, 请求电压=" << 10
                            << "V, 匹配表电压=" << matchedVol << "V, SPI值=" << spiValue 
                            << ", Level=" << static_cast<int>(levelValue);
                        Logger::instance().info(msg.str().c_str());

                        HighVoltageCtrl(spiValue, levelValue);

                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::SHUTDOWN_10V_WAIT;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_10V_WAIT:
                    if (elapsedMs >= 1000) {
                        g_sysConfig.biasVoltage = 10.0f;
                        ApdGatherEn(0);
                        
                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentApdState = ApdPowerState::SHUTDOWN_10V_FINAL_WAIT;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_10V_FINAL_WAIT:
                    if (elapsedMs >= 1000) {
                        ShutdownVoltageCtrl();

                        currentApdState = ApdPowerState::SHUTDOWN_SUCCESS;
                    }
                    break;
            }
        }
        
        void handleFullStateFrame(const CommandFrame &cmd) {
            // 1. APD上下电与制冷机控制 (power_on_off 字节13)  D0: 制冷机(1开/0关), D1: APD探测器(1下电/0上电)
            if (cmd.power_on_off != g_stateCache.power_on_off) {
                if (cmd.power_on_off == g_stateCache.last_received_power_on_off) {
                    g_stateCache.power_on_off_count++;
                } else {
                    g_stateCache.last_received_power_on_off = cmd.power_on_off;
                    g_stateCache.power_on_off_count = 1;
                }

                if (g_stateCache.power_on_off_count >= 1) {
                    uint8_t cooler_cmd = cmd.power_on_off & 0x01;
                    uint8_t apd_power_cmd = (cmd.power_on_off >> 1) & 0x01;
                    
                    uint8_t last_cooler_cmd = g_stateCache.power_on_off == 0xFF ? 0xFF : (g_stateCache.power_on_off & 0x01);
                    uint8_t last_apd_power_cmd = g_stateCache.power_on_off == 0xFF ? 0xFF : ((g_stateCache.power_on_off >> 1) & 0x01);

                    if (cooler_cmd != last_cooler_cmd) {
                        if (cooler_cmd == 1) {
                            Logger::instance().info("PreprocessUart - 制冷机上电");
                            if (Cooler::startCooler() == 0) {
                                g_power_status &= ~0x01; // D0=0 成功
                            } else {
                                g_power_status |= 0x01;  // D0=1 失败
                            }
                        } else {
                            Logger::instance().info("PreprocessUart - 制冷机下电");
                            if (Cooler::stopCooler() == 0) {
                                g_power_status &= ~0x01; // D0=0 成功
                            } else {
                                g_power_status |= 0x01;  // D0=1 失败
                            }
                        }
                    }

                    if (apd_power_cmd != last_apd_power_cmd) {
                        if (apd_power_cmd == 1) {
                            Logger::instance().info("PreprocessUart - 探测器上电状态机");

                            currentApdState = ApdPowerState::STARTING_1V8_5V;
                            lastApdStateTime = std::chrono::steady_clock::now();
                            g_power_status &= ~0x02; // 0 APD上电
                        } else {
                            Logger::instance().info("PreprocessUart - 探测器下电状态机");
                            currentApdState = ApdPowerState::SHUTDOWN_50V;
                            lastApdStateTime = std::chrono::steady_clock::now();
                            g_power_status |= 0x02; // 1 APD下电
                        }
                    }
                    
                    g_stateCache.power_on_off = cmd.power_on_off;
                    g_stateCache.power_on_off_count = 0; // 执行后重置计数
                }
            } else {
                g_stateCache.power_on_off_count = 0; // 状态如果已经一致则重置计数
                g_stateCache.last_received_power_on_off = cmd.power_on_off;
            }

            // 2. APD 偏压状态 (apd_bias 字节7-8)
            if (cmd.apd_bias != g_stateCache.apd_bias) {
                float targetVoltage = static_cast<float>(cmd.apd_bias & 0xFF) + (static_cast<float>(cmd.apd_bias >> 8) / 10.0f);
                float currentVoltage = g_sysConfig.biasVoltage;
                
                if (std::abs(targetVoltage - currentVoltage) <= 13.0f) {
                    uint16_t spiValue = 0;
                    uint8_t levelValue = 0;
                    float matchedVol = ApdVoltageConfig::getSpiAndLevelByVoltage(targetVoltage, spiValue, levelValue);
                    
                    Logger::instance().info(("PreprocessUart - APD偏压设置, 请求=" + std::to_string(targetVoltage) + "V").c_str());
                    
                    if (HighVoltageCtrl(spiValue, levelValue) == 0) {
                        g_apd_bias_status = 0; // 0x0 成功
                    } else {
                        g_apd_bias_status = 1; // 0x1 失败
                    }
                    g_sysConfig.biasVoltage = targetVoltage;
                    g_stateCache.apd_bias = cmd.apd_bias;
                } else {
                    Logger::instance().error(("PreprocessUart - APD偏压突变过大请求被丢弃: 当前=" + std::to_string(currentVoltage) + "V, 请求=" + std::to_string(targetVoltage) + "V").c_str());
                    g_apd_bias_status = 1; // 0x1 失败
                }
            }

            // 3. APD控制参数 (ctl_para 字节9)
            if (cmd.ctl_para != g_stateCache.ctl_para) {
                Logger::instance().info("PreprocessUart - APD控制参数设置");
                
                g_ctl_para_status = ApdStateSetting(cmd.ctl_para); 
                g_stateCache.ctl_para = cmd.ctl_para;
            }

            // 4. 算法参数 (字节10-12)
            if (cmd.algo_frame_denoise != g_stateCache.algo_frame_denoise || 
                cmd.algo_stride_diff != g_stateCache.algo_stride_diff || 
                cmd.algo_kernal != g_stateCache.algo_kernal) {
                
                Logger::instance().info("PreprocessUart - 算法参数配置");
                g_algo_para_status = AlgoParaSetting(cmd.algo_frame_denoise, cmd.algo_stride_diff, cmd.algo_kernal);

                g_stateCache.algo_frame_denoise = cmd.algo_frame_denoise;
                g_stateCache.algo_stride_diff = cmd.algo_stride_diff;
                g_stateCache.algo_kernal = cmd.algo_kernal;
            }

            // 5. 延迟设置 (弹目距离 字节14-15)
            if (cmd.distance != g_stateCache.distance) {
                Logger::instance().info("PreprocessUart - 弹目距离发送");
                DelaySetting(cmd.distance);
                g_stateCache.distance = cmd.distance;
            }

            // 6. 温度控制 (temp_ctl 字节18)
            if (cmd.temp_ctl != g_stateCache.temp_ctl) {
                Logger::instance().info(("PreprocessUart - 温度控制，目标=" + std::to_string(cmd.temp_ctl)).c_str());
                uint16_t tempRaw = static_cast<uint16_t>(cmd.temp_ctl) * 10; // Cooler期待K为单位
                if (Cooler::setTargetTemp(tempRaw) != 0) {
                    usleep(50000);
                    Cooler::saveConfig();
                }
                g_stateCache.temp_ctl = cmd.temp_ctl;
            }
        }
    } // namespace

    void thread_Uart_Communication(const std::string &devicePath,
                                         uint32_t baudrate,
                                         int periodMs) {
        Logger::instance().info(("PreprocessUart thread started on " + devicePath).c_str());

        serial_t* serial = SerialUtils::open_serial_port(devicePath.c_str(), baudrate, 8, 0, 1);
        if (!serial) {
            Logger::instance().error("PreprocessUart Failed to open serial port");
            return;
        }

        uint32_t txSequence = 1;

        while (1) {
            auto startTime = std::chrono::steady_clock::now();

            // 1. 接收指令帧 (先收以提升响应实时性)
            int readTimeout = std::max(1, periodMs - 10); // 留出一点余裕保证发送和状态机更新
            std::vector<uint8_t> rxData = SerialUtils::read_fixed_length(serial, kFrameSize, readTimeout);

            if (rxData.size() == kFrameSize) {
                std::array<uint8_t, kFrameSize> rxArray;
                std::copy(rxData.begin(), rxData.end(), rxArray.begin());
                Logger::instance().debug(("PreprocessUart Received frame: " + frameToHex(rxArray)).c_str());

                CommandFrame cmdFrame;
                std::string reason;
                if (decodeCommandFrame(rxArray, cmdFrame, &reason)) {
                    handleFullStateFrame(cmdFrame);
                } else {
                    Logger::instance().warning(("PreprocessUart Invalid frame: " + reason).c_str());
                }
            }

            // 2. Update state machine
            processApdStateMachine();

            // 3. 发送周期应答，获取电压及温度，根据电压值和读取温敏电阻并解析；

            uint8_t tempLow = 0, tempHigh = 0, voltLow = 0, voltHigh = 0;

            decodeVoltage(g_sysConfig.biasVoltage, voltLow, voltHigh);

            uint16_t tempValue = Cooler::getCoolerTemperature();
            decodeTemperature(tempValue, tempLow, tempHigh);
            Logger::instance().debug(("PreprocessUart Cooler Temperature: " + std::to_string(static_cast<float>(tempValue) / 10.0f) + "K").c_str());
            
            // 版本号固定下发：高四位大版本，低四位小版本
            uint8_t major_version = 1;
            uint8_t minor_version = 0;
            uint8_t version = generateVersionNumber(major_version, minor_version);

            uint8_t current_power_status = g_power_status;
            
            if (currentApdState >= ApdPowerState::STARTING_1V8_5V && currentApdState <= ApdPowerState::STARTING_58V) {
                current_power_status &= ~0x02; // 启动中对应下电标记清除
            }

            ReplyFrame reply = buildReplyFrame(txSequence++, 
                                               version, 
                                               g_apd_bias_status, 
                                               g_ctl_para_status, 
                                               g_algo_para_status, 
                                               current_power_status, 
                                               tempLow, tempHigh, 
                                               voltLow, voltHigh);
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
