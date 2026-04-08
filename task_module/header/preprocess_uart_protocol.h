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

    struct CommandFrame {
        std::array<uint8_t, kFrameSize> raw{};
        uint16_t sequence = 0;          // 5-6 (Idx 4-5)
        uint16_t apd_bias = 0;          // 7-8 (Idx 6-7)
        uint8_t ctl_para = 0;           // 9 (Idx 8)
        uint8_t algo_frame_denoise = 0; // 10 (Idx 9)
        uint8_t algo_stride_diff = 0;   // 11 (Idx 10)
        uint8_t algo_kernal = 0;        // 12 (Idx 11)
        uint8_t power_on_off = 0;       // 13 (Idx 12)
        uint16_t distance = 0;          // 14-15 (Idx 13-14)
        uint16_t velocity = 0;          // 16-17 (Idx 15-16)
        uint16_t temp_ctl = 0;          // 18-19 (Idx 17-18)
    };

    struct ReplyFrame {
        std::array<uint8_t, kFrameSize> raw{};
    };

    uint16_t calcFcs16(const uint8_t *data, std::size_t len);
    uint16_t calcFrameFcs(const std::array<uint8_t, kFrameSize> &frame);
    
    bool decodeCommandFrame(const std::array<uint8_t, kFrameSize> &raw,
                            CommandFrame &out,
                            std::string *reason);
                            
    ReplyFrame buildReplyFrame(uint16_t sequence,
                               uint8_t version,
                               uint8_t apd_bias_status,
                               uint8_t ctl_para_status,
                               uint8_t algo_para_status,
                               uint8_t power_status,
                               uint8_t temp_low,
                               uint8_t temp_high,
                               uint8_t volt_int,
                               uint8_t volt_frac);
    std::string frameToHex(const std::array<uint8_t, kFrameSize> &frame);

    void decodeTemperature(uint16_t temp, uint8_t &low, uint8_t &high);
    float encodeTemperature(uint8_t low, uint8_t high);
    
    void decodeVoltage(float voltage, uint8_t &low, uint8_t &high);
    float encodeVoltage(uint8_t low, uint8_t high);
        
} // namespace PreprocessUart
