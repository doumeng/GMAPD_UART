/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-02-02 09:07:25
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-23 10:47:41
 * @FilePath: /GMAPD_RK3588/task_module/header/compute_distance.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <cstddef>

namespace TofProcesser {
    bool calculateMedianDistance(const float *distanceData,
                                 std::size_t distanceCount,
                                 int rows,
                                 int cols,
                                 std::size_t roiSize,
                                 float &targetDistance);
    void thread_TofProcess();
}
