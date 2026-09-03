#!/bin/bash

# Check if both files are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <additional_settings_file> <sdkconfig_file>"
    exit 1
fi

ADDITIONAL_SETTINGS="$1"
SDKCONFIG="$2"

# Read additional settings line by line
while IFS= read -r line || [ -n "$line" ]; do
    # Skip completely empty lines
    [ -z "$line" ] && continue

    # Check if the line is a standard "is not set" ESP-IDF config comment
    if [[ "$line" =~ ^#\ (CONFIG_[A-Za-z0-9_]+)\ is\ not\ set ]]; then
        # Extract the key name from the regex match group
        key="${BASH_REMATCH[1]}"

        # Look for existing active or disabled versions of this key
        if grep -q "^$key=" "$SDKCONFIG" || grep -q "^# $key is not set" "$SDKCONFIG"; then
            sed -i "s|^$key=.*|$line|" "$SDKCONFIG"
            sed -i "s|^# $key is not set|$line|" "$SDKCONFIG"
            echo "Updated to 'not set': $key"
        else
            echo "$line" >> "$SDKCONFIG"
            echo "Added 'not set': $key"
        fi

    # Handle active settings (e.g., CONFIG_XYZ=y)
    elif [[ "$line" =~ ^[A-Za-z0-9_]+= ]]; then
        key=$(echo "$line" | cut -d'=' -f1)

        if grep -q "^$key=" "$SDKCONFIG" || grep -q "^# $key is not set" "$SDKCONFIG"; then
            sed -i "s|^$key=.*|$line|" "$SDKCONFIG"
            sed -i "s|^# $key is not set|$line|" "$SDKCONFIG"
            echo "Updated: $key"
        else
            echo "$line" >> "$SDKCONFIG"
            echo "Added: $key"
        fi

    # Handle all other lines (general text comments, headers, dividers)
    else
        echo "$line" >> "$SDKCONFIG"
        echo "Appended general comment/divider"
    fi

done < "$ADDITIONAL_SETTINGS"
