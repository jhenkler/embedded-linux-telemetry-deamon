#pragma once

#include <string>
#include <string_view>


inline constexpr std::string_view Devices = "devices";

[[nodiscard]]
inline std::string make_topic(std::string_view client_id, std::string_view metric) {
    
    std::string str;
    str.reserve(Devices.size() + 1 + client_id.size() + 1 + metric.size());
    str.append(Devices);
    str.push_back('/');
    str.append(client_id);
    str.push_back('/');
    str.append(metric);
    return str; 
}

[[nodiscard]]
inline std::string make_status_topic(std::string_view client_id) {
    return make_topic(client_id, "status");
}