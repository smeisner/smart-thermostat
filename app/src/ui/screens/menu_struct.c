#include <stdio.h>
// This is in C because there is no easy way to do recursive struct initialization at compile time
// in C++

/*
    * Network
        - Wifi
            - SSID
                - Scan
                - <ssid list> <strength>
            - Password
        - MQTT
            - Enabled <bool>
            - Hostname
            - Username
            - Password
        - Matter
            - Reset
                - Confirm y/n
            - Pairing code
            - QR code
    * HVAC <- add warning about damaging equipment
        - HVAC
            - 1 stage
            - 2 stage
        - Reversing valve
            - None
            - Active on cool
            - Active on heat
        - Aux
            - Has Aux
            - Allow Aux with Heat
    * Setup
        - UI
            - brightness
                - adapt to room lighting
                - level
            - absent brightness
                - adapt to room lighting
                - level
                - delay before absent
            - beep
                - on/off
                - volume

        - Set Time
        - Set Time-zone
        - Calibrate screen
        - Set hostname
        - Reboot
        - Hard Resest
*/
#include "menu_internal.h"

extern int stages;
menu_t *menuTop[] = MENU_LIST(
    MENU_HIER("Network",
        MENU_HIER("Wifi",
            MENU_LEAF("SSID", menuSSID),
            MENU_LEAF("Password", menuPassword)
        ),
        MENU_LEAF("Set NTP Server", menuNTP),
        MENU_LEAF("Set hostname", menuHostname),
        MENU_HIER("MQTT",
            MENU_LEAF("MQTT Hostname", menuMQTTHostname),
            MENU_LEAF("MQTT Username", menuMQTTUsername),
            MENU_LEAF("MQTT Password", menuMQTTPassword)
        ),
        MENU_LEAF("Matter", menuMatter)
    ),
    MENU_HIER("Setup",
        MENU_HIER("Interface",
            MENU_LEAF("Brigtness", menuBrightness),
            MENU_LEAF("Dim", menuDim),
            MENU_LEAF("Beep", menuBeep)
        ),
        MENU_LEAF("Timezone", menuTimezone),
        MENU_LEAF("Calibrate screen", menuCalibrateScreen),
        MENU_LEAF("Reboot", menuReboot),
        MENU_WARN("HVAC", "Improperly changing these settings could damage your HVAC Equipment!",
            MENU_LEAF("Stages", menuHVACStages),
            MENU_LEAF("Reversing Valve", menuHVACRV),
            MENU_LEAF("Aux Heat", menuHVACAuxHeat)
        ),
        MENU_LEAF("Factory Reset", menuFactoryReset)
    )
);
