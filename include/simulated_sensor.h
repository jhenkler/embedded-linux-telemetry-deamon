#pragma once

#include <cstdint>

#include "sensor.h"

class SimulatedSensor final : public ISensor {
    public:
        SimulatedSensor(std::string metric, std::string unit, double start, double step);

        bool init() override;
        std::optional<Reading> sample() override;
        std::string_view name() const override;

    private:
    std::string metric_;
    std::string unit_;
    double start_;
    double step_;
    std::uint64_t n_ = 0;
};