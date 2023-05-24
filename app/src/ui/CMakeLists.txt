SET(SOURCES screens/ui_MainScreen.c
    screens/ui_Info.c
    screens/ui_Setup.c
    screens/ui_LessCommonSetup.c
    screens/ui_WifiConfig.c
    ui.c
    ui_helpers.c)

add_library(ui ${SOURCES})
