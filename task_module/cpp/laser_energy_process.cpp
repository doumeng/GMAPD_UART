#include "laser_energy_process.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#include "log.h"
#include "task_reg.h"

namespace LaserEnergy {

    namespace {

        constexpr float kEnergyMinDistance = 1000.0f;
        constexpr float kEnergyMaxDistance = 3000.0f;
        constexpr float kGearThreshold0 = 50.0f;
        constexpr float kGearThreshold1 = 100.0f;
        constexpr float kGearThreshold2 = 150.0f;

        float computeEnergy(const LaserFrameShared &frame, std::size_t &validCount) {
            const std::size_t pixelCount = std::min<std::size_t>(
                    static_cast<std::size_t>(frame.rows) * static_cast<std::size_t>(frame.cols),
                    LaserFrameShared::kPixelCount);

            double intensitySum = 0.0;
            validCount = 0;

            for (std::size_t i = 0; i < pixelCount; ++i) {
                const float distance = frame.dist[i];
                if (distance < kEnergyMinDistance || distance > kEnergyMaxDistance) {
                    continue;
                }

                intensitySum += static_cast<double>(frame.inten[i]);
                ++validCount;
            }

            if (validCount == 0) {
                return 0.0f;
            }

            return static_cast<float>(intensitySum / static_cast<double>(validCount));
        }

        uint8_t mapEnergyToGear(float energy) {
            if (energy < kGearThreshold0) {
                return 0;
            }
            if (energy < kGearThreshold1) {
                return 1;
            }
            if (energy < kGearThreshold2) {
                return 2;
            }
            return 3;
        }

    } // namespace

    void thread_LaserEnergyProcess() {
        Logger::instance().info("Thread LaserEnergyProcess - Starting");

        uint64_t consumedFrames = 0;
        uint64_t gearSwitches = 0;
        uint64_t validPixelSamples = 0;
        double validPixelTotal = 0.0;
        double energyTotal = 0.0;
        uint64_t lastSeq = 0;
        auto reportStart = std::chrono::steady_clock::now();

        while (1) {
            LaserFrameShared localFrame;

            {
                std::unique_lock<std::mutex> lock(g_laserFrameMutex);
                g_laserFrameCV.wait_for(lock, std::chrono::milliseconds(100), [&] {
                    return g_laserFrameShared.valid && g_laserFrameShared.frameSeq != lastSeq;
                });

                if (!g_laserFrameShared.valid || g_laserFrameShared.frameSeq == lastSeq) {
                    continue;
                }

                localFrame = g_laserFrameShared;
                lastSeq = localFrame.frameSeq;
            }

            std::size_t validCount = 0;
            const float energy = computeEnergy(localFrame, validCount);
            g_sysConfig.laserEnergy = energy;

            ++consumedFrames;
            ++validPixelSamples;
            validPixelTotal += static_cast<double>(validCount);
            energyTotal += static_cast<double>(energy);

            if (validCount > 0) {
                const uint8_t nextGear = mapEnergyToGear(energy);
                if (nextGear != g_sysConfig.laserGear) {
                    const uint8_t oldGear = g_sysConfig.laserGear;
                    g_sysConfig.laserGear = nextGear;
                    ++gearSwitches;

                    Logger::instance().info((
                            "Thread LaserEnergyProcess - Laser gear changed: " +
                            std::to_string(static_cast<int>(oldGear)) + " -> " +
                            std::to_string(static_cast<int>(nextGear)) +
                            ", energy=" + std::to_string(energy) +
                            ", valid_pixels=" + std::to_string(validCount)).c_str());
                }
            }

            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - reportStart).count();
            if (elapsedMs >= 1000) {
                const double sec = static_cast<double>(elapsedMs) / 1000.0;
                const double fps = static_cast<double>(consumedFrames) / sec;
                const double avgValidPixels = (validPixelSamples > 0)
                                            ? (validPixelTotal / static_cast<double>(validPixelSamples))
                                            : 0.0;
                const double avgEnergy = (consumedFrames > 0)
                                        ? (energyTotal / static_cast<double>(consumedFrames))
                                        : 0.0;

                Logger::instance().debug((
                        "LaserEnergyStats "
                        "consumed=" + std::to_string(consumedFrames) +
                        ", consumed_fps=" + std::to_string(fps) +
                        ", avg_valid_pixels=" + std::to_string(avgValidPixels) +
                        ", avg_energy=" + std::to_string(avgEnergy) +
                        ", gear_switches=" + std::to_string(gearSwitches) +
                        ", current_gear=" + std::to_string(static_cast<int>(g_sysConfig.laserGear))
                ).c_str());

                reportStart = now;
                consumedFrames = 0;
                gearSwitches = 0;
                validPixelSamples = 0;
                validPixelTotal = 0.0;
                energyTotal = 0.0;
            }
        }
    }

} // namespace LaserEnergy
