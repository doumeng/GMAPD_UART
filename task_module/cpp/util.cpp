/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-01-23 09:31:42
 * @LastEditors: Do not edit
 * @LastEditTime: 2026-05-18 16:52:51
 * @FilePath: \GMAPD_UART\task_module\cpp\util.cpp
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
    int delay = TargetDistance / 0.15f ;
    return delay;
}