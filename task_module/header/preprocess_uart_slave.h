/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-17 19:36:19
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-21 15:52:45
 * @FilePath: /GMAPD_UART/task_module/header/preprocess_uart_slave.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

// 1. C/C++ 标准库
#include <cstdint>
#include <string>

namespace PreprocessUart {

    /**
     * @brief 预处理通信线程入口函数
     * 周期行为：先发送上一周期待回指令（无则0），再等待并解析本周期上行指令。
     *
     * @param devicePath 串口设备路径
     * @param stopFlag 停止标志，用于控制线程退出
     * @param baudrate 波特率，默认 115200
     * @param periodMs 周期时间，默认 20ms
     */
    void thread_Preprocess_Communication(const std::string &devicePath,
                                         uint32_t baudrate = 115200,
                                         int periodMs = 20);

} // namespace PreprocessUart
