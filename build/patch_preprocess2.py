import re

with open('/home/tcspc/code/GMAPD_UART/task_module/cpp/preprocess_uart_slave.cpp', 'r') as f:
    code = f.read()

# Replace the read_fixed_length synchronous call with async handling
thread_old = r'''        uint8_t pendingReplyCmd = 0;
        uint32_t txSequence = 1;
        auto last_temp_req = std::chrono::steady_clock::now\(\);'''

thread_new = '''        uint8_t pendingReplyCmd = 0;
        uint32_t txSequence = 1;
        auto last_temp_req = std::chrono::steady_clock::now();
        
        std::atomic<bool> isReading{false};
        std::vector<uint8_t> latestRxData;
        std::mutex rxMutex;'''

code = re.sub(thread_old, thread_new, code)

read_old = r'''            // 1\. 接收指令帧 \(先收以提升响应实时性\)
            int readTimeout = std::max\(1, periodMs - 10\); // 留出一点余裕保证发送和状态机更新
            std::vector<uint8_t> rxData = SerialUtils::read_fixed_length\(serial, kFrameSize, readTimeout\);

            if \(rxData\.size\(\) == kFrameSize\) \{'''

read_new = '''            // 1. 接收指令帧 (异步处理，不阻塞主循环)
            if (!isReading.exchange(true)) {
                // 如果当前没有正在读取，则发起一次异步读取
                SerialUtils::read_fixed_length_async(serial, kFrameSize, periodMs - 2, 
                    [&isReading, &latestRxData, &rxMutex](bool success, const std::vector<uint8_t>& data) {
                        if (success) {
                            std::lock_guard<std::mutex> lock(rxMutex);
                            latestRxData = data;
                        }
                        isReading = false;
                    });
            }

            std::vector<uint8_t> rxData;
            {
                std::lock_guard<std::mutex> lock(rxMutex);
                if (!latestRxData.empty()) {
                    rxData = std::move(latestRxData);
                    latestRxData.clear();
                }
            }

            if (rxData.size() == kFrameSize) {'''

code = re.sub(read_old, read_new, code)

# We should also sleep at the end of the loop if it finishes too quickly so we maintain the ~20ms period tightly!
end_loop_old = r'''            Logger::instance\(\)\.debug\(\("PreprocessUart Sending reply frame: " \+ frameToHex\(reply\.raw\)\)\.c_str\(\)\);
            SerialUtils::write_frame\(serial, txData\);
        \}'''

end_loop_new = '''            Logger::instance().debug(("PreprocessUart Sending reply frame: " + frameToHex(reply.raw)).c_str());
            SerialUtils::write_frame(serial, txData);
            
            // 维持约20ms的主循环周期
            auto endTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            if (elapsed < periodMs) {
                std::this_thread::sleep_for(std::chrono::milliseconds(periodMs - elapsed));
            }
        }'''

code = re.sub(end_loop_old, end_loop_new, code)

with open('/home/tcspc/code/GMAPD_UART/task_module/cpp/preprocess_uart_slave.cpp', 'w') as f:
    f.write(code)
