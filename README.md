Embedded Linux Telemetry Daemon

A production-style embedded Linux telemetry daemon written in C++ that publishes structured, versioned sensor data over MQTT.
Designed to model real-world device behavior including resilient reconnect logic, runtime configuration, graceful shutdown, and device presence tracking via MQTT Last Will & Testament (LWT).

Overview

This project implements a long-running Linux service that periodically samples one or more sensors and publishes telemetry to an MQTT broker. The daemon is designed to behave like a real embedded/IoT device rather than a one-off publisher or demo script.

Key design goals:

Non-blocking operation

Resilience to network and broker failures

Runtime configurability without recompilation

Clean startup/shutdown semantics

Production-style deployment via systemd

Features

Asynchronous MQTT client (libmosquitto)

Non-blocking reconnect logic with exponential backoff

Structured, versioned JSON telemetry payloads

Runtime configuration via JSON (broker, metrics, QoS, intervals, logging)

Multiple metric support with independent topics

Graceful shutdown via SIGINT / SIGTERM

Device presence tracking using MQTT Last Will & Testament (LWT)

Retained online status on connect

Retained offline status on crash or power loss

Thread-safe logging with runtime-configurable log levels

systemd service unit with basic hardening

Docker-hosted MQTT broker for local testing

Architecture

The project is intentionally structured with clear separation of concerns:

Config: JSON parsing and validation

Transport: MQTT connection management and publishing

Schema: Telemetry and status payload formats

Sensors: Pluggable sensor interface (simulated sensors included)

Application: Main control loop and lifecycle management

This structure allows real hardware sensors to be added later with minimal changes.

Build Instructions
Dependencies

C++20 compiler

CMake ≥ 3.16

libmosquitto (runtime + development)

nlohmann/json

On Debian/Ubuntu:

sudo apt install libmosquitto-dev libmosquitto1 nlohmann-json3-dev

Build
mkdir build
cd build
cmake ..
cmake --build .

Configuration

The daemon is configured via a JSON file passed at runtime.

Example config.json
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
    {
      "name": "temperature",
      "unit": "C",
      "start": 20.0,
      "step": 0.25,
      "topic_suffix": "temp"
    },
    {
      "name": "humidity",
      "unit": "%",
      "start": 40.0,
      "step": 0.1,
      "topic_suffix": "humidity"
    }
  ]
}

Running the Daemon
./embedded-linux-telemetry-daemon config/config.json


To observe telemetry:

mosquitto_sub -h localhost -p 1883 -t "devices/#" -v

Device Presence (LWT)

The daemon publishes device presence using MQTT retained messages.

Status Topic
devices/<client_id>/status

Behavior

On successful connection: publishes online (retained)

On crash / power loss: broker publishes offline via LWT

On clean shutdown: daemon publishes offline before disconnecting

Testing LWT
mosquitto_sub -h localhost -p 1883 -t "devices/+/status" -v


Kill the daemon ungracefully:

kill -9 <pid>


You should see a retained offline message.

systemd Integration

A sample systemd unit file is provided.

Install
sudo cp systemd/telemetry-daemon.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now telemetry-daemon

View Logs
journalctl -u telemetry-daemon -f


The service includes basic hardening options such as NoNewPrivileges, PrivateTmp, and ProtectSystem.

Why This Project

This project was built to demonstrate:

Embedded Linux service design

Real-world MQTT usage patterns

Fault-tolerant networked software

Clean C++ architecture for long-running systems

Operational awareness (logging, systemd, lifecycle management)

It intentionally avoids UI, databases, or cloud dependencies to focus on core embedded and systems engineering concerns.

Future Extensions (Out of Scope)

Possible follow-on projects:

Real hardware sensors (I2C/SPI)

TLS-secured MQTT

Metrics aggregation service

Embedded build integration (Yocto)

These are intentionally left out to keep this project focused and production-shaped.