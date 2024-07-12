FROM nvcr.io/nvidia/l4t-base:r32.7.1

# Replace default repositories with Aliyun mirrors
RUN sed -i 's|http://ports.ubuntu.com/ubuntu-ports|http://mirrors.aliyun.com/ubuntu-ports|g' /etc/apt/sources.list

ENV TERM=xterm
ENV DEBIAN_FRONTEND=noninteractive
ENV APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=DontWarn
ARG NVIDIA_VISIBLE_DEVICES=all
ARG NVIDIA_DRIVER_CAPABILITIES=compute,utility


# Builder dependencies installation
RUN apt-get update -o Acquire::http::proxy=false -o Acquire::https::proxy=false \
    && apt-get install -qq -y --no-install-recommends -o Acquire::http::proxy=false -o Acquire::https::proxy=false \
    sudo \
    build-essential \
    cmake \
    git \
    openssl \
    libssl-dev \
    libusb-1.0-0-dev \
    pkg-config \
    libgtk-3-dev \
    libglfw3-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    curl \
    python3 \
    python3-dev \
    ca-certificates \
    zip \
    tzdata \
    libopencv-dev \
    && rm -rf /var/lib/apt/lists/*


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