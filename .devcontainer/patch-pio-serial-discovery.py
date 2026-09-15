import glob
import os

from serial.tools import list_ports
from serial.tools.list_ports_linux import SysFS

_original_comports = list_ports.comports


def comports(*args, **kwargs):
    ports = list(_original_comports(*args, **kwargs))
    seen = {port.device for port in ports}

    for link in glob.glob("/dev/serial/by-id/*"):
        if link in seen:
            continue

        target = os.path.realpath(link)

        if not target.startswith("/run/host/dev/"):
            continue

        info = SysFS(target)
        info.device = link
        info.name = os.path.basename(link)

        ports.append(info)

    return ports


list_ports.comports = comports
