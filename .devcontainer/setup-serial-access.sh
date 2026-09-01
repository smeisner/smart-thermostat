#!/usr/bin/env bash
set -euo pipefail

HOST_GROUP_FILE=/run/host/etc/group
CONTAINER_USER=${DEVCONTAINER_USER:-vscode}

declare -A host_groups=()

get_host_group_gid() {
    local name=$1

    awk -F: -v name="$name" '
        $1 == name {
            print $3
            exit
        }
    ' "$HOST_GROUP_FILE"
}

get_host_group_name() {
    local gid=$1

    awk -F: -v gid="$gid" '
        $3 == gid {
            print $1
            exit
        }
    ' "$HOST_GROUP_FILE"
}

add_host_group() {
    local gid=$1
    local name=${2:-}

    [[ -n $gid ]] || return

    # Prefer a known host group name over an unnamed device GID.
    if [[ -n $name || ! -v "host_groups[$gid]" ]]; then
        host_groups["$gid"]=$name
    fi
}

map_host_group() {
    local gid=$1
    local host_group=$2
    local container_group

    # Reuse an existing container group with the same GID.
    if container_group=$(getent group "$gid" 2>/dev/null | cut -d: -f1); then
        echo "GID $gid already mapped to container group $container_group"
    else
        if [[ -n $host_group ]]; then
            container_group="host-$host_group"
        else
            container_group="host-gid-$gid"
        fi

        # Avoid a name collision with an unrelated container group.
        if getent group "$container_group" &>/dev/null; then
            container_group="$container_group-$gid"
        fi

        echo "Creating container group $container_group with GID $gid"
        groupadd --gid "$gid" "$container_group"
    fi

    if id -nG "$CONTAINER_USER" | tr ' ' '\n' | grep -qxF "$container_group"; then
        echo "$CONTAINER_USER already belongs to $container_group"
    else
        echo "Adding $CONTAINER_USER to $container_group"
        usermod --append --groups "$container_group" "$CONTAINER_USER"
    fi
}

if [[ ! -r $HOST_GROUP_FILE ]]; then
    echo "Host group file not available at $HOST_GROUP_FILE" >&2
    exec "$@"
fi

#
# Collect groups owning currently connected serial devices.
#

shopt -s nullglob

for device in /dev/ttyACM* /dev/ttyUSB*; do
    gid=$(stat -c '%g' "$device")
    host_group=$(get_host_group_name "$gid")

    echo "Found $device: ${host_group:-unknown} ($gid)"
    add_host_group "$gid" "$host_group"
done

#
# Include the standard serial-access groups even if no matching device is
# currently connected.
#

for host_group in uucp dialout; do
    gid=$(get_host_group_gid "$host_group")

    if [[ -n $gid ]]; then
        echo "Found host group $host_group ($gid)"
        add_host_group "$gid" "$host_group"
    fi
done

#
# Map each unique host GID into the container.
#

for gid in "${!host_groups[@]}"; do
    map_host_group "$gid" "${host_groups[$gid]}"
done

exec "$@"
