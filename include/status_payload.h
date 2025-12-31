#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string_view>

std::int64_t unix_time_s();

inline nlohmann::json make_status_payload_v1(std::string_view client_id, std::string_view state) {
    return {
        {"scheme_version", 1},
        {"device", {{"client_id", client_id}}},
        {"state", state},
        {"timestamp_s", unix_time_s()}
    };
}