# SimulIDE headless build for CI use.
# Builds the patched SimulIDE (headless mode + socket API) from source on
# Ubuntu 22.04 and runs it with the Qt "offscreen" platform plugin, so no
# X11 / Xvfb is required in CI.
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

# NOTE: must not be named /src — SimulIDE.pro's INCLUDEPATH uses "../src",
# and qmake resolves that relative to the .pro file's own directory when
# checking existence; a workdir literally called /src makes "../src"
# canonicalize back to /src itself instead of /src/src (silent wrong
# include path, breaks the build in a very confusing way).
WORKDIR /build
COPY src ./src
COPY resources ./resources
COPY SimulIDE.pro ./SimulIDE.pro

RUN mkdir -p build_XX && cd build_XX && \
    qmake ../SimulIDE.pro -spec linux-g++ CONFIG+=release BUILD_DIR=.. && \
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

COPY --from=build /build/executables/SimulIDE_1.1.0-SR2 /opt/simulide
COPY tests /opt/simulide-tests

ENV QT_QPA_PLATFORM=offscreen
ENTRYPOINT ["/opt/simulide/simulide", "--headless"]
