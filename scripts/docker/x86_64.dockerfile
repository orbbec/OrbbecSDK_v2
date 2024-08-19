FROM ubuntu:18.04

# Change all Ubuntu repository sources to Aliyun mirrors
RUN sed -i 's|http://archive.ubuntu.com/ubuntu|http://mirrors.aliyun.com/ubuntu|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com/ubuntu|http://mirrors.aliyun.com/ubuntu|g' /etc/apt/sources.list

ARG NVIDIA_VISIBLE_DEVICES=all
ARG NVIDIA_DRIVER_CAPABILITIES=compute,utility
ENV TERM=xterm
ENV DEBIAN_FRONTEND=noninteractive
ENV APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=DontWarn

# Builder dependencies installation
RUN apt-get update -o Acquire::http::proxy=false -o Acquire::https::proxy=false \
    && apt-get install -qq -y --no-install-recommends -o Acquire::http::proxy=false -o Acquire::https::proxy=false \
    sudo \
    build-essential \
    git \
    openssl \
    libssl-dev \
    libusb-1.0-0-dev \
    pkg-config \
    libgtk-3-dev \
    libglfw3-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    freeglut3-dev \
    libnuma1 \
    curl \
    python3 \
    python3-dev \
    ca-certificates \
    zip \
    tzdata \
    && rm -rf /var/lib/apt/lists/*


# Install CMake 3.30.0
RUN curl -L https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0-linux-x86_64.tar.gz -o cmake.tar.gz \
    && tar -xzf cmake.tar.gz --strip-components=1 -C /usr/local \
    && rm cmake.tar.gz


ADD 10_nvidia.json /etc/glvnd/egl_vendor.d/10_nvidia.json
RUN chmod 644 /etc/glvnd/egl_vendor.d/10_nvidia.json
ADD nvidia_icd.json /etc/vulkan/icd.d/nvidia_icd.json
RUN chmod 644 /etc/vulkan/icd.d/nvidia_icd.json
ENV NVIDIA_VISIBLE_DEVICES=${NVIDIA_VISIBLE_DEVICES:-all}
ENV NVIDIA_DRIVER_CAPABILITIES=${NVIDIA_DRIVER_CAPABILITIES:+$NVIDIA_DRIVER_CAPABILITIES,}graphics
COPY env.sh /etc/profile.d/ade_env.sh
COPY entrypoint.sh /ade_entrypoint
ENTRYPOINT ["/ade_entrypoint"]
CMD ["/bin/bash", "-c", "trap 'exit 147' TERM; tail -f /dev/null & while wait ${!}; test $? -ge 128; do true; done"]
