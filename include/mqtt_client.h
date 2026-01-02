#pragma once

#include <mosquitto.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

class MqttClient {
    public:
        MqttClient(std::string host, int port, std::string client_id, int qos);
        ~MqttClient();

        MqttClient(const MqttClient&) = delete;
        MqttClient& operator = (const MqttClient&) = delete;
        
        bool connect(int keepalive_seconds = 60);
        void tick(); // pulse (non-blocking reconnect attempts)
        std::uint64_t reconnects() const noexcept { return reconnects_.load(std::memory_order_relaxed); }
        
        bool publish(std::string_view topic, std::string_view payload, int qos = 0, bool retain = false);

        void stop() noexcept;

        const std::string& client_id() const { return client_id_; }

    private:
        // common variables
        std::string host_;
        int port_;
        std::string client_id_;
        mosquitto* mosq_ = nullptr;

        // connection
        std::atomic<bool> connected_ {false};
        std::atomic<bool> stopping_ {false};
        std::atomic<bool> loop_started_ {false};
        std::mutex reconnect_mtx_;

        static void on_connect(struct mosquitto* mosq, void* obj, int rc);
        static void on_disconnect(struct mosquitto* mosq, void* obj, int rc);
        bool ensure_connected();

        // reconnect
        std::chrono::steady_clock::time_point next_reconnect_ {};
        int backoff_seconds_ = 1;
        static constexpr int kMaxBackoffSeconds = 30;
        std::atomic<uint64_t> reconnects_{0};

        void tick_reconnect_();

        // status/LWT
        std::string status_topic_;
        std::string will_payload_; // offline
        std::string online_payload_; // online
        int qos_;

        void setup_lwt_();
        void publish_status_(const std::string& payload);
};