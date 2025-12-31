#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>
#include <memory>
#include <string>
#include <unistd.h>

#include <mosquitto.h>

#include "app_config.h"
#include "logger.h"
#include "mqtt_client.h"
#include "telemetry_payload.h"
#include "topic_builder.h"
#include "simulated_sensor.h"

static std::atomic<bool> g_running{true};
static void handle_signal(int) { g_running.store(false, std::memory_order_relaxed); }

struct SensorEntry {
    std::string topic_suffix;
    std::unique_ptr<ISensor> sensor;
};

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    // default logging until config is read
    logger::set_level(logger::Level::Info);
    LOG_INFO("PID: " + std::to_string(getpid()));
    LOG_INFO("Starting embedded telemetry daemon");

    mosquitto_lib_init();

    AppConfig cfg;
    try {
        LOG_INFO("Reading config file (log level will be applied after load)");
        const std::string config_path = (argc > 1) ? argv[1] : "config/config.json";
        cfg = load_config_or_throw(config_path);
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Config error: ") + e.what());
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    // Check log_level
    logger::Level lvl = logger::Level::Info;
    if(!logger::try_parse_level(cfg.log_level, lvl)) {LOG_WARN("expected log_level to be one of the following: debug, info, warn, error, off. Defaulting to info");}
    logger::set_level(lvl);
    LOG_INFO("Config loaded successfully");
    LOG_INFO("log level is: " + std::string(logger::level_str(lvl)));
    LOG_INFO("Client ID: " + cfg.client_id);
    LOG_INFO("Broker: " + cfg.host + ":" +std::to_string(cfg.port));
    LOG_INFO("Interval ms: " + std::to_string(cfg.interval_ms));
    LOG_INFO("Metrics: " + std::to_string(cfg.metrics.size()) + " metrics");

    // build sensors from config metrics
    std::vector<SensorEntry> sensors;
    sensors.reserve(cfg.metrics.size());

    for (const auto& metric : cfg.metrics) {
        auto sensor = std::make_unique<SimulatedSensor>(metric.name, metric.unit, metric.start, metric.step);
        if (!sensor->init()) {
            LOG_ERROR("Sensor init failed: " + std::string(sensor->name()));
            mosquitto_lib_cleanup();
            return EXIT_FAILURE;
        }
        sensors.push_back(SensorEntry{metric.topic_suffix, std::move(sensor)});
    }

    // connect to mqtt service
    MqttClient mqtt(cfg.host, cfg.port, cfg.client_id, cfg.qos);
    LOG_INFO("Connecting MQTT...");
    if (!mqtt.connect(cfg.keepalive_s)) {
        LOG_ERROR("MQTT connect failed");
        mqtt.stop();
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    std::uint64_t seq = 0;
    while (g_running.load(std::memory_order_relaxed)) {
        mqtt.tick();

        for (auto& entry : sensors) {
            auto reading = entry.sensor->sample();
            if (!reading) continue;

            const std::string topic = make_topic(cfg.client_id, entry.topic_suffix);

            auto payload = make_payload_v1(cfg.client_id,
                                           reading->metric_name,
                                           reading->unit,
                                           reading->value,
                                           seq);
            
            const bool ok = mqtt.publish(topic, payload.dump(), cfg.qos, cfg.retain);
            if (!ok) { LOG_DEBUG("Failed to publish topic: " + topic); }
        }
        ++seq;
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.interval_ms));
    }

    LOG_INFO("Shutting down...");
    mqtt.stop();
    mosquitto_lib_cleanup();

    return EXIT_SUCCESS;
}