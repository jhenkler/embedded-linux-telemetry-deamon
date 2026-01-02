# Embedded Linux Telemetry daemon

A production-style embedded Linux telemetry daemon written in C++ that publishes structured, versioned sensor data over MQTT.
Designed to model real-world device behavior including resilient reconnect logic, runtime configuration, graceful shutdown, and device presence tracking via MQTT Last Will & Testament (LWT).

## Overview

This project implements a long-running Linux service that periodically samples one or more sensors and publishes telemetry to an MQTT broker. The daemon is designed to behave like a real embedded/IoT device rather than a one-off publisher or demo script.

Key design goals:

* Non-blocking operation
* Resilience to network and broker failures
* Runtime configurability without recompilation
* Clean startup/shutdown semantics
* Production-style deployment via systemd

## Features

* Asynchronous MQTT client (libmosquitto)
* Non-blocking reconnect logic with exponential backoff
* Structured, versioned JSON telemetry payloads
* Runtime configuration via JSON (broker, metrics, QoS, intervals, logging)
* Multiple metric support with independent topics
* Graceful shutdown via SIGINT / SIGTERM
* Device presence tracking using MQTT Last Will & Testament (LWT)
  - Retained online status on connect
  - Retained offline status on crash or power loss
  - Retained offline status on clean shutdown
* Thread-safe logging with runtime-configurable log levels
* systemd service unit with basic hardening
* Docker-hosted MQTT broker for local testing

> Note: `--print-config` parses and prints the effective configuration (defaults applied) and exits without starting MQTT.

## Architecture

The project is intentionally structured with clear separation of concerns:
* Config: JSON parsing + validation ('AppConfig')
* Transport: MQTT connection management + publishing ('MqttClient')
* Schema: Telemetry / status payload formats (versioned)
* Sensors: Pluggable sensor interface ('ISensor') with simulated sensors included
* Application: Main loop + lifecycle management (signals, systemd-friendly behavior)

This structure allows real hardware sensors to be added later with minimal changes.

### Topic Layout

* Telemetry:
    - 'devices/<client_id>/<topic_suffix>'
* Device status (LWT):
    - 'devices/<client_id>/status'
* Health/heartbeat:
    - 'devices/<client_id>/health'

## Build Instructions

### Dependencies

* C++20 compiler
* CMake ≥ 3.16
* libmosquitto (runtime + development)
* nlohmann/json

On Debian/Ubuntu:
```bash
sudo apt install libmosquitto-dev libmosquitto1 nlohmann-json3-dev
```

### Build
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Local MQTT Broker (Docker)
```bash
docker run -d --name mqtt -p 1883:1883 -p 9001:9001 eclipse-mosquitto:2
```

## Configuration

The daemon is configured via a JSON file passed at runtime.

Example config.json
```json
{
    "log_level": "info", 
    "broker": {
        "host": "localhost",
        "port": 1883,
        "keepalive_s": 10
    },
    "client_id": "pi-sim-01",
    "interval_ms": 1000,
    "qos": 1,
    "retain": false,
    "metrics": [
        { "name": "temperature", "unit": "C", "start": 20.0, "step": 0.25, "topic_suffix": "temp"},
        { "name": "humidity", "unit": "%", "start": 45.0, "step": 0.5, "topic_suffix": "humidity"}
    ]
}
```

## Running the daemon
```bash
./embedded-linux-telemetry-daemon config/config.json
```

To observe telemetry:
```bash
mosquitto_sub -h localhost -p 1883 -t "devices/#" -v
```

## Example Usage
```bash
# Show version
./embedded-linux-telemetry-daemon --version

# Validate config
./embedded-linux-telemetry-daemon --print-config /etc/embedded-linux-telemetry-daemon/config.json

# Normal run
./embedded-linux-telemetry-daemon /etc/embedded-linux-telemetry-daemon/config.json
```

## Device Presence (LWT)

The daemon publishes device presence using MQTT retained messages.

### Status Topic
```bash
devices/<client_id>/status
```

### Behavior
* On successful connection: publishes online (retained)
* On crash / power loss: broker publishes offline via LWT
* On clean shutdown: daemon publishes offline before disconnecting

### Testing LWT
```bash
mosquitto_sub -h localhost -p 1883 -t "devices/+/status" -v
```

Kill the daemon ungracefully:
```bash
kill -9 <pid>
```
You should see a retained offline message.

### systemd Integration
A sample systemd unit file is provided.

### Install
```bash
sudo cp systemd/embedded-linux-telemetry-daemon.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now embedded-linux-telemetry-daemon
```
### View Logs
```bash
journalctl -u embedded-linux-telemetry-daemon -f
```
The service includes basic hardening options such as NoNewPrivileges, PrivateTmp, and ProtectSystem.

## Why This Project

This project was built to demonstrate:
* Embedded Linux service design
* Real-world MQTT usage patterns
* Fault-tolerant networked software
* Clean C++ architecture for long-running systems
* Operational awareness (logging, systemd, lifecycle management)

It intentionally avoids UI, databases, or cloud dependencies to focus on core embedded and systems engineering concerns.

## Future Extensions (Out of Scope)
Possible follow-on projects:
* Real hardware sensors (I2C/SPI)
* TLS-secured MQTT
* Metrics aggregation service
* Embedded build integration (Yocto)

These are intentionally left out to keep this project focused and production-shaped.
