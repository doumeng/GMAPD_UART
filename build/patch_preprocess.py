import re

with open('/home/tcspc/code/GMAPD_UART/task_module/cpp/preprocess_uart_slave.cpp', 'r') as f:
    code = f.read()

# Replace C2
code = re.sub(
    r'Logger::instance\(\)\.info\("PreprocessUart CMD 0xC2 - 制冷机上.电"\);\s+gCmdStatus = Cooler::startCooler\(\);\s+if \(gCmdStatus == 0\) \{\s+Logger::instance\(\)\.error\("Failed to start cooler!"\);\s+\} else \{\s+Logger::instance\(\)\.info\("Cooler start command executed successfully!"\);\s+\}',
    '''Logger::instance().info("PreprocessUart CMD 0xC2 - 制冷机上电");
                    Cooler::startCoolerAsync([](bool success, uint8_t) {
                        gCmdStatus = success ? 1 : 0;
                        if (!success) Logger::instance().error("Failed to start cooler!");
                        else Logger::instance().info("Cooler start command executed successfully!");
                    });''',
    code
)

# Replace C3
c3_old = r'gCmdStatus = Cooler::setTargetTemp\(tempRaw\); \s+if \(gCmdStatus == 0\) \{\s+Logger::instance\(\)\.error\("Failed to set cooler temperature!"\);\s+\} else \{\s+Logger::instance\(\)\.info\("Cooler temperature setting command executed successfully!"\);\s+\}\s+usleep\(50000\);\s+gCmdStatus = Cooler::saveConfig\(\);\s+if \(gCmdStatus == 0\) \{\s+Logger::instance\(\)\.error\("Failed to save temperature!"\);\s+\} else \{\s+Logger::instance\(\)\.info\("Cooler temperature save command executed successfully!"\);\s+\}'
c3_new = '''Cooler::setTargetTempAsync(tempRaw, [](bool success, uint8_t) {
                        if (!success) {
                            Logger::instance().error("Failed to set cooler temperature!");
                            gCmdStatus = 0;
                        } else {
                            Logger::instance().info("Cooler temperature setting command executed successfully!");
                            std::thread([]() {
                                usleep(50000);
                                Cooler::saveConfigAsync([](bool succ2, uint8_t) {
                                    gCmdStatus = succ2 ? 1 : 0;
                                    if (!succ2) Logger::instance().error("Failed to save temperature!");
                                    else Logger::instance().info("Cooler temperature save command executed successfully!");
                                });
                            }).detach();
                        }
                    });'''
code = re.sub(c3_old, c3_new, code)

# Replace C9
c9_old = r'gCmdStatus = Cooler::stopCooler\(\);\s+if \(gCmdStatus == 0\) \{\s+Logger::instance\(\)\.error\("Failed to stop cooler!"\);\s+\} else \{\s+Logger::instance\(\)\.info\("Cooler stop command executed successfully!"\);\s+\}'
c9_new = '''Cooler::stopCoolerAsync([](bool success, uint8_t) {
                        gCmdStatus = success ? 1 : 0;
                        if (!success) Logger::instance().error("Failed to stop cooler!");
                        else Logger::instance().info("Cooler stop command executed successfully!");
                    });'''
code = re.sub(c9_old, c9_new, code)

# Replace getCoolerTemperature wrapper!
# In thread_Preprocess_Communication, replace getCoolerTemperature() with a cached atomic value.
code = code.replace('#include "cooler_control.h"', '#include "cooler_control.h"\n#include <atomic>\n\nstd::atomic<uint16_t> g_cachedCoolerTemp{0};')

get_temp_old = r'uint16_t tempValue = Cooler::getCoolerTemperature\(\);'
get_temp_new = '''uint16_t tempValue = g_cachedCoolerTemp.load();'''
code = re.sub(get_temp_old, get_temp_new, code)

# Insert the periodic async fetch at the beginning of thread_Preprocess_Communication
thread_old = r'uint8_t pendingReplyCmd = 0;\s+uint32_t txSequence = 1;'
thread_new = '''uint8_t pendingReplyCmd = 0;
        uint32_t txSequence = 1;
        auto last_temp_req = std::chrono::steady_clock::now();'''
code = re.sub(thread_old, thread_new, code)

# Insert trigger inside while loop
while_old = r'auto startTime = std::chrono::steady_clock::now\(\);'
while_new = '''auto startTime = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(startTime - last_temp_req).count() > 500) {
                last_temp_req = startTime;
                Cooler::getCoolerTemperatureAsync([](bool success, uint16_t temp) {
                    if (success) g_cachedCoolerTemp.store(temp);
                });
            }'''
code = re.sub(while_old, while_new, code)

with open('/home/tcspc/code/GMAPD_UART/task_module/cpp/preprocess_uart_slave.cpp', 'w') as f:
    f.write(code)

