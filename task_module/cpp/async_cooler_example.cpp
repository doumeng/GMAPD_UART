/**
 * @file async_cooler_example.cpp
 * @brief 演示如何使用回调函数处理制冷机通信中的耗时操作
 *
 * 本文件展示了如何使用异步回调函数来处理制冷机的耗时操作，
 * 避免阻塞主线程，提高系统响应性能。
 */

#include "cooler_control.h"
#include "log.h"
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>

/**
 * @brief 演示同步调用方式（会阻塞当前线程）
 */
void demonstrateSyncApproach() {
    Logger::instance().info("=== 演示同步调用方式 ===");

    auto start = std::chrono::steady_clock::now();

    // 同步调用，会阻塞约50ms等待串口响应
    uint16_t temp = Cooler::getCoolerTemperature();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    Logger::instance().info(("同步读取温度: " + std::to_string(temp) +
                            ", 耗时: " + std::to_string(duration) + "ms").c_str());
    Logger::instance().info("注意：在这段时间内，调用线程被完全阻塞");
}

/**
 * @brief 演示异步回调方式（不阻塞当前线程）
 */
void demonstrateAsyncApproach() {
    Logger::instance().info("\n=== 演示异步回调方式 ===");

    std::atomic<bool> completed{false};
    uint16_t receivedTemp = 0;

    auto start = std::chrono::steady_clock::now();

    // 异步调用，立即返回，不阻塞当前线程
    Cooler::getCoolerTemperatureAsync([&completed, &receivedTemp](bool success, uint16_t temp) {
        if (success) {
            receivedTemp = temp;
            Logger::instance().info(("异步回调：成功读取温度 = " + std::to_string(temp)).c_str());
        } else {
            Logger::instance().error("异步回调：读取温度失败");
        }
        completed.store(true);
    });

    auto callReturn = std::chrono::steady_clock::now();
    auto callDuration = std::chrono::duration_cast<std::chrono::milliseconds>(callReturn - start).count();

    Logger::instance().info(("异步调用返回耗时: " + std::to_string(callDuration) +
                            "ms (几乎立即返回)").c_str());
    Logger::instance().info("主线程可以继续执行其他任务...");

    // 模拟主线程继续做其他工作
    for (int i = 0; i < 5 && !completed.load(); i++) {
        Logger::instance().info(("主线程工作中... (" + std::to_string(i+1) + "/5)").c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 等待回调完成
    while (!completed.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto end = std::chrono::steady_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    Logger::instance().info(("总耗时: " + std::to_string(totalDuration) +
                            "ms (主线程可以做其他工作)").c_str());
}

/**
 * @brief 演示批量异步操作
 */
void demonstrateBatchAsync() {
    Logger::instance().info("\n=== 演示批量异步操作 ===");

    std::atomic<int> completedCount{0};
    constexpr int totalOps = 3;

    auto start = std::chrono::steady_clock::now();

    // 同时启动多个异步操作
    Cooler::getCoolerTemperatureAsync([&completedCount](bool success, uint16_t temp) {
        Logger::instance().info(("操作1完成: 温度 = " + std::to_string(temp)).c_str());
        completedCount.fetch_add(1);
    });

    Cooler::getCoolerTemperatureAsync([&completedCount](bool success, uint16_t temp) {
        Logger::instance().info(("操作2完成: 温度 = " + std::to_string(temp)).c_str());
        completedCount.fetch_add(1);
    });

    Cooler::getCoolerTemperatureAsync([&completedCount](bool success, uint16_t temp) {
        Logger::instance().info(("操作3完成: 温度 = " + std::to_string(temp)).c_str());
        completedCount.fetch_add(1);
    });

    Logger::instance().info("已发起3个异步操作，等待全部完成...");

    // 等待所有操作完成
    while (completedCount.load() < totalOps) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    Logger::instance().info(("3个异步操作总耗时: " + std::to_string(duration) + "ms").c_str());
    Logger::instance().info("注意：异步操作可以并发执行，但串口访问仍然是串行的（有互斥锁保护）");
}

/**
 * @brief 演示错误处理
 */
void demonstrateErrorHandling() {
    Logger::instance().info("\n=== 演示错误处理 ===");

    std::atomic<bool> completed{false};

    Cooler::getCoolerTemperatureAsync([&completed](bool success, uint16_t temp) {
        if (success) {
            Logger::instance().info(("读取成功: 温度 = " + std::to_string(temp)).c_str());
            // 在这里处理成功的情况
        } else {
            Logger::instance().error("读取失败: 超时或串口错误");
            // 在这里处理失败的情况，例如重试或记录错误
        }
        completed.store(true);
    });

    // 等待完成
    while (!completed.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

/**
 * @brief 演示链式异步操作
 */
void demonstrateChainedAsync() {
    Logger::instance().info("\n=== 演示链式异步操作 ===");

    std::atomic<bool> allCompleted{false};

    // 第一步：启动制冷机
    Cooler::startCoolerAsync([&allCompleted](bool success, uint16_t result) {
        if (success) {
            Logger::instance().info("步骤1: 制冷机启动成功");

            // 第二步：设置目标温度
            Cooler::setTargetTempAsync(1200, [&allCompleted](bool success, uint16_t result) {
                if (success) {
                    Logger::instance().info("步骤2: 目标温度设置成功");

                    // 第三步：读取当前温度
                    Cooler::getCoolerTemperatureAsync([&allCompleted](bool success, uint16_t temp) {
                        if (success) {
                            Logger::instance().info(("步骤3: 读取温度成功 = " + std::to_string(temp)).c_str());
                        }
                        allCompleted.store(true);
                    });
                } else {
                    Logger::instance().error("步骤2: 目标温度设置失败");
                    allCompleted.store(true);
                }
            });
        } else {
            Logger::instance().error("步骤1: 制冷机启动失败");
            allCompleted.store(true);
        }
    });

    Logger::instance().info("已发起链式异步操作，等待完成...");

    // 等待所有操作完成
    while (!allCompleted.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Logger::instance().info("链式操作全部完成");
}

/**
 * @brief 主函数：运行所有演示
 */
int main() {
    Logger::instance().info("===== 回调函数处理耗时操作演示程序 =====\n");

    // 初始化制冷机串口
    if (!Cooler::initCooler("/dev/ttyS2", 4800)) {
        Logger::instance().error("无法打开制冷机串口，演示程序退出");
        return 1;
    }

    // 演示1：同步 vs 异步对比
    demonstrateSyncApproach();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    demonstrateAsyncApproach();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 演示2：批量异步操作
    demonstrateBatchAsync();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 演示3：错误处理
    demonstrateErrorHandling();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 演示4：链式异步操作
    demonstrateChainedAsync();

    // 清理
    Cooler::deinitCooler();

    Logger::instance().info("\n===== 演示程序结束 =====");

    return 0;
}
