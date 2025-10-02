
cmake_minimum_required(VERSION 3.16)
project(analytics_service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Find Dependencies ---
# Using find_package for modern CMake integration
find_package(spdlog REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED) # Version check for safety
find_package(redis-plus-plus REQUIRED)
find_package(libpqxx REQUIRED)
find_package(Threads REQUIRED)

# --- Source Files ---
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/redis_bus.cpp
    src/pg_store.cpp
    src/signals.cpp
    src/scoring.cpp
    src/entry_exit.cpp
    src/regime.cpp
    src/throttles.cpp
    src/api_signals.cpp
    src/analytics_service.cpp
)

# --- Executable ---
add_executable(analytics_service ${SOURCES})

# --- Include Directories ---
# The 'src' directory contains all our headers
target_include_directories(analytics_service PRIVATE src)

# --- Link Libraries ---
# Link all required libraries to our executable
target_link_libraries(analytics_service PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    redis-plus-plus::redis-plus-plus
    pqxx::pqxx
    Threads::Threads
)

# --- Installation ---
# Optional: Define installation rules for the executable
install(TARGETS analytics_service
    DESTINATION bin
)
