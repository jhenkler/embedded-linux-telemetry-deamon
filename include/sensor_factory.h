#pragma once

#include <memory>

struct MetricConfig;
class ISensor;

std::unique_ptr<ISensor> make_sensor(const MetricConfig& metric);