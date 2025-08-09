
```bash
wsl -l -o

wsl --install -d Ubuntu-24.04

wsl -d Ubuntu-24.04

lsb_release -a


# 要把 Ubuntu 24.04（Noble Numbat）的软件源（apt 仓库）更换为阿里云镜像，你需要修改系统的 APT 源配置文件 /etc/apt/sources.list
# 你看到的提示说明 从 Ubuntu 23.10 开始（包括 24.04），APT 的软件源配置方式已经变更：
# sources.list 不再是主要配置文件，而是迁移到了新的 deb822 格式配置文件中，位置为：
# /etc/apt/sources.list.d/ubuntu.sources

# sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak

sudo cp /etc/apt/sources.list.d/ubuntu.sources  /etc/apt/sources.list.d/ubuntu.sources.bak

# manually
sudo nano /etc/apt/sources.list.d/ubuntu.sources
# 2. 找到并修改 URL 字段（原默认是 archive.ubuntu.com）
URIs: http://archive.ubuntu.com/ubuntu or  URIs: http://security.ubuntu.com/ubuntu
URIs: http://mirrors.aliyun.com/ubuntu

# 如果你使用的是 nano，按 Ctrl + O 保存，按回车确认，Ctrl + X 退出。

# 替换的方法自动更改
sudo sed -i 's|http://archive.ubuntu.com/ubuntu|http://mirrors.aliyun.com/ubuntu|g' /etc/apt/sources.list.d/ubuntu.sources
sudo sed -i 's|http://security.ubuntu.com/ubuntu|http://mirrors.aliyun.com/ubuntu|g' /etc/apt/sources.list.d/ubuntu.sources

sudo apt update|grep aliyun


sudo apt-get update && sudo apt-get upgrade

sudo apt install -y build-essential gcc gdb make libssl-dev zlib1g-dev libbz2-dev libreadline-dev libsqlite3-dev wget curl llvm libncurses5-dev libncursesw5-dev xz-utils tk-dev libffi-dev liblzma-dev libcurl4-openssl-dev

# 推出当前 wsl
wsl -d Ubuntu-24.04
# WSL mounts Windows drives under /mnt, so E:\ becomes /mnt/e.
cd /mnt/e/projects/operating_system
# This opens the current folder (/mnt/e/projects/operating_system) in VS Code, using the WSL Ubuntu-24.04 environment.
code .
```
