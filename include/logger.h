#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <array>

namespace logger {

    enum class Level : int { Debug = 0, Info = 1, Warn = 2, Error = 3, Off = 4 };

    struct LevelEntry {
        std::string_view name;
        Level lvl;
    };

    static constexpr std::array<LevelEntry, 5> level_table{{
        {"debug", Level::Debug},
        {"info", Level::Info},
        {"warn", Level::Warn},
        {"error", Level::Error},
        {"off", Level::Off},
    }};

    inline std::string_view level_str(Level lvl) {
        for (const auto& lvl_entry : level_table) {
            if (lvl == lvl_entry.lvl) return lvl_entry.name;
        }
        return "INFO";
    }

    inline std::atomic<Level>& current_level() {
        static std::atomic<Level> lvl{Level::Info};
        return lvl;
    }

    inline void set_level(Level lvl) {
        current_level().store(lvl, std::memory_order_relaxed);
    }

    inline bool enabled(Level msg_level) {
        return static_cast<int>(msg_level) >= static_cast<int>(current_level().load(std::memory_order_relaxed)) &&
            current_level().load(std::memory_order_relaxed) != Level::Off;
    }

    inline std::mutex& mutex() {
        static std::mutex m;
        return m;
    }

    inline std::string timestamp() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);

        std::tm tm{};

        #if defined(_WIN32) 
            localtime_s(&tm, &t);
        #else 
            localtime_r(&t, &tm);
        #endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    inline void write(Level lvl, std::string_view file, int line, std::string_view msg) {
        if (!enabled(lvl)) return;
        std::lock_guard<std::mutex> lock(mutex());
        std::cerr << timestamp()
            << " [" << level_str(lvl) << "] "
            << file << ":" << line << " - "
            << msg << '\n'; 
    }

    inline Level parse_level(std::string_view str) {
        for (const auto& lvl_entry : level_table) {
            if (str == lvl_entry.name) return lvl_entry.lvl;
        }
        return Level::Info; // default
    }

    inline bool try_parse_level(std::string_view str, Level& out) {
        for (const auto& lvl_entry : level_table) {
            if (str == lvl_entry.name) {out = lvl_entry.lvl; return true; }
        }
        return false;
    }

} // namespace logger

// ---- Convenience macros ----
#define LOG_DEBUG(msg) ::logger::write(::logger::Level::Debug, __FILE__, __LINE__, (msg))
#define LOG_INFO(msg) ::logger::write(::logger::Level::Info, __FILE__, __LINE__, (msg))
#define LOG_WARN(msg) ::logger::write(::logger::Level::Warn, __FILE__, __LINE__, (msg))
#define LOG_ERROR(msg) ::logger::write(::logger::Level::Error, __FILE__, __LINE__, (msg))