#include "preprocess_uart_protocol.h"

// 1. C/C++ 标准库
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace PreprocessUart {

namespace {

constexpr std::size_t kHeaderStart = 0;
constexpr std::size_t kHeaderEnd = 3;

constexpr std::size_t kSeqIdx = 4;
constexpr std::size_t kApdBiasIdx = 6;
constexpr std::size_t kCtlParaIdx = 8;
constexpr std::size_t kAlgoFrameDenoiseIdx = 9;
constexpr std::size_t kAlgoStrideDiffIdx = 10;
constexpr std::size_t kAlgoKernalIdx = 11;
constexpr std::size_t kPowerOnOffIdx = 12;
constexpr std::size_t kDistanceIdx = 13;
constexpr std::size_t kVelocityIdx = 15;
constexpr std::size_t kTempCtlIdx = 17;

constexpr std::size_t kFcsLowIdx = 26;
constexpr std::size_t kFcsHighIdx = 27;
constexpr std::size_t kTailStart = 28;
constexpr std::size_t kTailEnd = 31;


uint16_t readLe16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

void writeLe16(uint8_t *p, uint16_t value) {
    p[0] = static_cast<uint8_t>(value & 0xFF);
    p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

bool checkFlag(const std::array<uint8_t, kFrameSize> &raw,
               std::size_t begin,
               std::size_t end) {
    for (std::size_t i = begin; i <= end; ++i) {
        if (raw[i] != kFlagByte) {
            return false;
        }
    }
    return true;
}

} // namespace

uint16_t calcFcs16(const uint8_t *data, std::size_t len) {
    uint32_t sum = 0;
    for (std::size_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return static_cast<uint16_t>(sum & 0xFFFF);
}

uint16_t calcFrameFcs(const std::array<uint8_t, kFrameSize> &frame) {
    return calcFcs16(frame.data() + 4, 22);
}

bool decodeCommandFrame(const std::array<uint8_t, kFrameSize> &raw,
                        CommandFrame &out,
                        std::string *reason) {
    if (!checkFlag(raw, kHeaderStart, kHeaderEnd)) {
        if (reason != nullptr) {
            *reason = "invalid header";
        }
        return false;
    }

    if (!checkFlag(raw, kTailStart, kTailEnd)) {
        if (reason != nullptr) {
            *reason = "invalid tail";
        }
        return false;
    }

    const uint16_t rxFcs = readLe16(raw.data() + kFcsLowIdx);
    const uint16_t expectedFcs = calcFrameFcs(raw);
    if (rxFcs != expectedFcs) {
        if (reason != nullptr) {
            std::ostringstream oss;
            oss << "fcs mismatch rx=0x" << std::hex << std::setw(4) << std::setfill('0') << rxFcs
                << " expected=0x" << std::setw(4) << expectedFcs;
            *reason = oss.str();
        }
        return false;
    }

    out.raw = raw;
    out.sequence = readLe16(raw.data() + kSeqIdx);
    out.apd_bias = readLe16(raw.data() + kApdBiasIdx);
    out.ctl_para = raw[kCtlParaIdx];
    out.algo_frame_denoise = raw[kAlgoFrameDenoiseIdx];
    out.algo_stride_diff = raw[kAlgoStrideDiffIdx];
    out.algo_kernal = raw[kAlgoKernalIdx];
    out.power_on_off = raw[kPowerOnOffIdx];
    out.distance = readLe16(raw.data() + kDistanceIdx);
    out.velocity = readLe16(raw.data() + kVelocityIdx);
    out.temp_ctl = readLe16(raw.data() + kTempCtlIdx);
    return true;
}

ReplyFrame buildReplyFrame(uint16_t sequence,
                           uint8_t version,
                           uint8_t apd_bias_status,
                           uint8_t ctl_para_status,
                           uint8_t algo_para_status,
                           uint8_t power_status,
                           uint8_t temp_low,
                           uint8_t temp_high,
                           uint8_t volt_int,
                           uint8_t volt_frac,
                           uint8_t fpga_temp_low,
                           uint8_t fpga_temp_high) {
    ReplyFrame reply;

    for (std::size_t i = 0; i < kFrameSize; ++i) {
        reply.raw[i] = 0;
    }

    for (std::size_t i = kHeaderStart; i <= kHeaderEnd; ++i) {
        reply.raw[i] = kFlagByte;
    }
    for (std::size_t i = kTailStart; i <= kTailEnd; ++i) {
        reply.raw[i] = kFlagByte;
    }

    writeLe16(reply.raw.data() + 4, sequence);
    
    reply.raw[6] = version;
    
    reply.raw[7] = apd_bias_status;
    reply.raw[8] = ctl_para_status;
    reply.raw[9] = algo_para_status;
    reply.raw[10] = power_status;
    
    reply.raw[11] = temp_low;
    reply.raw[12] = temp_high;
    reply.raw[13] = volt_int;
    reply.raw[14] = volt_frac;
    reply.raw[15] = fpga_temp_low;
    reply.raw[16] = fpga_temp_high;

    const uint16_t fcs = calcFrameFcs(reply.raw);
    writeLe16(reply.raw.data() + kFcsLowIdx, fcs);
    return reply;
}

std::string frameToHex(const std::array<uint8_t, kFrameSize> &frame) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < frame.size(); ++i) {
        oss << std::setw(2) << static_cast<int>(frame[i]);
        if (i + 1 < frame.size()) {
            oss << ' ';
        }
    }
    return oss.str();
}

void decodeTemperature(uint16_t temp, uint8_t &low, uint8_t &high) {
    low = static_cast<uint8_t>(temp & 0xFF);
    high = static_cast<uint8_t>((temp >> 8) & 0xFF);
}

float encodeTemperature(uint8_t low, uint8_t high) {
    int16_t t = static_cast<int16_t>(low | (high << 8));
    return static_cast<float>(t) / 10.0f;
}

void decodeVoltage(float voltage, uint8_t &low, uint8_t &high) {
    int v10 = static_cast<int>(voltage * 10.0f + (voltage >= 0 ? 0.5f : -0.5f));
    low = static_cast<uint8_t>((v10 / 10) & 0xFF);
    high = static_cast<uint8_t>((v10 % 10) & 0xFF);
}

float encodeVoltage(uint8_t low, uint8_t high) {
    return static_cast<float>(low) + static_cast<float>(high) / 10.0f;
}

} // namespace PreprocessUart
