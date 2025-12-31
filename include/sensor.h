#pragma once

#include <string>
#include <optional>

struct Reading {
    std::string metric_name;
    std::string unit;
    double value;
};

class ISensor {
    public:
        virtual ~ISensor() = default;

        virtual bool init() = 0;
        virtual std::optional<Reading> sample() = 0;
        virtual std::string_view name() const = 0;
};