#!/bin/sh
set -eu

HOST_GROUP_FILE=/run/host/etc/group

host_gid=""

# Prefer the group actually assigned to a currently connected USB serial device.
serial_device="$(
  find /dev -maxdepth 1 \
    \( -name 'ttyACM*' -o -name 'ttyUSB*' \) \
    -print -quit
)"

if [ -n "$serial_device" ]; then
  host_gid="$(stat -c '%g' "$serial_device")"
else
  # No serial device is currently connected. Fall back to the host's conventional serial device group.
  host_gid="$(
    awk -F: '
      $1 == "uucp" {
        print $3
        found = 1
        exit
      }

      $1 == "dialout" {
        fallback = $3
      }

      END {
        if (!found && fallback)
          print fallback
      }
    ' "$HOST_GROUP_FILE"
  )"
fi

if [ -n "$host_gid" ]; then
  # Reuse an existing container group if one already has this GID.
  group_name="$(
    getent group "$host_gid" |
      cut -d: -f1 ||
      true
  )"

  # Otherwise create a container-local group with the host GID.
  if [ -z "$group_name" ]; then
    groupadd --gid "$host_gid" host-serial
    group_name=host-serial
  fi

  # Add the normal devcontainer user to the matching group.
  usermod -aG "$group_name" vscode
else
  echo "Warning: could not determine host serial device group" >&2
fi

exec "$@"
