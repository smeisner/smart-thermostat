#!/bin/bash
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install -y git curl wget python3-pip python3-venv apt-transport-https gpg

sudo chown vscode:vscode ~/.platformio ~/.cache ~/.cache/platformio-build

getpio="$(mktemp --suffix=-get-platformio.py)"
curl -fsSL -o "$getpio" https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 "$getpio"

sudo mkdir -p /usr/local/bin
sudo ln -s ~/.platformio/penv/bin/platformio /usr/local/bin/platformio
sudo ln -s ~/.platformio/penv/bin/pio /usr/local/bin/pio
sudo ln -s ~/.platformio/penv/bin/piodebuggdb /usr/local/bin/piodebuggdb
