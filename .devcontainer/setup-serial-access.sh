#!/usr/bin/env bash
set -euo pipefail

HOST_DEV=/run/host/dev
CONTAINER_USER=${DEVCONTAINER_USER:-vscode}

declare -A host_gids=()

map_host_gid() {
    local gid=$1
    local container_group

    # Reuse an existing container group with the same GID.
    if container_group=$(getent group "$gid" 2>/dev/null | cut -d: -f1); then
        echo "GID $gid already mapped to container group $container_group"
    else
        container_group="host-serial-$gid"

        # Avoid a name collision with an unrelated container group.
        if getent group "$container_group" &>/dev/null; then
            container_group="$container_group-$gid"
        fi

        echo "Creating container group $container_group with GID $gid"
        groupadd --gid "$gid" "$container_group"
    fi

    # Check membership numerically so group names are irrelevant.
    if id -G "$CONTAINER_USER" | tr ' ' '\n' | grep -qxF "$gid"; then
        echo "$CONTAINER_USER already belongs to GID $gid"
    else
        echo "Adding $CONTAINER_USER to $container_group"
        usermod --append --groups "$container_group" "$CONTAINER_USER"
    fi
}

#
# Expose the host's stable serial-device links without replacing the
# container's own /dev filesystem.
#

if [[ -d $HOST_DEV/serial ]]; then
    ln -sfn "$HOST_DEV/serial" /dev/serial
fi

#
# Discover the GIDs used by host serial devices.
#
# ttyS devices are useful here because they normally exist even when no USB
# serial device is connected, allowing us to configure access at container
# startup before a board is plugged in.
#

shopt -s nullglob

for device in \
    "$HOST_DEV"/ttyS* \
    "$HOST_DEV"/ttyACM* \
    "$HOST_DEV"/ttyUSB*
do
    gid=$(stat -c '%g' "$device")

    echo "Found $device (GID $gid)"
    host_gids["$gid"]=1
done

#
# Map each unique host serial-device GID into the container.
#

for gid in "${!host_gids[@]}"; do
    map_host_gid "$gid"
done

exec "$@"
