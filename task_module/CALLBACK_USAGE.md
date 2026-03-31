# 使用回调函数处理通信中的耗时操作

## 概述

本文档说明如何使用回调函数来处理UART通信中的耗时操作，避免阻塞主线程，提高系统响应性能。

## 背景

在原始实现中，串口通信操作（如读取制冷机温度）是同步阻塞的：

```cpp
// 同步方式 - 阻塞约50ms
uint16_t temp = Cooler::getCoolerTemperature();
// 在这50ms内，调用线程无法做其他事情
```

这种方式的问题：
1. **阻塞主线程**：在等待串口响应期间，线程被完全阻塞
2. **影响实时性**：如果在周期性任务中调用，会影响任务的时序精度
3. **降低吞吐量**：无法在等待期间处理其他任务

## 解决方案：回调函数

新增的异步接口使用回调函数，在后台线程中执行耗时操作：

```cpp
// 异步方式 - 立即返回
Cooler::getCoolerTemperatureAsync([](bool success, uint16_t temp) {
    if (success) {
        // 处理温度数据
        printf("温度: %d\n", temp);
    } else {
        // 处理错误
        printf("读取失败\n");
    }
});
// 主线程继续执行，不会被阻塞
```

## 可用的异步接口

### 1. 串口工具 (SerialUtils)

#### read_frame_by_boundary_async
```cpp
void read_frame_by_boundary_async(
    serial_t* serial,
    uint8_t start_byte,
    uint8_t end_byte,
    int timeout_ms,
    ReadCallback callback
);
```

**用途**：异步读取由起始和结束字节界定的数据帧

**回调函数类型**：
```cpp
using ReadCallback = std::function<void(bool success, const std::vector<uint8_t>& data)>;
```

**示例**：
```cpp
SerialUtils::read_frame_by_boundary_async(
    serial,
    0x24,  // 起始字节
    0x40,  // 结束字节
    50,    // 超时50ms
    [](bool success, const std::vector<uint8_t>& data) {
        if (success) {
            // 处理接收到的数据帧
            process_frame(data);
        }
    }
);
```

#### read_fixed_length_async
```cpp
void read_fixed_length_async(
    serial_t* serial,
    size_t length,
    int timeout_ms,
    ReadCallback callback
);
```

**用途**：异步读取固定长度的数据帧

**示例**：
```cpp
SerialUtils::read_fixed_length_async(
    serial,
    32,    // 读取32字节
    50,    // 超时50ms
    [](bool success, const std::vector<uint8_t>& data) {
        if (success && data.size() == 32) {
            // 处理32字节数据
            decode_command(data);
        }
    }
);
```

### 2. 制冷机控制 (Cooler)

#### getCoolerTemperatureAsync
```cpp
void getCoolerTemperatureAsync(CoolerCallback callback);
```

**用途**：异步读取制冷机温度

**回调函数类型**：
```cpp
using CoolerCallback = std::function<void(bool success, uint16_t result)>;
```

**示例**：
```cpp
Cooler::getCoolerTemperatureAsync([](bool success, uint16_t temp) {
    if (success) {
        float tempK = temp / 10.0f;
        printf("制冷机温度: %.1f K\n", tempK);
    } else {
        printf("读取温度失败\n");
    }
});
```

#### startCoolerAsync
```cpp
void startCoolerAsync(CoolerCallback callback);
```

**用途**：异步启动制冷机

**示例**：
```cpp
Cooler::startCoolerAsync([](bool success, uint16_t result) {
    if (success) {
        printf("制冷机启动成功\n");
    } else {
        printf("制冷机启动失败\n");
    }
});
```

#### stopCoolerAsync
```cpp
void stopCoolerAsync(CoolerCallback callback);
```

**用途**：异步停止制冷机

#### setTargetTempAsync
```cpp
void setTargetTempAsync(uint16_t tempK, CoolerCallback callback);
```

**用途**：异步设置目标温度

**示例**：
```cpp
Cooler::setTargetTempAsync(1200, [](bool success, uint16_t result) {
    if (success) {
        printf("目标温度设置为120.0K成功\n");
    }
});
```

## 使用场景

### 场景1：周期性任务中的非阻塞操作

**问题**：在20ms周期的主循环中，需要读取温度但不能阻塞循环

**解决方案**：使用异步回调
```cpp
void periodic_task() {
    while (running) {
        auto start = std::chrono::steady_clock::now();

        // 处理其他任务...
        process_commands();

        // 异步读取温度，不阻塞主循环
        Cooler::getCoolerTemperatureAsync([](bool success, uint16_t temp) {
            // 回调在另一个线程中执行
            if (success) {
                update_temperature_display(temp);
            }
        });

        // 继续其他任务...
        send_status();

        // 等待到周期结束
        sleep_until_period_end(start, 20ms);
    }
}
```

### 场景2：避免串行等待

**问题**：需要连续执行多个耗时操作

**同步方式（低效）**：
```cpp
// 总耗时 = 50ms + 50ms + 50ms = 150ms
startCooler();           // 阻塞50ms
setTargetTemp(1200);     // 阻塞50ms
getCoolerTemperature();  // 阻塞50ms
```

**异步方式（高效）**：
```cpp
// 总耗时约50-60ms（操作可以流水线执行）
startCoolerAsync([](bool success, uint16_t result) {
    if (success) {
        setTargetTempAsync(1200, [](bool success, uint16_t result) {
            if (success) {
                getCoolerTemperatureAsync([](bool success, uint16_t temp) {
                    // 完成
                });
            }
        });
    }
});
```

### 场景3：超时保护

**问题**：串口操作可能因为硬件问题长时间无响应

**解决方案**：使用异步+超时监控
```cpp
std::atomic<bool> completed{false};

Cooler::getCoolerTemperatureAsync([&completed](bool success, uint16_t temp) {
    // 处理结果
    completed.store(true);
});

// 在主线程中实现超时监控
auto start = std::chrono::steady_clock::now();
while (!completed.load()) {
    auto now = std::chrono::steady_clock::now();
    if (duration_cast<milliseconds>(now - start).count() > 100) {
        // 超时处理
        handle_timeout();
        break;
    }
    std::this_thread::sleep_for(1ms);
}
```

## 注意事项

### 1. 线程安全

所有异步函数内部都使用了互斥锁保护串口访问：
```cpp
static std::mutex g_serialMutex;

uint16_t getCoolerTemperature() {
    std::lock_guard<std::mutex> lock(g_serialMutex);
    // 串口操作
}
```

因此可以安全地从多个线程调用异步函数。

### 2. 回调函数的执行上下文

回调函数在**后台线程**中执行，不是在调用异步函数的线程中执行：

```cpp
printf("主线程ID: %ld\n", std::this_thread::get_id());

Cooler::getCoolerTemperatureAsync([](bool success, uint16_t temp) {
    // 这里运行在不同的线程中
    printf("回调线程ID: %ld\n", std::this_thread::get_id());
});
```

**影响**：
- 回调中访问共享数据需要加锁
- 回调中不能假设调用线程的状态
- 回调函数要尽快执行完毕，避免阻塞后台线程

### 3. 何时使用同步 vs 异步

**使用同步接口的场景**：
- 需要立即获得结果才能继续
- 代码逻辑简单，不需要并发
- 在专门的串口处理线程中

**使用异步接口的场景**：
- 在主循环或实时任务中
- 可以在等待时做其他工作
- 需要同时发起多个操作
- 需要超时监控和错误恢复

### 4. 资源管理

异步函数使用`std::thread::detach()`创建分离线程。确保：
- 串口句柄在回调执行期间保持有效
- 回调中捕获的变量生命周期足够长（建议使用shared_ptr或atomic）

**错误示例**：
```cpp
void bad_example() {
    int local_var = 0;

    Cooler::getCoolerTemperatureAsync([&local_var](bool success, uint16_t temp) {
        // 危险！local_var可能已经失效
        local_var = temp;
    });

    // 函数返回，local_var被销毁
}
```

**正确示例**：
```cpp
void good_example() {
    auto shared_var = std::make_shared<std::atomic<int>>(0);

    Cooler::getCoolerTemperatureAsync([shared_var](bool success, uint16_t temp) {
        // 安全：shared_var由智能指针管理
        shared_var->store(temp);
    });
}
```

## 示例程序

完整的示例程序位于：`task_module/cpp/async_cooler_example.cpp`

编译运行：
```bash
# 添加到CMakeLists.txt
add_executable(async_example task_module/cpp/async_cooler_example.cpp)
target_link_libraries(async_example ...)

# 编译
mkdir build && cd build
cmake ..
make async_example

# 运行
./async_example
```

## 性能对比

### 同步方式
```
操作: 读取温度 (同步)
耗时: ~50ms
主线程阻塞: 是
可并发操作: 否
```

### 异步方式
```
操作: 读取温度 (异步)
调用耗时: <1ms (立即返回)
回调耗时: ~50ms (在后台线程)
主线程阻塞: 否
可并发操作: 是
```

## 总结

回调函数方式的优势：
1. ✅ 不阻塞主线程，提高响应性
2. ✅ 可以在等待时执行其他任务
3. ✅ 支持超时监控和错误恢复
4. ✅ 便于实现异步流水线处理

同时保留同步接口：
- ✅ 向后兼容现有代码
- ✅ 简单场景下使用更方便
- ✅ 代码更易理解和维护

根据具体需求选择合适的接口方式。
