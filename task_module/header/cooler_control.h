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

    // 回调函数类型定义
    // success: 操作是否成功
    // result: 操作返回值（对于读取温度，result包含温度值；对于控制命令，result为状态码）
    using CoolerCallback = std::function<void(bool success, uint16_t result)>;

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

    // 异步控制接口（使用回调函数处理耗时操作）
    void getCoolerTemperatureAsync(CoolerCallback callback);    // 异步获取制冷机温度
    void startCoolerAsync(CoolerCallback callback);             // 异步启动制冷机
    void stopCoolerAsync(CoolerCallback callback);              // 异步停止制冷机
    void setTargetTempAsync(uint16_t tempK, CoolerCallback callback);  // 异步设置目标温度

    // 交互式测试
    void runInteractiveTest();

} // namespace Cooler