FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture arm64 \
    && sed -i -e 's|deb http://archive.ubuntu.com/ubuntu/ focal|deb [arch=amd64] http://archive.ubuntu.com/ubuntu/ focal|g' \
              -e 's|deb http://security.ubuntu.com/ubuntu/ focal|deb [arch=amd64] http://security.ubuntu.com/ubuntu/ focal|g' \
              /etc/apt/sources.list \
    && printf '%s\n' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ focal main restricted universe multiverse' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ focal-updates main restricted universe multiverse' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ focal-security main restricted universe multiverse' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ focal-backports main restricted universe multiverse' \
        > /etc/apt/sources.list.d/arm64.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
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
        python3-pip \
        unzip \
        xz-utils \
        zip \
        g++-10-aarch64-linux-gnu \
        gcc-10-aarch64-linux-gnu \
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
    && python3 -m pip install --no-cache-dir "cmake>=3.25,<3.30" \
    && rm -rf /var/lib/apt/lists/*

RUN curl --proto '=https' --tlsv1.2 -fsSL https://sh.rustup.rs \
        | sh -s -- -y --profile minimal --default-toolchain stable \
    && /root/.cargo/bin/rustup target add aarch64-unknown-linux-gnu

ENV PATH=/root/.cargo/bin:/usr/local/bin:$PATH
ENV PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
ENV PKG_CONFIG_SYSROOT_DIR=/
ENV CC_aarch64_unknown_linux_gnu=aarch64-linux-gnu-gcc-10
ENV CXX_aarch64_unknown_linux_gnu=aarch64-linux-gnu-g++-10
ENV AR_aarch64_unknown_linux_gnu=aarch64-linux-gnu-ar
ENV CARGO_TARGET_AARCH64_UNKNOWN_LINUX_GNU_LINKER=aarch64-linux-gnu-gcc-10

WORKDIR /work

CMD ["bash", "-lc", "rm -rf /work/build/portmaster-aarch64-focal && cmake -S /work -B /work/build/portmaster-aarch64-focal -G Ninja -DCMAKE_TOOLCHAIN_FILE=/work/packaging/portmaster/aarch64-linux-gnu-gcc10.toolchain.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/work/build/portmaster-aarch64-focal/install -DBUILD_SHARED_LIBS=OFF -DDUSK_VERSION_OVERRIDE=v0.0.0-portmaster -DDUSK_ENABLE_UPDATE_CHECKER=OFF -DDUSK_ENABLE_DISCORD=OFF -DDUSK_ENABLE_SENTRY_NATIVE=OFF -DAURORA_DAWN_PROVIDER=vendor -DAURORA_DAWN_LINKAGE=shared -DDAWN_ENABLE_VULKAN=OFF -DDAWN_ENABLE_NULL=OFF -DDAWN_ENABLE_DESKTOP_GL=OFF -DDAWN_ENABLE_OPENGLES=ON -DDAWN_USE_GLFW=OFF -DDAWN_USE_X11=OFF -DDAWN_BUILD_TESTS=OFF -DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_BENCHMARKS=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_CMD_TOOLS=OFF -DTINT_BUILD_BENCHMARKS=OFF -DTINT_BUILD_NULL_WRITER=ON -DAURORA_SDL3_PROVIDER=vendor -DAURORA_SDL3_LINKAGE=shared -DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF -DAURORA_NOD_PROVIDER=vendor -DAURORA_NOD_LINKAGE=shared -DRust_CARGO_TARGET=aarch64-unknown-linux-gnu -DRust_RUSTUP_INSTALL_MISSING_TARGET=OFF && chmod -R u+w /work/build/portmaster-aarch64-focal/_deps/dawn-src && patch -d /work/build/portmaster-aarch64-focal/_deps/dawn-src -p1 < /work/packaging/portmaster/patches/dawn-abseil-no-std-source-location.patch && patch -d /work/build/portmaster-aarch64-focal/_deps/dawn-src -p1 < /work/packaging/portmaster/patches/dawn-gcc10-bit-cast.patch && patch -d /work/build/portmaster-aarch64-focal/_deps/dawn-src -p1 < /work/packaging/portmaster/patches/dawn-gcc10-buffer-atomic-wait.patch && patch -d /work/build/portmaster-aarch64-focal/_deps/dawn-src -p1 < /work/packaging/portmaster/patches/dawn-portmaster-fbdev-surface.patch && cmake --build /work/build/portmaster-aarch64-focal --target dusklight -j$(nproc) && cmake --install /work/build/portmaster-aarch64-focal && file /work/build/portmaster-aarch64-focal/install/dusklight"]
