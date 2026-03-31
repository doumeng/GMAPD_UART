#ifndef APD_VOLTAGE_TABLE_H
#define APD_VOLTAGE_TABLE_H

#include <stdint.h>

namespace ApdVoltageConfig {

    struct VoltageConfig {
        float voltage;
        uint16_t spiValue;
        uint8_t level;
    };

    // 输入目标电压，输出最接近配置的 SPI值 和 Level，返回找到的实际匹配电压
    float getSpiAndLevelByVoltage(float targetVoltage, uint16_t& outSpi, uint8_t& outLevel);

    // 交互式测试入口
    void interactiveVoltageTest();

} // namespace ApdVoltageConfig

#endif // APD_VOLTAGE_TABLE_H
