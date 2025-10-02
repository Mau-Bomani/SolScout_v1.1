
cmake_minimum_required(VERSION 3.20)
project(soul_ingestor VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find vcpkg packages
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(redis-plus-plus CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/util.cpp
    src/market_ingestor.cpp
    src/solana_client.cpp
    src/dex_client.cpp
    src/jupiter_client.cpp
    src/database_manager.cpp
    src/redis_publisher.cpp
    src/pool_cache.cpp
    src/rate_limiter.cpp
    src/backoff_manager.cpp
    src/ohlcv_aggregator.cpp
)

set(HEADERS
    src/config.hpp
    src/util.hpp
    src/market_ingestor.hpp
    src/solana_client.hpp
    src/dex_client.hpp
    src/jupiter_client.hpp
    src/database_manager.hpp
    src/redis_publisher.hpp
    src/pool_cache.hpp
    src/rate_limiter.hpp
    src/backoff_manager.hpp
    src/ohlcv_aggregator.hpp
    src/types.hpp
)

# Main executable
add_executable(soul_ingestor ${SOURCES} ${HEADERS})

target_link_libraries(soul_ingestor PRIVATE
    nlohmann_json::nlohmann_json
    cpr::cpr
    redis++::redis++
    libpqxx::pqxx
    fmt::fmt
    spdlog::spdlog
)

target_include_directories(soul_ingestor PRIVATE src)

# Tests
add_executable(tests
    tests/test_main.cpp
    tests/test_pool_cache.cpp
    tests/test_rate_limiter.cpp
    tests/test_ohlcv_aggregator.cpp
    ${SOURCES}
)

target_link_libraries(tests PRIVATE
    Catch2::Catch2WithMain
    nlohmann_json::nlohmann_json
    cpr::cpr
    redis++::redis++
    libpqxx::pqxx
    fmt::fmt
    spdlog::spdlog
)

target_include_directories(tests PRIVATE src)
