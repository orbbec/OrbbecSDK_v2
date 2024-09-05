# OrbbecSDK Docker Environment Setup

## Installing Docker

### For Overseas Users:

Refer to the [official Docker documentation](https://docs.docker.com/engine/install/ubuntu/).

### For Users in China:

Refer to the [Tsinghua University Open Source Software Mirror](https://mirrors.tuna.tsinghua.edu.cn/help/docker-ce/).

### Uninstall Previous Docker Installations:

If you have previously installed Docker, remove it first:

```bash
sudo apt-get remove docker docker-engine docker.io containerd runc
```

### Install Dependencies:

```bash
sudo apt-get install apt-transport-https ca-certificates curl gnupg2 software-properties-common
```

### Trust Docker’s GPG Key:

```bash
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
```

### Add Docker Repository:

```bash
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://mirrors.tuna.tsinghua.edu.cn/docker-ce/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
```

### Install Docker:

```bash
sudo apt-get update
sudo apt-get install docker-ce
```

### Add Current User to Docker Group:

To avoid using `sudo` for Docker commands, add your user to the Docker group:

```bash
sudo groupadd docker
sudo usermod -aG docker $USER
```

You will need to log out or reboot for this to take effect.

## Installing NVIDIA Docker

Refer to
the [official NVIDIA documentation](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html#docker).

## Getting Started

### Install ADE CLI

Go to the [ADE CLI releases page](https://gitlab.com/ApexAI/ade-cli/-/releases) and download the latest version for your
system.

```bash
mv ade+x86_64 ade
# or mv ade+aarch64 ade
chmod +x ade
sudo mv ade /usr/local/bin
```

### Shell Completion

Add the following code to your `.zshrc` or `.bashrc`:

```bash
if [ -n "$ZSH_VERSION" ]; then
    eval "$(_ADE_COMPLETE=source_zsh ade)"
else
    eval "$(_ADE_COMPLETE=source ade)"
fi
```

### Build Docker

```bash
./build_docker.sh
```

### Usage

#### Choose Directory to Mount (e.g., `~/ws`):

```bash
cd ~/ws
touch .adehome
```

This indicates that Docker will mount `~/ws` as the container's home directory. You can also mount other directories.

#### Set Up `.aderc`:

```bash
cd ~/ws
touch .aderc
```

Add the following to `.aderc`:

```bash
export ADE_DOCKER_RUN_ARGS="--cap-add=SYS_PTRACE \
 --privileged \
 --net=host \
 --add-host ade:127.0.0.1 \
 -v ${HOME}/.Xauthority:${HOME}/.Xauthority:ro \
 -e XAUTHORITY=${HOME}/.Xauthority \
 -v /dev:/dev \
"

export ADE_IMAGES=" \
 orbbecsdk-env:x86_64_20240711 \
"
```

#### Start Container:

Navigate to the directory containing `.aderc`:

```bash
ade start
ade enter
```

You should now be inside the Docker container.

### References

1. [Docker Documentation](https://docs.docker.com/engine/install/ubuntu/)
2. [NVIDIA Container Toolkit Installation Guide](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html)
3. [ADE CLI Documentation](https://ade-cli.readthedocs.io/en/latest/)
