#pragma once

//
// Define all the GPIO pins used
//

// HW Version 0.02

// #define LED_BUILTIN 2

// #define LED_HEAT_PIN 0
// #define BUZZER_PIN 4
// #define TOUCH_CS_PIN 5
// #define SWD_TDI_PIN 12
// #define SWD_TCK_PIN 13
// #define SWD_TMS_PIN 14
// #define TFT_CS_PIN 16
// #define HVAC_HEAT_PIN 17
// #define TFT_CLK_PIN 18
// #define HVAC_COOL_PIN 19
// #define TFT_LED_PIN 32
// #define TFT_RESET_PIN 33
// #define TFT_MISO_PIN 34
// #define TOUCH_IRQ_PIN 35
// #define LIGHT_SENS_PIN 36
// #define MOTION_PIN 39
// #define SDA_PIN 21
// #define SCL_PIN 22
// #define TFT_MOSI_PIN 23
// #define HVAC_FAN_PIN 25
// #define LED_COOL_PIN 26
// #define TFT_DC_PIN 27
// //#define LED_IDLE_PIN -1
// #define LED_IDLE_PIN 2

// HW Version 0.04

#define HVAC_FAN_PIN 4
#define HVAC_HEAT_PIN 5
#define HVAC_COOL_PIN 6
#define HVAC_RVALV_PIN 7
#define BUZZER_PIN 17
#define MOTION_PIN 18
#define LIGHT_SENS_PIN 8
#define TFT_CS_PIN 9
#define TFT_RESET_PIN 10
#define TFT_DC_PIN 11
#define TFT_MOSI_PIN 12
#define TFT_CLK_PIN 13
#define TFT_LED_PIN 14
#define LD_RX 15
#define LD_TX 16
#define TFT_MISO_PIN 21
#define TOUCH_CS_PIN 47
#define TOUCH_IRQ_PIN 48
#define SCL_PIN 35
#define SDA_PIN 36
#define LED_FAN_PIN 37
#define LED_HEAT_PIN 38
#define HVAC_STAGE2_PIN 39
#define GPIO40 40 //@@@ Not used
#define GPIO41 41 //@@@ Not used
#define GPIO42 42 //@@@ Not used
#define LED_COOL_PIN 2

// #define SCL 35 //28
// #define SDA 36 //29

/*

Macros to convert GPIO number to ADC channel and vice versa

*/
#define _ADC_TO_GPIO(x) x##_GPIO_NUM
#define ADC_TO_GPIO(x) _ADC_TO_GPIO(x)
#define _GPIO_TO_ADC1(x) ADC1_GPIO##x##_CHANNEL
#define GPIO_TO_ADC1(x) _GPIO_TO_ADC1(x)
#define _GPIO_TO_ADC2(x) ADC2_GPIO##x##_CHANNEL
#define GPIO_TO_ADC2(x) _GPIO_TO_ADC2(x)

// Use some convoluted macro-magic to find the corresponding ADC for a specified GPIO
// Because ADCx_CHANNELx is n Enum, we can't use macro checks to verify which mapping is correct
//  Instead:
//    (1)  map GPIO->ADCx_GPIOy_CHANNEL : This will either result in an enum or an undefined macro
//    (2)  map (1) -> ADCx_CHANNEL_y_GPIO_NUM :
//         If (1) mapped to an enum, this will map back to a GPIO #
//         If (1) did NOT map to an enum, this will map to an empty value
//    (3)  compare (2) to the original GPIO which will only succeed if (1) mapped properly
#if (ADC_TO_GPIO(GPIO_TO_ADC1(CONFIG_BUTTON_INTR_PIN)) == CONFIG_BUTTON_INTR_PIN)
  #if (ADC_TO_GPIO(GPIO_TO_ADC1(CONFIG_GPIO_PIN)) == CONFIG_GPIO_PIN)
    #define CONFIG_ADC_CHAN GPIO_TO_ADC1(CONFIG_GPIO_PIN)
  #elif (ADC_TO_GPIO(GPIO_TO_ADC2(CONFIG_GPIO_PIN)) == CONFIG_GPIO_PIN)
    #define CONFIG_ADC_CHAN GPIO_TO_ADC2(CONFIG_GPIO_PIN)
  #else
    #error "No ADC found for CONFIG_GPIO_PIN"
    #define CONFIG_BUTTON_ADC_CHAN 0
  #endif
#endif
