/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-01-27 09:00:23
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-20 20:44:01
 * @FilePath: /GMAPD_RK3588/task_module/header/util.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <cstdint>
#include <cmath>

#include "peripheryApi.h"
#include "log.h"

extern periDev::Ltc2991Dev fpga_temp_dev ;
extern periDev::Ltc2991Dev rk_temp_dev ;
extern periDev::Ltc2991Dev air_temp_dev ;

int ComputeDelay(float TargetDistance, int BinWidth, int Gatecount);
void initTemperatureSensors();
