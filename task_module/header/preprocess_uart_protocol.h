/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-17 15:50:09
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-27 14:36:37
 * @FilePath: /GMAPD_RK3588/task_module/header/preprocess_uart_protocol.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

// 1. C/C++ 标准库
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace PreprocessUart {

    constexpr std::size_t kFrameSize = 32;
    constexpr uint8_t kFlagByte = 0xBA;
    constexpr uint8_t kAddr = 0x56;
    constexpr uint8_t kType = 0x46;

    struct CommandFrame {
        std::array<uint8_t, kFrameSize> raw{};
        uint32_t sequence = 0;
        uint8_t cmd = 0;
        uint8_t paramLow = 0;
        uint8_t paramHigh = 0;
        uint8_t paramLast = 0;
    };

    struct ReplyFrame {
        std::array<uint8_t, kFrameSize> raw{};
    };

    uint16_t calcFcs16(const uint8_t *data, std::size_t len);
    uint16_t calcFrameFcs(const std::array<uint8_t, kFrameSize> &frame);
    
    bool decodeCommandFrame(const std::array<uint8_t, kFrameSize> &raw,
                            CommandFrame &out,
                            std::string *reason);
    ReplyFrame buildReplyFrame(uint32_t sequence,
                               uint8_t resultCmd,
                               uint8_t resp1,
                               uint8_t resp2,
                               uint8_t resp3,
                               uint8_t tempLow,
                               uint8_t tempHigh,
                               uint8_t voltLow,
                               uint8_t voltHigh);
    std::string frameToHex(const std::array<uint8_t, kFrameSize> &frame);

    void decodeTemperature(uint16_t temp, uint8_t &low, uint8_t &high);
    float encodeTemperature(uint8_t low, uint8_t high);
    
    void decodeVoltage(float voltage, uint8_t &low, uint8_t &high);
    float encodeVoltage(uint8_t low, uint8_t high);
        
} // namespace PreprocessUart
