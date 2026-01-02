cmake_minimum_required(VERSION 3.16)
project(embedded-linux-telemetry-daemon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(embedded-linux-telemetry-daemon
    src/main.cpp
    src/mqtt_client.cpp
    src/simulated_sensor.cpp
    src/sensor_factory.cpp
)

target_include_directories(embedded-linux-telemetry-daemon
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(embedded-linux-telemetry-daemon PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# ---- Threads ----
find_package(Threads REQUIRED)

# ---- Mosquitto via pkg-config ----
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)

# ---- nlohmann/json via CMake -> pkg-config fallback ----
find_package(nlohmann_json 3.2.0 QUIET)

if (nlohmann_json_FOUND)
    message(STATUS "Found nlohmann_json via CMake")
    target_link_libraries(embedded-linux-telemetry-daemon PRIVATE nlohmann_json::nlohmann_json)
else()
    message(STATUS "nlohmann_json Cmake package not found, trying pkg-config")
    pkg_check_modules(NLOHMANN_JSON QUIET nlohmann_json)

    if (NLOHMANN_JSON_FOUND)
        target_include_directories(embedded-linux-telemetry-daemon PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
        target_compile_options(embedded-linux-telemetry-daemon PRIVATE ${NLOHMANN_JSON_CFLAGS_OTHER})
        message(STATUS "Found nlohmann_json via pkg-config")
    else() 
        message(FATAL_ERROR "nlohmann_json not found. Install nlohmann-json3-dev or provide it manually.")
    endif()
endif()

target_link_libraries(embedded-linux-telemetry-daemon PRIVATE 
    PkgConfig::MOSQUITTO 
    Threads::Threads
)