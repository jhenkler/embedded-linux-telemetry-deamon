#include <simulated_sensor.h>

SimulatedSensor::SimulatedSensor(std::string metric, std::string unit, double start, double step)
    : metric_(std::move(metric)), unit_(std::move(unit)), start_(start), step_(step) {}

bool SimulatedSensor::init() { return true; }

std::optional<Reading> SimulatedSensor::sample() {
    return Reading {
        .metric_name = metric_,
        .unit = unit_,
        .value = start_ * step_ * static_cast<double>(n_++)
    };
}

std::string_view SimulatedSensor::name() const { return metric_; }