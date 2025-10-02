
cmake_minimum_required(VERSION 3.16)

project(notifier CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set vcpkg toolchain file
if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

# --- Source Files ---
# Add all the .cpp files for the notifier service
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/redis_bus.cpp
    src/audit_logger.cpp
    src/throttler.cpp
    src/deduplicator.cpp
    src/formatter.cpp
    src/notifier_service.cpp
)

# --- Include Directories ---
# Add the current source directory for headers
include_directories(src)

# Add the project's common include directory.
# This path assumes a 'common/include' directory at the project root.
include_directories(${CMAKE_SOURCE_DIR}/common/include)

# --- Find Dependencies ---
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)
find_package(PostgreSQL REQUIRED) # For libpqxx
find_package(LibPQXX REQUIRED)
find_package(Redis++ REQUIRED)    # For sw::redis++

# --- Build Executable ---
add_executable(notifier ${SOURCES})

# --- Link Libraries ---
target_link_libraries(notifier PRIVATE
    # Project-specific common library (assuming it's a target)
    common

    # Third-party libraries
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    fmt::fmt
    PQXX::pqxx
    redis++::redis++
)

# --- Installation (Optional) ---
# Defines where to install the built executable
install(TARGETS notifier
    DESTINATION bin
)

# Compiler flags for release build
target_compile_options(notifier PRIVATE
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
    -Wall -Wextra -Werror
)

# Testing with Catch2
enable_testing()
find_package(Catch2 3 REQUIRED)

set(TEST_SOURCES
    tests/test_formatter.cpp
    tests/test_throttler.cpp
    tests/test_deduplicator.cpp
    # Add other test files here as they are created
)

add_executable(notifier_tests ${TEST_SOURCES} src/formatter.cpp src/throttler.cpp src/deduplicator.cpp src/config.cpp src/types.cpp src/util.cpp)
target_link_libraries(notifier_tests PRIVATE
    Catch2::Catch2WithMain
    nlohmann_json::nlohmann_json
    sw::redis-plus-plus::redis-plus-plus
    fmt::fmt
    spdlog::spdlog
)

add_test(NAME notifier_tests COMMAND notifier_tests)
