
cmake_minimum_required(VERSION 3.16)

project(PortfolioService LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)  # Changed from 20 to 17 as per requirements
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror -fPIC)
endif()

# Find dependencies
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(redis-plus-plus CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(date CONFIG REQUIRED)

# Add source files - your approach is perfect!
file(GLOB_RECURSE SRC_FILES "src/*.cpp")

# Add executable
add_executable(portfolio_service ${SRC_FILES})

# Link libraries
target_link_libraries(portfolio_service PRIVATE
    nlohmann_json::nlohmann_json
    cpr::cpr
    redis-plus-plus::redis-plus-plus
    libpqxx::pqxx
    fmt::fmt
    spdlog::spdlog
    httplib::httplib
    date::date
)

# Set include directories
target_include_directories(portfolio_service PRIVATE src)

# Install step (optional, for packaging)
install(TARGETS portfolio_service DESTINATION bin)
