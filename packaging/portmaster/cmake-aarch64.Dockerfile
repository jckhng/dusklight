FROM debian:bookworm-slim

ARG DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture arm64 \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        clang \
        curl \
        file \
        g++ \
        gcc \
        git \
        make \
        ninja-build \
        patch \
        pkg-config \
        python3 \
        unzip \
        xz-utils \
        zip \
        cmake \
        g++-aarch64-linux-gnu \
        gcc-aarch64-linux-gnu \
        libc6-dev-arm64-cross \
        libasound2-dev:arm64 \
        libdbus-1-dev:arm64 \
        libegl-dev:arm64 \
        libgles-dev:arm64 \
        libpulse-dev:arm64 \
        libudev-dev:arm64 \
        libwayland-dev:arm64 \
        libx11-dev:arm64 \
        libxcursor-dev:arm64 \
        libxext-dev:arm64 \
        libxfixes-dev:arm64 \
        libxi-dev:arm64 \
        libxinerama-dev:arm64 \
        libxkbcommon-dev:arm64 \
        libxrandr-dev:arm64 \
        zlib1g-dev:arm64 \
    && rm -rf /var/lib/apt/lists/*

RUN curl --proto '=https' --tlsv1.2 -fsSL https://sh.rustup.rs \
        | sh -s -- -y --profile minimal --default-toolchain stable \
    && /root/.cargo/bin/rustup target add aarch64-unknown-linux-gnu

ENV PATH=/root/.cargo/bin:$PATH
ENV PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
ENV PKG_CONFIG_SYSROOT_DIR=/
ENV CC_aarch64_unknown_linux_gnu=aarch64-linux-gnu-gcc
ENV CXX_aarch64_unknown_linux_gnu=aarch64-linux-gnu-g++
ENV AR_aarch64_unknown_linux_gnu=aarch64-linux-gnu-ar
ENV CARGO_TARGET_AARCH64_UNKNOWN_LINUX_GNU_LINKER=aarch64-linux-gnu-gcc

WORKDIR /work

CMD ["bash", "-lc", "rm -rf /work/build/portmaster-aarch64 && cmake -S /work -B /work/build/portmaster-aarch64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=/work/packaging/portmaster/aarch64-linux-gnu.toolchain.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/work/build/portmaster-aarch64/install -DBUILD_SHARED_LIBS=OFF -DDUSK_VERSION_OVERRIDE=v0.0.0-portmaster -DDUSK_ENABLE_UPDATE_CHECKER=OFF -DDUSK_ENABLE_DISCORD=OFF -DDUSK_ENABLE_SENTRY_NATIVE=OFF -DAURORA_DAWN_PROVIDER=vendor -DAURORA_DAWN_LINKAGE=shared -DDAWN_ENABLE_VULKAN=OFF -DDAWN_ENABLE_NULL=OFF -DDAWN_ENABLE_DESKTOP_GL=OFF -DDAWN_ENABLE_OPENGLES=ON -DDAWN_USE_GLFW=OFF -DDAWN_USE_X11=OFF -DDAWN_BUILD_TESTS=OFF -DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_BENCHMARKS=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_CMD_TOOLS=OFF -DTINT_BUILD_BENCHMARKS=OFF -DTINT_BUILD_NULL_WRITER=ON -DAURORA_SDL3_PROVIDER=vendor -DAURORA_SDL3_LINKAGE=shared -DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF -DAURORA_NOD_PROVIDER=vendor -DAURORA_NOD_LINKAGE=shared -DRust_CARGO_TARGET=aarch64-unknown-linux-gnu -DRust_RUSTUP_INSTALL_MISSING_TARGET=OFF && git -C /work/build/portmaster-aarch64/_deps/dawn-src apply /work/packaging/portmaster/patches/dawn-abseil-no-std-source-location.patch && chmod u+w /work/build/portmaster-aarch64/_deps/dawn-src/src/dawn/native/Surface.cpp /work/build/portmaster-aarch64/_deps/dawn-src/src/dawn/native/opengl/SwapChainEGL.cpp && cd /work/build/portmaster-aarch64/_deps/dawn-src && patch -p1 < /work/packaging/portmaster/patches/dawn-portmaster-fbdev-surface.patch && cmake --build /work/build/portmaster-aarch64 --target dusklight -j$(nproc) && cmake --install /work/build/portmaster-aarch64 && file /work/build/portmaster-aarch64/Binaries/dusklight"]
