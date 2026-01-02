#include <string>
#include <mosquitto.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <algorithm>

#include "mqtt_client.h"
#include "logger.h"
#include "topic_builder.h"
#include "status_payload.h"

MqttClient::MqttClient(std::string host, int port, std::string client_id, int qos) 
    : host_(std::move(host)), port_(port), client_id_(std::move(client_id)), qos_(qos) {

        // clean_session=true, userdata=this
        mosq_ = mosquitto_new(client_id_.c_str(), true, this);
        if (!mosq_) {
            LOG_ERROR("mosquitto_new failed");
            return;
        }

        // Call backs
        mosquitto_connect_callback_set(mosq_, &MqttClient::on_connect);
        mosquitto_disconnect_callback_set(mosq_, &MqttClient::on_disconnect);

        // LWT
        setup_lwt_();
    }

MqttClient::~MqttClient() {
    stop();
    if (mosq_) {
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
    }
}

void MqttClient::on_connect(struct mosquitto* /*mosq*/, void* obj, int rc) {
    auto* self = static_cast<MqttClient*>(obj);

    if (rc == 0) {
        self->connected_.store(true, std::memory_order_relaxed);
        self->backoff_seconds_ = 1;
        self->next_reconnect_ = {};
        LOG_INFO("Connected to broker");

        // mark online (retained)
        self->publish_status_(self->online_payload_);
    } else {
        self->connected_.store(false, std::memory_order_relaxed);
        LOG_ERROR("Connect failed rc=" + std::to_string(rc));
    }
}

void MqttClient::on_disconnect(struct mosquitto* /*mosq*/, void* obj, int rc) {
    auto* self = static_cast<MqttClient*>(obj);
    self->connected_.store(false, std::memory_order_relaxed);

    if (self->stopping_.load(std::memory_order_relaxed)) {
        LOG_INFO("Disconnected cleanly rc=" + std::to_string(rc));
    } else {
        LOG_WARN("Disconnect rc=" + std::to_string(rc) + " (will reconnect)");
    }
}

bool MqttClient::connect(int keepalive_seconds) {
    if (!mosq_) return false;

    int rc = mosquitto_connect_async(mosq_, host_.c_str(), port_, keepalive_seconds);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR(std::string("mosquitto_connect_async error: ") + mosquitto_strerror(rc));
        return false;
    }

    bool expected = false;
    if (loop_started_.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        rc = mosquitto_loop_start(mosq_);
        if ( rc != MOSQ_ERR_SUCCESS) {
            LOG_ERROR(std::string("mosquitto_loop_start error: ") + mosquitto_strerror(rc));
            loop_started_.store(false, std::memory_order_relaxed);
            return false;
        }
    }

    return true;
}

void MqttClient::tick() { tick_reconnect_(); }

void MqttClient::tick_reconnect_() {
    if (stopping_.load(std::memory_order_relaxed)) return;
    if (connected_.load(std::memory_order_relaxed)) return;
    if (!mosq_) return;

    const auto now = std::chrono::steady_clock::now();

    if (next_reconnect_.time_since_epoch().count() == 0) {
        next_reconnect_ = now;
    }

    if (now < next_reconnect_) return;

    if (!reconnect_mtx_.try_lock()) return;
    std::lock_guard<std::mutex> lock(reconnect_mtx_, std::adopt_lock);

    if (connected_.load(std::memory_order_relaxed)) {
        backoff_seconds_ = 1;
        next_reconnect_ = {};
        return;
    }

    int rc = mosquitto_reconnect_async(mosq_);
    if (rc == MOSQ_ERR_SUCCESS) {
        reconnects_.fetch_add(1, std::memory_order_relaxed);
        // schedule next attempt incase it fails
        next_reconnect_ = now + std::chrono::seconds(backoff_seconds_);
        backoff_seconds_ = std::min(backoff_seconds_ * 2, kMaxBackoffSeconds);
    } else {
        LOG_ERROR(std::string("reconnect_async error: ") + mosquitto_strerror(rc));
        next_reconnect_ = now + std::chrono::seconds(backoff_seconds_);
        backoff_seconds_ = std::min(backoff_seconds_ * 2, kMaxBackoffSeconds);
    }
}

bool MqttClient::ensure_connected() {
    if (connected_.load(std::memory_order_relaxed)) return true;
    tick_reconnect_();
    return connected_.load(std::memory_order_relaxed);
}

bool MqttClient::publish(std::string_view topic, std::string_view payload, int qos, bool retain) {
    if (!ensure_connected()) return false;

    int payload_len = static_cast<int>(payload.size());
    std::string topic_str(topic);
    int rc = mosquitto_publish(
        mosq_,
        nullptr,
        topic_str.c_str(),
        payload_len,
        payload.data(),
        qos,
        retain
    );

    if (rc == MOSQ_ERR_NO_CONN) {
        connected_.store(false, std::memory_order_relaxed);
        tick_reconnect_();
        return false;
    }

    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR(std::string("mosquitto publish error: ") + mosquitto_strerror(rc));
        return false;
    }
    return true;
}

void MqttClient::stop() noexcept {
    if (stopping_.exchange(true, std::memory_order_relaxed)) return;
    if (!mosq_) return;

    // mark offline (retained)
    publish_status_(will_payload_);

    mosquitto_disconnect(mosq_);

    if (loop_started_.load(std::memory_order_relaxed)) {
        mosquitto_loop_stop(mosq_, true);
        loop_started_.store(false, std::memory_order_relaxed);
    }
}

// status/LWT
void MqttClient::setup_lwt_() {
    status_topic_ = make_status_topic(client_id_);

    will_payload_   = make_status_payload_v1(client_id_, "offline").dump();
    online_payload_ = make_status_payload_v1(client_id_, "online").dump();

    const bool retain = true;

    int rc = mosquitto_will_set(
        mosq_,
        status_topic_.c_str(),
        static_cast<int>(will_payload_.size()),
        will_payload_.data(),
        qos_,
        retain
    );

    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_WARN(std::string("mosquitto_will_set failed: ") + mosquitto_strerror(rc));
    }
}

void MqttClient::publish_status_(const std::string& payload) {
    if (!mosq_) return;
    if (!connected_.load(std::memory_order_relaxed)) return;
    const bool retain = true;

    int rc = mosquitto_publish(
        mosq_,
        nullptr,
        status_topic_.c_str(),
        static_cast<int>(payload.size()),
        payload.data(),
        qos_,
        retain
    );

    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_DEBUG(std::string("status publish failed: ") + mosquitto_strerror(rc));
    }
}
