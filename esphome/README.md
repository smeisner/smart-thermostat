# ESPHome

This directory contains [ESPHome](https://esphome.io)-based firmware.

## Why build locally?

It is much faster to build on a more powerful if your Home Assistant with ESPHome runs on something like Raspberry Pi.

## How to build/flash locally

1. Create a Python virtual environment and install `esphome` and `pillow==10.2.0`, as documented [here](https://esphome.io/guides/getting_started_command_line.html)
2. Attached your device via USB or extend the configuration to flash wirelessly
3. Build with `esphome run ...`
4. Clean configuration if you're having issues rebuilding (`esphome clean ...`)


You can achieve the above with the following snippet:

```bash
python3 -m venv venv
source venv/bin/activate
pip install esphome pillow==10.2.0
esphome run --no-logs device-v0.5.2.yaml --device /dev/ttyACM0
```
