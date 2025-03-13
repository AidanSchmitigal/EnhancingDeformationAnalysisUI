FROM python:3.10-slim AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential cmake libjpeg-dev libpng-dev python3-dev python3-pip git \
    && git clone --depth 1 --branch 4.9.0 https://github.com/opencv/opencv.git \
    && cd opencv && mkdir build && cd build \
    && cmake -D BUILD_opencv_apps=OFF -D WITH_QT=OFF -D WITH_FFMPEG=OFF .. \
    && make -j$(nproc) && make install \
    && pip3 install tensorflow-cpu==2.15.0 \
    && rm -rf /opencv /var/lib/apt/lists/*

FROM python:3.10-slim
COPY --from=builder /usr/local /usr/local
RUN apt-get update && apt-get install -y \
    build-essential cmake libjpeg-dev libpng-dev git xorg-dev libwayland-dev libxkbcommon-dev \
    wayland-protocols extra-cmake-modules wget \
    && rm -rf /var/lib/apt/lists/*
RUN wget -q --no-check-certificate https://storage.googleapis.com/tensorflow/versions/2.18.0/libtensorflow-cpu-linux-x86_64.tar.gz && tar -C /usr/local -xzf libtensorflow-cpu-linux-x86_64.tar.gz && ldconfig /usr/local/
WORKDIR /
