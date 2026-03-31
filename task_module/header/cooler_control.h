/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-16 14:51:17
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-27 15:34:40
 * @FilePath: /GMAPD_RK3588/task_module/header/cooler_control.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

// 1. C/C++ 标准库
#include <cstdint>
#include <mutex>
#include <string>
#include <functional>

namespace Cooler {

    using CoolerCallback = std::function<void(bool success, uint8_t result)>;
    using TemperatureCallback = std::function<void(bool success, uint16_t temperature)>;

    // 外部操作命令枚举
    enum class UserCmd {
        NONE,
        START_COOLER,
        STOP_COOLER,
        CLEAR_ERROR,
        SET_TARGET_TEMP
    };

    // 串口初始化/释放接口
    bool initCooler(const std::string &devicePath, uint32_t baudrate = 4800);
    void deinitCooler();

    // 外部同步控制接口（返回0失败或无响应 返回非0成功）
    uint8_t startCooler();                 // 启动制冷机
    uint8_t stopCooler();                  // 停止制冷机
    uint8_t clearError();                  // 清除错误
    uint8_t saveConfig();
    uint8_t setTargetTemp(uint16_t tempK);    // 设置目标温度，单位K
    uint16_t getCoolerTemperature();          // 独立获取制冷机温度（冷头）
    
    // 异步回调接口
    void startCoolerAsync(CoolerCallback callback);
    void stopCoolerAsync(CoolerCallback callback);
    void clearErrorAsync(CoolerCallback callback);
    void saveConfigAsync(CoolerCallback callback);
    void setTargetTempAsync(uint16_t tempK, CoolerCallback callback);
    void getCoolerTemperatureAsync(TemperatureCallback callback);

    // 交互式测试
    void runInteractiveTest();

} // namespace Cooler