FROM ubuntu:latest
RUN apt-get update && apt-get install -y \
    build-essential cmake libjpeg-dev libpng-dev git xorg-dev libwayland-dev libxkbcommon-dev libopencv-dev \
    wayland-protocols extra-cmake-modules wget \
    && rm -rf /var/lib/apt/lists/*
RUN wget -q --no-check-certificate https://storage.googleapis.com/tensorflow/versions/2.18.0/libtensorflow-cpu-linux-x86_64.tar.gz && tar -C /usr/local -xzf libtensorflow-cpu-linux-x86_64.tar.gz && ldconfig /usr/local/ && echo "done"
WORKDIR /
