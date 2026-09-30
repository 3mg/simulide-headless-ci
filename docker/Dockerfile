# SimulIDE headless build for CI use.
# Builds the patched SimulIDE (headless mode + socket API) from source on
# Ubuntu 22.04 and runs it with the Qt "offscreen" platform plugin, so no
# X11 / Xvfb is required in CI.
#
# Build from the repo root:
#   docker build -f docker/Dockerfile -t simulide-headless .
FROM ubuntu:22.04 AS build

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    qtbase5-dev \
    qt5-qmake \
    qtmultimedia5-dev \
    qttools5-dev-tools \
    libqt5svg5-dev \
    libqt5serialport5-dev \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY src ./src
COPY resources ./resources
COPY SimulIDE.pri ./SimulIDE.pri
COPY build_XX/SimulIDE_Build.pro ./build_XX/SimulIDE_Build.pro

RUN cd build_XX && \
    qmake SimulIDE_Build.pro -spec linux-g++ CONFIG+=release && \
    make -j"$(nproc)"

FROM ubuntu:22.04 AS runtime

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libqt5widgets5 \
    libqt5svg5 \
    libqt5multimedia5 \
    libqt5serialport5 \
    libqt5network5 \
    libqt5concurrent5 \
    libqt5xml5 \
    python3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /build/build_XX/executables/SimulIDE_2.0.0-/ /opt/simulide/
COPY tests /opt/simulide-tests

ENV QT_QPA_PLATFORM=offscreen
ENTRYPOINT ["/opt/simulide/simulide", "-nogui-ci"]
