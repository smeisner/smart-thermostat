SET(SOURCES screens/ui_MainScreen.c
    screens/ui_Info.c
    screens/ui_Setup.c
    screens/ui_LessCommonSetup.c
    screens/ui_WifiConfig.c
    screens/ui_MatterConfig.c
    screens/ui_ThermostatRestart.c
    screens/ui_MqttConfig.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c)

add_library(ui ${SOURCES})
