#pragma once
/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPI panel driver for the JD9853 used on the Waveshare ESP32-S3-Touch-LCD-1.47.
 * Derived from the Waveshare esp_lcd_jd9853 component (Apache-2.0, after Espressif's
 * esp_lcd_st7789) with its initialisation table baked in.
 */
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a 16-bit RGB565 panel on the given panel IO.
 * @param io              SPI panel IO (8-bit commands and parameters, D/C pin)
 * @param reset_gpio_num  hardware reset GPIO or -1
 * @param ret_panel       resulting panel handle
 */
esp_err_t lcd_jd9853_panel_create(esp_lcd_panel_io_handle_t io, int reset_gpio_num, esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif
