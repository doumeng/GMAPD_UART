/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-02-02 09:07:25
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-20 21:25:57
 * @FilePath: /GMAPD_RK3588/task_module/header/point_cloud_process.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <tuple>

#include "util.h" // 包含二维数组分配和释放的函数声明
#include "log.h"  // 日志记录功能
#include "task_reg.h" // 包含共享数据结构和全局变量声明

// 二维数组内存分配和释放由 util.h 提供

namespace PointCloud {
    void thread_PointCloudProcess();
}

