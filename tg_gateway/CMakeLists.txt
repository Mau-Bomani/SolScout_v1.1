
cmake_minimum_required(VERSION 3.20)
project(tg_gateway CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(spdlog 1.5.0 REQUIRED)
find_package(cpr 1.6.2 REQUIRED)
find_package(sw-redis-plusplus 1.3.3 REQUIRED)
find_package(fmt 8.0.0 REQUIRED)

add_executable(tg_gateway
    src/main.cpp
    src/config.cpp
    src/telegram_client.cpp
    src/webhook_server.cpp
    src/poller.cpp
    src/redis_bus.cpp
    src/parser.cpp
    src/auth.cpp
    src/rate_limiter.cpp
    src/health.cpp
    src/util.cpp
    src/json_schemas.cpp
)

target_include_directories(tg_gateway PUBLIC src)

target_link_libraries(tg_gateway PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    cpr::cpr
    sw::redis++::redis++
    fmt::fmt
)
