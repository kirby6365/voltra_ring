#pragma once
// Waveshare ESP32-S3-Touch-LCD-1.47 — ESP32-S3 side pin map
// (GPIOs from the Waveshare schematic and pinout drawing; the demo code disagrees with
// itself on the touch RST / INT pins, the schematic is authoritative.)

// JD9853 172x320 LCD over 4-wire SPI
#define PIN_LCD_CS      21
#define PIN_LCD_SCK     38
#define PIN_LCD_MOSI    39
#define PIN_LCD_DC      45
#define PIN_LCD_RST     40
#define PIN_LCD_BL      46   // backlight, PWM

// Portrait, as the case holds it. LCD_ROTATE_180 (platformio.ini) flips the picture and
// the touch together if the controls end of your case should be at the bottom.
#define LCD_H_RES       172
#define LCD_V_RES       320
#define LCD_X_GAP       34   // the 172 visible columns sit in the middle of a 240-column GRAM
#ifndef LCD_ROTATE_180
#define LCD_ROTATE_180  0
#endif

// I2C bus: AXS5106L touch (0x63)
#define PIN_I2C_SDA     42
#define PIN_I2C_SCL     41
#define PIN_TOUCH_RST   47
#define PIN_TOUCH_INT   48
#define I2C_ADDR_AXS5106L 0x63
// Not fitted on this board; the haptics driver probes for it and goes quiet without it.
#define I2C_ADDR_DRV2605  0x5A

// Controls in the case's end bay, wired to the pin header
#define PIN_ENC_A       9    // scroll wheel encoder (quadrature, common to GND)
#define PIN_ENC_B       11
#define PIN_BUTTON      2    // load / unload push button (to GND)

// Battery sense: ADC2_CH1 through a 200k/100k divider
#define PIN_BAT_ADC     12
#define BAT_DIVIDER     3
