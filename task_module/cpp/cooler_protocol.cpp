/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-16 14:51:02
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-17 11:17:53
 * @FilePath: /GMAPD_RK3588/task_module/cpp/cooler_protocol.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "cooler_protocol.h"

namespace Cooler {

    std::vector<uint8_t> escape_data(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> result;
        result.reserve(data.size());
        for (uint8_t b : data) {
            if (b == START_BYTE) {
                result.push_back(TRANS_BYTE);
                result.push_back(0x35);
            } else if (b == END_BYTE) {
                result.push_back(TRANS_BYTE);
                result.push_back(0x53);
            } else if (b == TRANS_BYTE) {
                result.push_back(TRANS_BYTE);
                result.push_back(0x55);
            } else {
                result.push_back(b);
            }
        }
        return result;
    }

    std::vector<uint8_t> unescape_data(const std::vector<uint8_t>& in_data) {
        std::vector<uint8_t> result;
        result.reserve(in_data.size());
        for (size_t i = 0; i < in_data.size(); ++i) {
            if (in_data[i] == TRANS_BYTE && i + 1 < in_data.size()) {
                uint8_t next = in_data[i + 1];
                if (next == 0x35) result.push_back(START_BYTE);
                else if (next == 0x53) result.push_back(END_BYTE);
                else if (next == 0x55) result.push_back(TRANS_BYTE);
                else {
                    // 不合法转义则直接放入
                    result.push_back(in_data[i]);
                    result.push_back(next);
                }
                i++; // 跳过转义后缀
            } else {
                result.push_back(in_data[i]);
            }
        }
        return result;
    }

    uint8_t calc_checksum(const std::vector<uint8_t>& data) {
        uint32_t sum = 0;
        for (uint8_t b : data) {
            sum += b;
        }
        return static_cast<uint8_t>(~sum);
    }

    std::vector<uint8_t> pack_frame(uint8_t cmd, const std::vector<uint8_t>& params) {
        std::vector<uint8_t> data_field;
        data_field.push_back(ADDR);
        data_field.push_back(cmd);
        data_field.insert(data_field.end(), params.begin(), params.end());
        
        uint8_t chk = calc_checksum(data_field);
        data_field.push_back(chk);
        
        std::vector<uint8_t> escaped = escape_data(data_field);
        
        std::vector<uint8_t> final_frame;
        final_frame.reserve(escaped.size() + 2);
        final_frame.push_back(START_BYTE);
        final_frame.insert(final_frame.end(), escaped.begin(), escaped.end());
        final_frame.push_back(END_BYTE);
        
        return final_frame;
    }

    bool unpack_frame(const std::vector<uint8_t>& raw_frame, uint8_t& out_cmd, std::vector<uint8_t>& out_params) {
        if (raw_frame.size() < 5) return false; // 最小 CA ADDR CMD CHK AC (未转义情况)
        if (raw_frame.front() != START_BYTE || raw_frame.back() != END_BYTE) return false;
        
        // 提取有效载荷部分用于反转义
        std::vector<uint8_t> payload(raw_frame.begin() + 1, raw_frame.end() - 1);
        std::vector<uint8_t> unescaped = unescape_data(payload);
        
        if (unescaped.size() < 3) return false; // ADDR, CMD, CHK 最少3字节
        
        uint8_t rx_chk = unescaped.back();
        std::vector<uint8_t> data_field(unescaped.begin(), unescaped.end() - 1);
        
        if (calc_checksum(data_field) != rx_chk) return false; // 校验失败
        if (data_field[0] != ADDR) return false; // 地址错误
        
        out_cmd = data_field[1];
        out_params = std::vector<uint8_t>(data_field.begin() + 2, data_field.end());
        return true;
    }

} // namespace Cooler