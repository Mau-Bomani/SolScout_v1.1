
# Stage 1: Build
FROM cmake:3.22.1 as builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    curl \
    zip \
    unzip \
    pkg-config \
    ninja-build

# Clone vcpkg
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git && \
    ./vcpkg/bootstrap-vcpkg.sh

# Copy project files
WORKDIR /build
COPY . .

# Install dependencies using vcpkg
RUN /opt/vcpkg/vcpkg install --triplet=x64-linux \
    $(cat portfolio/vcpkg.json | python3 -c "import json, sys; print(' '.join(dep['name'] for dep in json.load(sys.stdin)['dependencies']))")

# Configure and build the project
RUN cmake -B . -S portfolio \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build . --config Release

# Stage 2: Runtime
FROM debian:bullseye-slim

# Install runtime dependencies (libpq for postgres)
RUN apt-get update && apt-get install -y libpq5 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the binary from the builder stage
COPY --from=builder /build/portfolio_service .

# Set the entrypoint
CMD ["./portfolio_service"]
