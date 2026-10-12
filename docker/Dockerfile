# SoilWaterCM C++ Backend Dockerfile

# Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    curl \
    zip \
    unzip \
    tar \
    ninja-build \
    libssl-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/Microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh \
    && ${VCPKG_ROOT}/vcpkg integrate install

# Install vcpkg packages
RUN ${VCPKG_ROOT}/vcpkg install \
    drogon:x64-linux \
    nlohmann-json:x64-linux \
    spdlog:x64-linux \
    pqxx:x64-linux \
    mongo-cxx-driver:x64-linux \
    hiredis:x64-linux \
    jwt-cpp:x64-linux \
    gtest:x64-linux \
    benchmark:x64-linux \
    yaml-cpp:x64-linux \
    openssl:x64-linux \
    curl:x64-linux \
    zlib:x64-linux

# Copy source code
WORKDIR /build
COPY . .

# Build
RUN cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -GNinja \
    && cmake --build build --config Release

# Runtime stage
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libcurl4 \
    libpq5 \
    && rm -rf /var/lib/apt/lists/*

# Copy built binaries
COPY --from=builder /build/build/bin/soilwatercm /app/bin/soilwatercm
COPY --from=builder /build/config /app/config

# Create data directories
RUN mkdir -p /data/images /data/results /data/reports /data/temp

# Set working directory
WORKDIR /app

# Expose port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# Run
CMD ["/app/bin/soilwatercm"]
