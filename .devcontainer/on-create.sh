#!/bin/bash
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get install -y git curl wget python3-pip python3-venv apt-transport-https gpg libusb-1.0-0

sudo chown vscode:vscode ~/.platformio ~/.cache/platformio-build

getpio="$(mktemp --suffix=-get-platformio.py)"
curl -fsSL -o "$getpio" https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 "$getpio"

sudo mkdir -p /usr/local/bin
sudo ln -sf ~/.platformio/penv/bin/platformio /usr/local/bin/platformio
sudo ln -sf ~/.platformio/penv/bin/pio /usr/local/bin/pio
sudo ln -sf ~/.platformio/penv/bin/piodebuggdb /usr/local/bin/piodebuggdb

pio_python="$HOME/.platformio/penv/bin/python"
site_packages="$("$pio_python" -c 'import site; print(site.getsitepackages()[0])')"
cp ./.devcontainer/patch-pio-serial-discovery.py "$site_packages/platformio_host_serial.py" 
echo 'import platformio_host_serial' > "$site_packages/platformio_host_serial.pth"
