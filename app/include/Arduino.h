/*---------------------------------------------------------------
        Simple header file to incorporate Arduino functions
        mostly for the LD2410 sensor library.
---------------------------------------------------------------*/

#include "thermostat.hpp"
#include "Stream.h"

#define F(x) (x)
#define HEX "0x%x"

typedef uint8_t byte;

#define delay(x) vTaskDelay(pdMS_TO_TICKS(x))