#include <memory>

#include "app_config.h"
#include "simulated_sensor.h"
#include "sensor.h"
#include "logger.h"

std::unique_ptr<ISensor> make_sensor(const MetricConfig& metric) {
    if (metric.type == "simulated") {
        return std::make_unique<SimulatedSensor>(metric.name, metric.unit, metric.start, metric.step);
    }

    // Future: if (metric.type == "device_name") return std::make_unique<deviceSensor>(...)

    LOG_WARN("Unkown sensor type: " + metric.type + " (falling back to simulated)");
    return std::make_unique<SimulatedSensor>(metric.name, metric.unit, metric.start, metric.step);
}