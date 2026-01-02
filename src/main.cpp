#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>
#include <memory>
#include <string>
#include <unistd.h>
#include <chrono>
#include <stdexcept>
#include <iostream>

#include <mosquitto.h>

#include "app_config.h"
#include "logger.h"
#include "mqtt_client.h"
#include "telemetry_payload.h"
#include "topic_builder.h"
#include "health_payload.h"
#include "sensor_factory.h"
#include "simulated_sensor.h"
#include "version.h"

static std::atomic<bool> g_running{true};
static void handle_signal(int) { g_running.store(false, std::memory_order_relaxed); }

namespace {

    enum class CliAction {
        Run,
        PrintVersion,
        PrintConfig
    };

    struct CliOptions {
        CliAction action = CliAction::Run;
        std::string config_path = "config/config.json";
    };

    CliOptions parse_cli(int argc, char** argv) {
        CliOptions opts;

        for (int i = 0; i < argc; ++i) {
            const std::string arg = argv[i];
            
            if (arg == "--version") {
                opts.action = CliAction::PrintVersion;
                return opts;
            }

            if (arg == "print-config") {
                opts.action = CliAction::PrintConfig;
                if (i + 1 < argc) {
                    opts.config_path = argv[++i];
                }
                return opts;
            }

            if (!arg.empty() && arg[0] != '-') {
                opts.config_path = arg;
            }
        }

        return opts;
    };

    void print_config(const AppConfig& cfg) {
        nlohmann::json out;
        out["log_level"] = cfg.log_level;
        out["client_id"] = cfg.client_id;
        out["interval_ms"] = cfg.interval_ms;
        out["qos"] = cfg.qos;
        out["retain"] = cfg.retain;
        out["broker"] = {
            {"host", cfg.host},
            {"port", cfg.port},
            {"keepalive_s", cfg.keepalive_s}
        };

        for (const auto& m : cfg.metrics) {
            out["metrics"].push_back({
                {"name", m.name},
                {"unit", m.unit},
                {"type", m.type},
                {"topic_suffix", m.topic_suffix}
            });
        }
        std::cout << out.dump(2) << "\n";
    }

    struct MosquittoLibGuard {
        MosquittoLibGuard() { mosquitto_lib_init(); }
        ~MosquittoLibGuard() { mosquitto_lib_cleanup(); }
        MosquittoLibGuard(const MosquittoLibGuard&) = delete;
        MosquittoLibGuard& operator=(const MosquittoLibGuard&) = delete;
    };

    struct SensorEntry {
        std::string topic;
        std::unique_ptr<ISensor> sensor;
    };

    struct AppState {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        std::uint64_t publish_ok = 0;
        std::uint64_t publish_fail = 0;

        std::uint64_t uptime_s() const {
            return (std::uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start
            ).count();
        }
    };

    AppConfig load_config(const CliOptions& cli) {
        LOG_INFO("Reading config file (log level will be applied after load)");
        return load_config_or_throw(cli.config_path);
    }

    void configure_logging_from_config(const AppConfig& cfg) {
        logger::Level lvl = logger::Level::Info;
        if(!logger::try_parse_level(cfg.log_level, lvl)) {
            LOG_WARN("Invalid log_level '" + cfg.log_level + "'. Using 'info'.");
            lvl = logger::Level::Info;
        }
        logger::set_level(lvl);
        LOG_INFO("Config loaded successfully");
        LOG_INFO("log level is: " + std::string(logger::level_str(lvl)));
    }

    std::vector<SensorEntry> build_sensors(const AppConfig& cfg) {
        std::vector<SensorEntry> sensors;
        sensors.reserve(cfg.metrics.size());

        for (const auto& metric : cfg.metrics) {
            auto sensor = make_sensor(metric); 
            if (!sensor || !sensor->init()) {
                throw std::runtime_error("Sensor init failed: " + std::string(sensor ? sensor->name() : "null"));
            }
            sensors.push_back(SensorEntry {
                make_topic(cfg.client_id, metric.topic_suffix),
                std::move(sensor)
            });
        }
        return sensors;
    }

    void log_config_summary(const AppConfig& cfg) {
        LOG_INFO("Client ID: " + cfg.client_id);
        LOG_INFO("Broker: " + cfg.host + ":" +std::to_string(cfg.port));
        LOG_INFO("Interval ms: " + std::to_string(cfg.interval_ms));
        LOG_INFO("Metrics: " + std::to_string(cfg.metrics.size()) + " metrics");
    }

    void publish_health(MqttClient& mqtt, 
                        std::string health_topic,
                        const AppConfig& cfg, 
                        const AppState& state, 
                        std::uint64_t seq) {
        const auto now_s = unix_time_s();
        auto health_payload = make_health_payload_v1(
            cfg.client_id,
            state.uptime_s(),
            seq,
            state.publish_ok,
            state.publish_fail,
            mqtt.reconnects(),
            now_s
        );
        (void)mqtt.publish(health_topic, health_payload.dump(), /*qos*/ 1, /*retain*/ true);
    }

    int run_loop(MqttClient& mqtt, const AppConfig& cfg, std::vector<SensorEntry>& sensors) {
        AppState state;
        std::uint64_t seq = 0;
        constexpr std::uint64_t health_every = 5;
        const std::string health_topic = make_health_topic(cfg.client_id);

        while (g_running.load(std::memory_order_relaxed)) {
            mqtt.tick();

            for (auto& entry : sensors) {
                auto reading = entry.sensor->sample();
                if (!reading) continue;

                auto payload = make_payload_v1(cfg.client_id,
                                            reading->metric_name,
                                            reading->unit,
                                            reading->value,
                                            seq);
                
                const bool ok = mqtt.publish(entry.topic, payload.dump(), cfg.qos, cfg.retain);
                if (ok) ++state.publish_ok;
                else { ++state.publish_fail; LOG_DEBUG("Failed to publish topic: " + entry.topic); }
            }

            if ((seq % health_every) == 0) publish_health(mqtt, health_topic, cfg, state, seq);
            ++seq;
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg.interval_ms));
        }
        return EXIT_SUCCESS;
    }
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    const auto cli = parse_cli(argc, argv);

    if (cli.action == CliAction::PrintVersion) {
        std::cout << TELEMETRY_DAEMON_NAME << " v" << TELEMETRY_DAEMON_VERSION << "\n";
        return EXIT_SUCCESS;
    }

    logger::set_level(logger::Level::Info); // default logging

    try {
        AppConfig cfg = load_config(cli);
        if (cli.action == CliAction::PrintConfig) {
            print_config(cfg);
            return EXIT_SUCCESS;
        }
        configure_logging_from_config(cfg);
        LOG_INFO("PID: " + std::to_string(getpid()));
        LOG_INFO("Starting embedded telemetry daemon");
        log_config_summary(cfg);

        MosquittoLibGuard mosq_guard;

        auto sensors = build_sensors(cfg);

        MqttClient mqtt(cfg.host, cfg.port, cfg.client_id, cfg.qos);
        LOG_INFO("Connecting MQTT...");
        if (!mqtt.connect(cfg.keepalive_s)) {
            LOG_ERROR("MQTT connect failed");
            return EXIT_FAILURE;
        }

        const int rc = run_loop(mqtt, cfg, sensors);

        LOG_INFO("Shutting down...");
        mqtt.stop();
        return rc;

    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Fatal error: ") + e.what());
        return EXIT_FAILURE;
    }
}