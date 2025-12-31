#pragma once

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

struct MetricConfig {
    std::string name;
    std::string unit;
    double start = 0.0;
    double step = 0.0;
    std::string topic_suffix;
};

struct AppConfig {
    std::string log_level = "info";
    std::string host = "localhost";
    int port = 1883;
    int keepalive_s = 60;

    std::string client_id = "pi-sim-01";
    int interval_ms = 100;

    int qos = 1;
    bool retain = false;

    std::vector<MetricConfig> metrics;
};

inline AppConfig load_config_or_throw(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Failed to open config: " + path);

    nlohmann::json jsn;
    file >> jsn;

    AppConfig cfg;

    cfg.log_level = jsn.value("log_level", cfg.log_level);
    if (jsn.contains("broker")) {
        const auto& broker = jsn.at("broker");
        cfg.host = broker.value("host", cfg.host);
        cfg.port = broker.value("port", cfg.port);
        cfg.keepalive_s = broker.value("keepalive_s", cfg.keepalive_s);
    }
    cfg.client_id = jsn.value("client_id", cfg.client_id);
    cfg.interval_ms = jsn.value("interval_ms", cfg.interval_ms);
    cfg.qos = jsn.value("qos", cfg.qos);
    cfg.retain = jsn.value("retain", cfg.retain);
    
    if (!jsn.contains("metrics") || !jsn.at("metrics").is_array() || jsn.at("metrics").empty()) {
        throw std::runtime_error("Config must contain non-empty metrics array");
    }

    // validate Appconfig
    // cfg.log_level is checked in main to avoid intertwining app_config.h and logger.h
    if (cfg.client_id.empty()) throw std::runtime_error("client_id must not be empty");
    if (cfg.interval_ms <= 0) throw std::runtime_error("interval_ms must be > 0");
    if (cfg.qos < 0 || cfg.qos > 2) throw std::runtime_error("qos must be 0, 1, or 2");

    for (const auto& metric : jsn.at("metrics")) {
        MetricConfig metric_cfg;

        metric_cfg.name = metric.at("name").get<std::string>();
        metric_cfg.unit = metric.value("unit", "");
        metric_cfg.start = metric.value("start", 0.0);
        metric_cfg.step = metric.value("step", 0.0);
        metric_cfg.topic_suffix = metric.at("topic_suffix").get<std::string>();

        // validate metric
        if (metric_cfg.name.empty()) throw std::runtime_error("metric name must not be empty");
        if (metric_cfg.topic_suffix.empty()) throw std::runtime_error("topic_suffix must not be empty");

        cfg.metrics.push_back(std::move(metric_cfg));
    }


    return cfg;
}