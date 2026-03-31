#pragma once

#include <cstdint>

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <memory>

// 向前声明
struct serial_handle;
typedef struct serial_handle serial_t;

namespace SerialUtils {

    // 回调函数类型定义
    // success: 是否成功读取完整数据帧
    // data: 读取到的数据
    using ReadCallback = std::function<void(bool success, const std::vector<uint8_t>& data)>;

    /**
     * @brief 打开串口设备
     * 
     * @param port 串口设备路径
     * @param baudrate 波特率
     * @param databits 数据位，默认8
     * @param parity 校验位，默认无校验(0)
     * @param stopbits 停止位，默认1
     * @return serial_t* 返回打开的串口句柄，失败返回nullptr
     */
    serial_t* open_serial_port(const std::string& port, uint32_t baudrate, int databits = 8, int parity = 0, int stopbits = 1);

    /**
     * @brief 关闭串口设备
     * 
     * @param serial 串口句柄
     */
    void close_serial_port(serial_t* serial);

    /**
     * @brief 向串口写入数据帧
     * 
     * @param serial 串口句柄
     * @param frame 要写入的数据
     * @return true 写入成功
     * @return false 写入失败
     */
    bool write_frame(serial_t* serial, const std::vector<uint8_t>& frame);

    /**
     * @brief 根据起始字节和结束字节读取一帧数据 (适用于制冷机协议)
     * 
     * @param serial 串口句柄
     * @param start_byte 起始字节
     * @param end_byte 结束字节
     * @param timeout_ms 超时时间(毫秒)
     * @return std::vector<uint8_t> 读取到的完整数据帧
     */
    std::vector<uint8_t> read_frame_by_boundary(serial_t* serial, uint8_t start_byte, uint8_t end_byte, int timeout_ms);

    /**
     * @brief 读取固定长度的数据帧 (适用于预处理通信协议)
     *
     * @param serial 串口句柄
     * @param length 期望读取的长度
     * @param timeout_ms 超时时间(毫秒)
     * @return std::vector<uint8_t> 读取到的数据帧
     */
    std::vector<uint8_t> read_fixed_length(serial_t* serial, size_t length, int timeout_ms);

    /**
     * @brief 异步读取数据帧（根据起始和结束字节），读取完成后调用回调函数
     *
     * @param serial 串口句柄
     * @param start_byte 起始字节
     * @param end_byte 结束字节
     * @param timeout_ms 超时时间(毫秒)
     * @param callback 读取完成后的回调函数
     */
    void read_frame_by_boundary_async(serial_t* serial, uint8_t start_byte, uint8_t end_byte,
                                      int timeout_ms, ReadCallback callback);

    /**
     * @brief 异步读取固定长度数据帧，读取完成后调用回调函数
     *
     * @param serial 串口句柄
     * @param length 期望读取的长度
     * @param timeout_ms 超时时间(毫秒)
     * @param callback 读取完成后的回调函数
     */
    void read_fixed_length_async(serial_t* serial, size_t length, int timeout_ms, ReadCallback callback);

} // namespace SerialUtils
