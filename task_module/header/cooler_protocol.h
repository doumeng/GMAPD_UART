/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-16 14:50:46
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-17 11:18:03
 * @FilePath: /GMAPD_RK3588/task_module/header/cooler_protocol.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#pragma once

// 1. C/C++ 标准库
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Cooler {

    // 协议常量
    constexpr uint8_t START_BYTE = 0xCA;
    constexpr uint8_t END_BYTE   = 0xAC;
    constexpr uint8_t TRANS_BYTE = 0xAA;
    constexpr uint8_t ADDR       = 0x01;

    // 命令字枚举
    enum class CmdType : uint8_t {
        READ_STATUS      = 0x21, // 21
        RET_STATUS       = 0x01, // 1
        WRITE_CTRL       = 0x38, // 38
        RET_CTRL         = 0x18, // 18
        READ_DISP_PARAM  = 0x23, // 23
        RET_DISP_PARAM   = 0x03, // 3
        READ_CFG_PARAM   = 0x24, // 24
        RET_CFG_PARAM    = 0x04, // 4
        WRITE_CFG_PARAM  = 0x34, // 34
        RET_CFG_PARAM_ST = 0x14  // 14
    };

    // 工具函数
    std::vector<uint8_t> escape_data(const std::vector<uint8_t>& data);             // 转义数据
    std::vector<uint8_t> unescape_data(const std::vector<uint8_t>& in_data);        // 反转义数据
    uint8_t calc_checksum(const std::vector<uint8_t>& data);                        // 计算校验和

    // 组装数据帧，包含转义和校验，返回完整的从 CA 到 AC 的帧数据
    std::vector<uint8_t> pack_frame(uint8_t cmd, const std::vector<uint8_t>& params);   

    // 解析数据帧，要求传入完整的从 CA 到 AC 的帧数据
    bool unpack_frame(const std::vector<uint8_t>& raw_frame, uint8_t& out_cmd, std::vector<uint8_t>& out_params);

} // namespace Cooler