
# --- Builder Stage ---
FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-22.04 AS builder

# Install build dependencies
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends cmake ninja-build

WORKDIR /app

# Copy vcpkg manifest and install dependencies
COPY vcpkg.json .
RUN vcpkg install

# Copy source code
COPY . .

# Configure and build the project
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build --config Release

# --- Runtime Stage ---
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies (OpenSSL for cpr/https)
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends libssl-dev ca-certificates \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled binary from the builder stage
COPY --from=builder /app/build/tg_gateway .

# Set the entrypoint
ENTRYPOINT ["./tg_gateway"]
