import glob
import os

from serial.tools import list_ports
from serial.tools.list_ports_linux import SysFS

_original_comports = list_ports.comports


def comports(*args, **kwargs):
    ports = list(_original_comports(*args, **kwargs))
    seen = {port.device for port in ports}

    for link in glob.glob("/dev/serial/by-id/*"):
        target = os.path.realpath(link)

        if not target.startswith("/run/host/dev/"):
            continue

        # Stable path exposed to PlatformIO.
        if link not in seen:
            info = SysFS(target)
            info.device = link
            info.name = os.path.basename(link)
            ports.append(info)
            seen.add(link)

        # Real relocated host path used by esptool after it resolves the
        # /dev/serial/by-id symlink.
        if target not in seen:
            ports.append(SysFS(target))
            seen.add(target)

    return ports


list_ports.comports = comports
