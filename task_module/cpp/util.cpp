/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-01-23 09:31:42
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-26 19:39:02
 * @FilePath: /K253154_Preprocess_RK3588_0123/task_module/cpp/util.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#include <string>

#include "peripheryApi.h"
#include "util.h"

std::string fpga_temp_path = "/sys/class/hwmon/hwmon0/temp1_input";
std::string rk_temp_path = "/sys/class/hwmon/hwmon0/temp2_input";
std::string air_temp_path = "/sys/class/hwmon/hwmon0/temp5_input";

periDev::Ltc2991Dev fpga_temp_dev = periDev::Ltc2991Dev(fpga_temp_path);
periDev::Ltc2991Dev rk_temp_dev = periDev::Ltc2991Dev(rk_temp_path);
periDev::Ltc2991Dev air_temp_dev = periDev::Ltc2991Dev(air_temp_path);

void initTemperatureSensors() {
    if (fpga_temp_dev.openDev() == 0) {
        Logger::instance().error("Failed to open FPGA temperature sensor");
    }
    if (rk_temp_dev.openDev() == 0) {
        Logger::instance().error("Failed to open RK temperature sensor");
    }
    if (air_temp_dev.openDev() == 0) {
        Logger::instance().error("Failed to open Air temperature sensor");
    }
}

// 根据距离，计算需要的延迟时间
int ComputeDelay(float TargetDistance, int BinWidth, int Gatecount) {
    // int bincount = (TargetDistance * 2) / (0.3 * BinWidth); // 计算需要的bin数
    // int timeDelay = bincount - (bincount % Gatecount); // 计算实际延迟bin数
    // return timeDelay * 2;
    int delay = TargetDistance / 0.15f ;
    return delay;
}