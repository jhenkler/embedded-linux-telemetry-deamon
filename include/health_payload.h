#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string_view>

inline nlohmann::json make_health_payload_v1 (
    std::string_view client_id,
    std::uint64_t uptime_s,
    std::uint64_t seq,
    std::uint64_t publish_ok,
    std::uint64_t publish_fail,
    std::uint64_t reconnects,
    std::uint64_t now_s
) {
    return {
        {"schema_version", 1},
        {"device", {{"client_id", client_id}}},
        {"uptime_s", uptime_s},
        {"seq", seq},
        {"counters", {
            {"publish_ok", publish_ok},
            {"publish_fail", publish_fail},
            {"reconnects", reconnects},
        }},
        {"timestamp_s", now_s}
    };
}