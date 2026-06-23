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
#include "peripheryApi.h"

#include "log.h"

namespace PreprocessUart {

    namespace {
        std::string byteToHex(uint8_t value) {
            std::ostringstream oss;
            oss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(value);
            return oss.str();
        }

        // 完备状态切换状态机
        enum class CompletenessState {
            IDLE,

            COOLING,
            COOLING_WAIT,
            DETECTOR,
            DETECTOR_WAIT,
            COMPLETE,

            DETECTOR_SHUTDOWN,
            DETECTOR_SHUTDOWN_WAIT,
            COOLING_SHUTDOWN,
            COOLING_SHUTDOWN_WAIT,
            INCOMPLETE
        };
        // 电压状态切换状态机
        enum class ApdPowerState {
            IDLE,
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
            SHUTDOWN_50V,
            SHUTDOWN_50V_WAIT,
            SHUTDOWN_30V,
            SHUTDOWN_30V_WAIT,
            SHUTDOWN_10V,
            SHUTDOWN_10V_WAIT,
            SHUTDOWN_10V_FINAL_WAIT,
            SHUTDOWN_SUCCESS
        };

        CompletenessState currentCompletenessState = CompletenessState::IDLE;
        ApdPowerState currentApdState = ApdPowerState::IDLE;
        auto lastApdStateTime = std::chrono::steady_clock::now();
        std::mutex g_delayMutex;

        // 通信状态缓存
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
            float temp_value = 0;
            uint8_t received_complete_cmd = 0;
            uint8_t complete_status = 1; // 默认不完备
            bool manual_delay_applied = false;

        };

        PreprocessStateCache g_stateCache;

        uint8_t g_apd_bias_status = 0;
        uint8_t g_ctl_para_status = 0;
        uint8_t g_algo_para_status = 0;
        uint8_t g_power_status = 0;

        uint8_t generateVersionNumber(uint8_t major, uint8_t minor) {
            return (major << 4) | (minor & 0x0F);
        }

        void setHighVoltageStep(float voltage) {
            uint16_t spiValue  = 0;
            uint8_t  levelValue = 0;
            float matched = ApdVoltageConfig::getSpiAndLevelByVoltage(voltage, spiValue, levelValue);
            std::ostringstream msg;
            msg << "PreprocessUart - APD偏压, 请求=" << voltage
                << "V 匹配=" << matched << "V SPI=" << spiValue
                << " Level=" << static_cast<int>(levelValue);
            Logger::instance().debug(msg.str().c_str());
            HighVoltageCtrl(spiValue, levelValue);
        }

        uint8_t AlgoParaSetting(uint8_t algo_frame_denoise, uint8_t algo_stride_diff, uint8_t algo_kernal) {
            int status = 0;
            uint8_t constructFrame = algo_frame_denoise & 0x0F; // 低4位
            uint8_t denoiseLevel = (algo_frame_denoise >> 4) & 0x0F; // 高4位

            uint8_t strideLength = algo_stride_diff & 0x0F; // 低4位
            uint8_t diffThreshold = (algo_stride_diff >> 4) & 0x0F; // 高4位

            uint8_t kernalSize = algo_kernal;

            if (strideLength > 0){
                strideLength = std::min(strideLength, static_cast<uint8_t>(10)); // 限制最大步长为10
            } else {
                strideLength = 3; // 默认值
            }

            if (diffThreshold > 0){
                diffThreshold = std::min(diffThreshold, static_cast<uint8_t>(15)); // 限制最大阈值为15
            } else {
                diffThreshold = 3; // 默认值
            }

            if (constructFrame > 0){
                constructFrame = std::min(constructFrame, static_cast<uint8_t>(15)); // 限制最大帧数为15
            } else {
                constructFrame = 5; // 默认值
            }

            if (denoiseLevel > 0){
                denoiseLevel = std::min(denoiseLevel, static_cast<uint8_t>(15)); // 限制最大降噪等级为15
            } else {
                denoiseLevel = 3; // 默认值
            }
            // 大于0以及值改变则更新
            if (kernalSize > 0){
                kernalSize = std::min(kernalSize, static_cast<uint8_t>(15)); // 限制最大核大小为10
            } else {
                kernalSize = 3; // 默认值
            }

            {
                std::lock_guard<std::mutex> lock(g_histConfig.mtx);
                g_histConfig.stride = strideLength;
                g_histConfig.threshold = diffThreshold;
                g_histConfig.frameNum = constructFrame;
                g_histConfig.denoiseLevel = denoiseLevel;
                g_histConfig.kernalSize = kernalSize;
                g_histConfig.dbscanEps = denoiseLevel;
                g_histConfig.dbscanMinSamples = constructFrame;
            }

            // 打印算法参数设置结果
            Logger::instance().debug(" PreprocessUart Algorithm Parameters Set");

            Logger::instance().debug((" Kernel Size received: " + std::to_string(kernalSize)).c_str());
            Logger::instance().debug((" Kernel Size setting: " + std::to_string(kernalSize)).c_str());
            Logger::instance().debug((" Eps received: " + std::to_string(denoiseLevel)).c_str());
            Logger::instance().debug((" Eps setting: " + std::to_string(denoiseLevel)).c_str());
            Logger::instance().debug((" Min Samples received: " + std::to_string(constructFrame)).c_str());
            Logger::instance().debug((" Min Samples setting: " + std::to_string(constructFrame)).c_str());

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

        bool applyDelayForDistance(float distance) {
            constexpr float kGateAdvanceDistance = 300.0f;
            const float gateDistance = std::max(0.0f, distance - kGateAdvanceDistance);
            const int delay = ComputeDelay(gateDistance, 2, 1000) / 5;

            Logger::instance().info(("PreprocessUart Setting delay, TargetDistance=" + std::to_string(distance) + "m, GateDistance=" +
                    std::to_string(gateDistance) + "m, RegisterDelay=" + std::to_string(delay)).c_str());

            const int enResult = EnDelayCtrl(delay);
            const int recResult = RecDelayCtrl(delay + 1);
            if (enResult != 0 || recResult != 0) {
                Logger::instance().error("PreprocessUart Failed to set APD gate delay");
                return false;
            }

            g_sysConfig.enDelay.store(delay * 5); // 转换回实际延迟值
            return true;
        }

        void updateDistanceWindow(float distance) {
            std::lock_guard<std::mutex> lock(g_histConfig.mtx);
            g_histConfig.distance = distance;
            g_histConfig.maxDistance = distance + 250.0f;
            g_histConfig.minDistance = (distance < 250.0f) ? 0.0f : distance - 250.0f;
        }

        uint8_t ApdStateSetting(uint8_t ctl_para) {
            uint8_t triggerMode = ctl_para & 0x01; // 低1位
            uint8_t testPoint = (ctl_para >> 1) & 0x01; // 第2位
            uint8_t outputMode = (ctl_para >> 2) & 0x01; // 第3位：输出模式

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
                g_sysConfig.workMode = UartComm::WorkMode::STANDARD;
            } else {
                // 输出tof
                g_sysConfig.workMode = UartComm::WorkMode::TEST;
            }

            // 打印算法参数设置结果
            Logger::instance().debug(" ApdStateSetting Algorithm Parameters Set");
            Logger::instance().debug((" Trigger mode: " + std::to_string(static_cast<int>(g_sysConfig.triggerMode))).c_str());
            Logger::instance().debug((" Work Mode: " + std::to_string(static_cast<int>(g_sysConfig.workMode))).c_str());

            return 0;
        }

        void processCompletenessStateMachine() {
            switch (currentCompletenessState) {
                case CompletenessState::COOLING:
                    if (Cooler::startCooler() == 1) {
                        // g_power_status &= ~0x01; // D0=0 成功
                        currentCompletenessState = CompletenessState::COOLING_WAIT;
                        Logger::instance().debug("完备状态切换状态机- 制冷机上电完成，进入等待状态");
                    } else {
                        currentCompletenessState = CompletenessState::COOLING;
                    }

                    break;

                case CompletenessState::COOLING_WAIT:
                    // 等待制冷机温度达到要求
                    Logger::instance().info(("完备状态切换状态机- 等待制冷机温度达到要求 - 当前温度"+ std::to_string(g_stateCache.temp_value/10.0f) + "K, 目标温度"+ std::to_string(g_stateCache.temp_ctl) + "K").c_str());

                    if(g_stateCache.temp_value <= (g_stateCache.temp_ctl*10)) {
                        currentCompletenessState = CompletenessState::DETECTOR;
                        Logger::instance().debug("完备状态切换状态机- 制冷机温度达到要求 - 进入探测器上电状态");
                    }
                    break;

                case CompletenessState::DETECTOR:
                    // 启动apd上电状态机，更新回令中的探测器状态
                    Logger::instance().debug("完备状态切换状态机- 启动探测器上电状态机");

                    {
                        currentApdState = ApdPowerState::STARTING_1V8_5V;
                        lastApdStateTime = std::chrono::steady_clock::now();
                        // g_power_status &= ~0x02; // 0 APD上电
                    }

                    currentCompletenessState = CompletenessState::DETECTOR_WAIT;
                    Logger::instance().debug("完备状态切换状态机- 进入探测器上电等待状态");
                    break;

                case CompletenessState::DETECTOR_WAIT:

                    Logger::instance().debug(("完备状态切换状态机- 等待探测器上电完成 - 当前APD状态" + std::to_string(static_cast<int>(currentApdState))).c_str());
                    if (currentApdState == ApdPowerState::STARTING_SUCCESS) {
                        currentCompletenessState = CompletenessState::COMPLETE;
                        Logger::instance().debug("完备状态切换状态机- 探测器上电完成 - 完备状态切换完成");
                    }

                    break;

                case CompletenessState::DETECTOR_SHUTDOWN:
                    Logger::instance().debug("完备状态切换状态机- 启动探测器断电状态机");

                    {
                        currentApdState = ApdPowerState::SHUTDOWN_50V;
                        lastApdStateTime = std::chrono::steady_clock::now();
                        currentCompletenessState = CompletenessState::DETECTOR_SHUTDOWN_WAIT;
                        // g_power_status |= 0x02; // 1 APD断电
                    }

                    break;
                case CompletenessState::DETECTOR_SHUTDOWN_WAIT:
                    if (currentApdState == ApdPowerState::SHUTDOWN_SUCCESS) {
                        currentCompletenessState = CompletenessState::COOLING_SHUTDOWN;
                    }
                    break;
                case CompletenessState::COOLING_SHUTDOWN:
                    Logger::instance().debug("完备状态切换状态机- 启动制冷机断电状态机");
                    if (Cooler::stopCooler() == 1) {
                        // g_power_status &= ~0x01; // D0=0 成功
                        currentCompletenessState = CompletenessState::COOLING_SHUTDOWN_WAIT;
                    } else {
                        currentCompletenessState = CompletenessState::COOLING_SHUTDOWN;
                    }

                    break;
                case CompletenessState::COOLING_SHUTDOWN_WAIT:
                    static auto coolingShutdownStartTime = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - coolingShutdownStartTime).count() >= 2) {
                        currentCompletenessState = CompletenessState::INCOMPLETE;
                    }
                    break;
            }

            g_stateCache.complete_status = (currentCompletenessState == CompletenessState::COMPLETE) ? 1 : 0;
            Logger::instance().debug(("完备状态切换状态机- 当前状态: " + std::to_string(static_cast<int>(currentCompletenessState)) + " 完备状态: " + std::to_string(g_stateCache.complete_status)).c_str());
        }

        void processApdStateMachine() {
            auto now = std::chrono::steady_clock::now();
            int elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastApdStateTime).count();

            switch (currentApdState) {
                case ApdPowerState::IDLE:
                case ApdPowerState::STARTING_SUCCESS:
                case ApdPowerState::SHUTDOWN_SUCCESS:
                    break;
                case ApdPowerState::STARTING_1V8_5V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("APD上电状态机 - 阶段 1V8_5V");

                        SecondVoltageCtrl();

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_1V8_5V_WAIT;
                    }
                    break;
                case ApdPowerState::STARTING_1V8_5V_WAIT:
                    if (elapsedMs >= 2000) { // 2s 延时，确保电压稳定
                        Logger::instance().info("APD上电状态机 - 阶段 1V8_5V 等待完成");
                        ApdGatherEn(1);

                        ApdStateSetting(0); // 下发默认参数，开始输出图像
                        g_stateCache.ctl_para = 0;

                        PcieChlCtrl(1);

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_30V;
                    }
                    break;
                case ApdPowerState::STARTING_30V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("APD上电状态机 - 阶段 30V");

                        setHighVoltageStep(30.0f);

                        lastApdStateTime = now;
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
                        Logger::instance().info("APD上电状态机 - 阶段 50V");

                        setHighVoltageStep(50.0f);

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_50V_WAIT;
                    }
                    break;
                case ApdPowerState::STARTING_50V_WAIT:
                    if (elapsedMs >= 1000) { // 1s 延时
                        g_sysConfig.biasVoltage = 50.0f;
                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_55V;
                    }
                    break;
                case ApdPowerState::STARTING_55V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("APD上电状态机 - 阶段 55V");

                        setHighVoltageStep(55.0f);

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_55V_WAIT;
                    }
                    break;
                case ApdPowerState::STARTING_55V_WAIT:
                    if (elapsedMs >= 1000) { // 1s 延时
                        g_sysConfig.biasVoltage = 55.0f;
                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_58V;
                    }
                    break;
                case ApdPowerState::STARTING_58V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("APD上电状态机 - 阶段 58V");

                        setHighVoltageStep(58.0f);

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_58V_WAIT;
                    }
                    break;
                case ApdPowerState::STARTING_58V_WAIT:
                    if (elapsedMs >= 1000) { // 1s 延时
                        g_sysConfig.biasVoltage = 58.0f;
                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::STARTING_SUCCESS;
                    }
                    break;

                case ApdPowerState::SHUTDOWN_50V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("APD下电状态机 - 阶段 50V");

                        setHighVoltageStep(50.0f);

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::SHUTDOWN_50V_WAIT;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_50V_WAIT:
                    if (elapsedMs >= 1000) {
                        g_sysConfig.biasVoltage = 50.0f;
                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::SHUTDOWN_30V;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_30V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("APD下电状态机 - 阶段 30V");

                        setHighVoltageStep(30.0f);

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::SHUTDOWN_30V_WAIT;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_30V_WAIT:
                    if (elapsedMs >= 1000) {
                        g_sysConfig.biasVoltage = 30.0f;
                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::SHUTDOWN_10V;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_10V:
                    if (elapsedMs >= 100) {
                        Logger::instance().info("APD下电状态机 - 阶段 10V");

                        setHighVoltageStep(10.0f);

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::SHUTDOWN_10V_WAIT;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_10V_WAIT:
                    if (elapsedMs >= 1000) {

                        g_sysConfig.biasVoltage = 10.0f;
                        ApdGatherEn(0);

                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::SHUTDOWN_10V_FINAL_WAIT;
                    }
                    break;
                case ApdPowerState::SHUTDOWN_10V_FINAL_WAIT:
                    if (elapsedMs >= 1000) {
                        ShutdownVoltageCtrl();
                        lastApdStateTime = now;
                        currentApdState = ApdPowerState::SHUTDOWN_SUCCESS;
                    }
                    break;
            }
        }

        void handleFullStateFrame(const CommandFrame &cmd) {

            Logger::instance().info(("PreprocessUart - 跟踪状态：" + std::to_string(cmd.tracking_status)).c_str());

            const bool requestedTracking = cmd.tracking_status == 1;
            bool trackingChanged = false;

            if (cmd.tracking_status == 0){
                // 非跟踪状态
                if (!requestedTracking && (trackingChanged || !g_stateCache.manual_delay_applied || cmd.distance != g_stateCache.distance)) {
                    Logger::instance().info("PreprocessUart - 弹目距离发送");
                    if (applyDelayForDistance(static_cast<float>(cmd.distance))) {
                        updateDistanceWindow(static_cast<float>(cmd.distance));
                        g_stateCache.manual_delay_applied = true;
                    } else {
                        g_stateCache.manual_delay_applied = false;
                    }
                }
                g_stateCache.distance = cmd.distance;
            }else if (cmd.tracking_status == 1)
            {
                // 跟踪状态， 不响应弹目距离确定
                std::lock_guard<std::mutex> delayLock(g_delayMutex);
                trackingChanged = requestedTracking != g_trackingEnabled.load();
                if (trackingChanged) {
                    g_trackingEnabled.store(requestedTracking);
                    g_trackingGeneration.fetch_add(1);
                    if (requestedTracking) {
                        g_stateCache.manual_delay_applied = false;
                    }
                    Logger::instance().info(requestedTracking ? "PreprocessUart - Tracking enabled" : "PreprocessUart - Tracking disabled");
                }
            }

            if (cmd.test_mode == 1)  // 测试模式，不响应转完备指令
            {
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
                                Logger::instance().debug("PreprocessUart - 制冷机上电");
                                if (Cooler::startCooler() == 1) {
                                    g_power_status |= 0x01;  // D0=1 上电成功
                                } else {
                                    g_power_status &= ~0x01; // D0=0 上电失败
                                }
                            } else {
                                Logger::instance().debug("PreprocessUart - 制冷机下电");
                                if (Cooler::stopCooler() == 1) {
                                    g_power_status &= ~0x01; // D0=0 下电成功
                                } else {
                                    g_power_status |= 0x01;  // D0=1 下电失败
                                }
                            }
                        }

                        Logger::instance().debug(("PreprocessUart - 制冷机工作状态:" + std::to_string(g_power_status)).c_str());

                        if (apd_power_cmd != last_apd_power_cmd) {
                            if (apd_power_cmd == 1) {
                                Logger::instance().debug("PreprocessUart - 探测器上电状态机");

                                currentApdState = ApdPowerState::STARTING_1V8_5V;
                                lastApdStateTime = std::chrono::steady_clock::now();
                                g_power_status |= 0x02; // 1 APD上电

                            } else {
                                Logger::instance().debug("PreprocessUart - 探测器下电状态机");

                                currentApdState = ApdPowerState::SHUTDOWN_50V;
                                lastApdStateTime = std::chrono::steady_clock::now();
                                g_power_status &= ~0x02; // 0 APD下电
                            }
                        }

                        Logger::instance().debug(("PreprocessUart - 电源工作状态:" + std::to_string(g_power_status)).c_str());

                        g_stateCache.power_on_off = cmd.power_on_off;
                        g_stateCache.power_on_off_count = 0; // 执行后重置计数
                    }
                    }
                else {
                    g_stateCache.power_on_off_count = 0; // 状态如果已经一致则重置计数
                    g_stateCache.last_received_power_on_off = cmd.power_on_off;
                }
            }
            else { // 非测试模式，处理完备指令状态机
                if (cmd.complete_status != g_stateCache.received_complete_cmd)
                {

                    if (cmd.complete_status == 1)
                    {
                        Logger::instance().debug("PreprocessUart - 完备指令状态机");
                        currentCompletenessState = CompletenessState::COOLING;
                    }
                    else
                    {
                        Logger::instance().debug("PreprocessUart - 低功耗状态机");
                        currentCompletenessState = CompletenessState::DETECTOR_SHUTDOWN;
                    }

                    g_stateCache.received_complete_cmd = cmd.complete_status; // 回令中保持当前状态，直到状态机切换完成
                }
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

            // 6. 温度控制 (temp_ctl 字节18)
            if ((cmd.temp_ctl != g_stateCache.temp_ctl) && (cmd.temp_ctl != 0)) {
                // 命令行中 温度控制字和缓存不一致，且温度控制字不为0
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

    // 更新跟踪得到的距离，并根据需要更新延迟寄存器和距离窗口
    bool updateTrackingDistance(float distance, bool updateDelay) {
        if (!std::isfinite(distance) || distance < 0.0f) {
            Logger::instance().error(
                    "PreprocessUart Refused invalid tracking distance");
            return false;
        }
        std::lock_guard<std::mutex> delayLock(g_delayMutex);
        if (!g_trackingEnabled.load()) {
            return false;
        }
        if (updateDelay && !applyDelayForDistance(distance)) {
            return false;
        }
        updateDistanceWindow(distance);

        return true;
    }

    void thread_Uart_Communication(const std::string &devicePath,
                                         uint32_t baudrate,
                                         int periodMs) {
        Logger::instance().info(("PreprocessUart thread started on " + devicePath).c_str());

        serial_t* serial = SerialUtils::open_serial_port(devicePath.c_str(), baudrate, 8, 0, 1);
        if (!serial) {
            Logger::instance().error("PreprocessUart Failed to open serial port");
            return;
        }

        g_stateCache.temp_ctl = Cooler::getTargetTemp() / 10; // 转换回协议中的温度值
        Logger::instance().info(("Cooler target temperature: " + std::to_string(g_stateCache.temp_ctl) + " K").c_str());

        uint32_t txSequence = 1;

        while (1) {
            auto startTime = std::chrono::steady_clock::now();

            // 1. 接收指令帧 (先收以提升响应实时性)
            int readTimeout = std::max(1, periodMs - 10); // 留出一点余裕保证发送和状态机更新
            std::vector<uint8_t> rxData = SerialUtils::read_fixed_length(serial, kFrameSize, readTimeout);

            if (rxData.size() == kFrameSize) {
                std::array<uint8_t, kFrameSize> rxArray;
                std::copy(rxData.begin(), rxData.end(), rxArray.begin());
                Logger::instance().debug(("PreprocessUart Received frame: \n" + frameToHex(rxArray)).c_str());

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
            processCompletenessStateMachine();

            // 3. 发送周期应答，获取电压及温度，根据电压值和读取温敏电阻并解析；
            uint8_t coolerTempLow = 0, coolerTempHigh = 0, voltLow = 0, voltHigh = 0, fpgaTempLow = 0, fpgaTempHigh = 0;
            decodeVoltage(g_sysConfig.biasVoltage, voltLow, voltHigh);

            g_stateCache.temp_value = Cooler::getCoolerTemperature(); // 制冷机温度除以10得到原始温度
            decodeTemperature(g_stateCache.temp_value, coolerTempLow, coolerTempHigh);

            Logger::instance().debug(("PreprocessUart Cooler Temperature: " + std::to_string(static_cast<float>(g_stateCache.temp_value) / 10.0f) + "K").c_str());


            uint16_t fpgaTemp = static_cast<uint16_t>(std::lround(fpga_temp_dev.getAdc() / 100.0f)); // FPGA温度除以1000得到原始温度，此处除以100是为了保持统一的协议，传输数值为原始温度乘以10；
            decodeTemperature(fpgaTemp, fpgaTempLow, fpgaTempHigh);
            Logger::instance().debug(("PreprocessUart fpga Temperature ADC: " + std::to_string(fpgaTemp/10.0f)).c_str());

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
                                               coolerTempLow, coolerTempHigh,
                                               voltLow, voltHigh,
                                               fpgaTempLow, fpgaTempHigh,
                                               g_stateCache.complete_status);

            std::vector<uint8_t> txData(reply.raw.begin(), reply.raw.end());

            Logger::instance().debug(("PreprocessUart Sending reply frame: \n" + frameToHex(reply.raw)).c_str());

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
