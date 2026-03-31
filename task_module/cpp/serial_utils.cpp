/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-17 19:36:19
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-23 11:07:24
 * @FilePath: /GMAPD_UART/task_module/cpp/serial_utils.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "serial_utils.h"

#include <chrono>

#include "serial.h"
#include "log.h"

namespace SerialUtils {

    serial_t* open_serial_port(const std::string& port, uint32_t baudrate, int databits, int parity, int stopbits) {
        serial_t* serial = serial_new();
        if (!serial) return nullptr;

        if (serial_open_advanced(serial, port.c_str(), baudrate, databits, (serial_parity_t)parity, stopbits, false, false) < 0) {
            Logger::instance().error(std::string("Failed to open serial port " + port + ": " + std::string(serial_errmsg(serial))).c_str());
            serial_free(serial);
            return nullptr;
        }
        return serial;
    }

    void close_serial_port(serial_t* serial) {
        if (serial) {
            serial_close(serial);
            serial_free(serial);
        }
    }

    bool write_frame(serial_t* serial, const std::vector<uint8_t>& frame) {
        if (!serial || frame.empty()) return false;
        if (serial_write(serial, frame.data(), frame.size()) < 0) {
            Logger::instance().error("Serial write failed");
            return false;
        }
        return true;
    }

    std::vector<uint8_t> read_frame_by_boundary(serial_t* serial, uint8_t start_byte, uint8_t end_byte, int timeout_ms) {
        std::vector<uint8_t> frame;
        if (!serial) return frame;

        auto start_time = std::chrono::steady_clock::now();
        bool started = false;

        while (true) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() > timeout_ms) {
                break;
            }

            uint8_t b;
            if (serial_read(serial, &b, 1, 10) == 1) {
                if (!started) {
                    if (b == start_byte) {
                        started = true;
                        frame.push_back(b);
                    }
                } else {
                    frame.push_back(b);
                    if (b == end_byte) {
                        break;
                    }
                }
            }
        }
        return frame;
    }

    std::vector<uint8_t> read_fixed_length(serial_t* serial, size_t length, int timeout_ms) {
        std::vector<uint8_t> frame;
        if (!serial || length == 0) return frame;

        auto start_time = std::chrono::steady_clock::now();
        frame.reserve(length);

        while (frame.size() < length) {
            auto now = std::chrono::steady_clock::now();
            int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            if (elapsed > timeout_ms) {
                break;
            }

            uint8_t b;
            if (serial_read(serial, &b, 1, std::max(1, timeout_ms - elapsed)) == 1) {
                frame.push_back(b);
            }
        }
        return frame;
    }

} // namespace SerialUtils
