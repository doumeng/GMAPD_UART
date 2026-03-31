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
constexpr std::size_t kAddrIdx = 4;
constexpr std::size_t kTypeIdx = 5;
constexpr std::size_t kNumStart = 6;
constexpr std::size_t kCmdIdx = 10;
constexpr std::size_t kParamLowIdx = 11;
constexpr std::size_t kParamHighIdx = 12;
constexpr std::size_t kParamLastIdx = 13;
constexpr std::size_t kFcsLowIdx = 26;
constexpr std::size_t kFcsHighIdx = 27;
constexpr std::size_t kTailStart = 28;
constexpr std::size_t kTailEnd = 31;

uint32_t readLe32(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t readLe16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

void writeLe16(uint8_t *p, uint16_t value) {
    p[0] = static_cast<uint8_t>(value & 0xFF);
    p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void writeLe32(uint8_t *p, uint32_t value) {
    p[0] = static_cast<uint8_t>(value & 0xFF);
    p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
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
    return calcFcs16(frame.data() + kAddrIdx, 22);
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

    if (raw[kAddrIdx] != kAddr) {
        if (reason != nullptr) {
            *reason = "invalid address";
        }
        return false;
    }

    if (raw[kTypeIdx] != kType) {
        if (reason != nullptr) {
            *reason = "invalid type";
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
    out.sequence = readLe32(raw.data() + kNumStart);
    out.cmd = raw[kCmdIdx];
    out.paramLow = raw[kParamLowIdx];
    out.paramHigh = raw[kParamHighIdx];
    out.paramLast = raw[kParamLastIdx];
    return true;
}

ReplyFrame buildReplyFrame(uint32_t sequence,
                           uint8_t resultCmd,
                           uint8_t resp1,
                           uint8_t resp2,
                           uint8_t resp3,
                           uint8_t tempLow,
                           uint8_t tempHigh,
                           uint8_t voltLow,
                           uint8_t voltHigh) {
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

    reply.raw[kAddrIdx] = kAddr;
    reply.raw[kTypeIdx] = kType;
    writeLe32(reply.raw.data() + kNumStart, sequence);

    reply.raw[kCmdIdx] = resultCmd;
    reply.raw[kParamLowIdx] = resp1;
    reply.raw[kParamHighIdx] = resp2;
    reply.raw[13] = resp3;
    reply.raw[15] = tempLow;
    reply.raw[16] = tempHigh;
    reply.raw[17] = voltLow;
    reply.raw[18] = voltHigh;

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
