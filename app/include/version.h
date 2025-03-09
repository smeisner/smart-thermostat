
#define VERSION_MAJOR 0
#define VERSION_MINOR 8
#define VERSION_BUILD 0

#define VERSION_BUILD_DATE __DATE__
#define VERSION_BUILD_TIME __TIME__

#define _STR(x) #x
#define STR(x) _STR(x)

#define VERSION_STRING STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_BUILD)
#define VERSION_COPYRIGHT "(C) Tah Der 2024 (steve@meisners.net)"
#define VERSION_BUILD_DATE_TIME VERSION_BUILD_DATE " - " VERSION_BUILD_TIME

extern const char *VersionString;
extern const char *VersionCopyright;
extern const char *VersionBuildDateTime;
