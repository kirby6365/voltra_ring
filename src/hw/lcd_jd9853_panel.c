/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * JD9853 SPI panel driver with the init sequence shipped in the Waveshare
 * ESP32-S3-Touch-LCD-1.47 demo code (ESP-IDF/03_lvgl_example/components/esp_lcd_jd9853).
 */
#include <stdlib.h>
#include <sys/cdefs.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "esp_log.h"

#include "lcd_jd9853_panel.h"

static const char *TAG = "jd9853";

typedef struct {
    int cmd;
    const void *data;
    size_t data_bytes;
    unsigned int delay_ms;
} lcd_init_cmd_t;

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    int x_gap;
    int y_gap;
    uint8_t madctl_val;
} jd9853_panel_t;

/* Waveshare JD9853 vendor init sequence */
static const lcd_init_cmd_t jd9853_init_cmds[] = {
    {0x11, NULL, 0, 120},
    {0xDF, (uint8_t[]){0x98, 0x53}, 2, 0},
    {0xDF, (uint8_t[]){0x98, 0x53}, 2, 0},
    {0xB2, (uint8_t[]){0x23}, 1, 0},
    {0xB7, (uint8_t[]){0x00, 0x47, 0x00, 0x6F}, 4, 0},
    {0xBB, (uint8_t[]){0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0}, 6, 0},
    {0xC0, (uint8_t[]){0x44, 0xA4}, 2, 0},
    {0xC1, (uint8_t[]){0x16}, 1, 0},
    {0xC3, (uint8_t[]){0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77}, 8, 0},
    {0xC4, (uint8_t[]){0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82}, 12, 0},
    {0xC8, (uint8_t[]){0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
                       0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00}, 32, 0},
    {0xD0, (uint8_t[]){0x04, 0x06, 0x6B, 0x0F, 0x00}, 5, 0},
    {0xD7, (uint8_t[]){0x00, 0x30}, 2, 0},
    {0xE6, (uint8_t[]){0x14}, 1, 0},
    {0xDE, (uint8_t[]){0x01}, 1, 0},
    {0xB7, (uint8_t[]){0x03, 0x13, 0xEF, 0x35, 0x35}, 5, 0},
    {0xC1, (uint8_t[]){0x14, 0x15, 0xC0}, 3, 0},
    {0xC2, (uint8_t[]){0x06, 0x3A}, 2, 0},
    {0xC4, (uint8_t[]){0x72, 0x12}, 2, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xDE, (uint8_t[]){0x02}, 1, 0},
    {0xE5, (uint8_t[]){0x00, 0x02, 0x00}, 3, 0},
    {0xE5, (uint8_t[]){0x01, 0x02, 0x00}, 3, 0},
    {0xDE, (uint8_t[]){0x00}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x3A, (uint8_t[]){0x05}, 1, 0},                     // RGB565
    {0x2A, (uint8_t[]){0x00, 0x22, 0x00, 0xCD}, 4, 0},   // columns 34..205
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0x3F}, 4, 0},   // rows 0..319
    {0xDE, (uint8_t[]){0x02}, 1, 0},
    {0xE5, (uint8_t[]){0x00, 0x02, 0x00}, 3, 0},
    {0xDE, (uint8_t[]){0x00}, 1, 0},
    {0x29, NULL, 0, 0},
};

static esp_err_t panel_del(esp_lcd_panel_t *panel)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    if (p->reset_gpio_num >= 0) gpio_reset_pin(p->reset_gpio_num);
    free(p);
    return ESP_OK;
}

static esp_err_t panel_reset(esp_lcd_panel_t *panel)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    if (p->reset_gpio_num >= 0) {
        gpio_set_level(p->reset_gpio_num, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(p->reset_gpio_num, 1);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(p->io, LCD_CMD_SWRESET, NULL, 0), TAG, "swreset failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    return ESP_OK;
}

static esp_err_t panel_init(esp_lcd_panel_t *panel)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    for (size_t i = 0; i < sizeof(jd9853_init_cmds) / sizeof(jd9853_init_cmds[0]); i++) {
        const lcd_init_cmd_t *c = &jd9853_init_cmds[i];
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(p->io, c->cmd, c->data, c->data_bytes), TAG, "init cmd failed");
        if (c->delay_ms) vTaskDelay(pdMS_TO_TICKS(c->delay_ms));
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(p->io, LCD_CMD_MADCTL, (uint8_t[]){p->madctl_val}, 1), TAG, "madctl failed");
    return ESP_OK;
}

static esp_err_t panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    x_start += p->x_gap;
    x_end += p->x_gap;
    y_start += p->y_gap;
    y_end += p->y_gap;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(p->io, LCD_CMD_CASET, (uint8_t[]){
        (x_start >> 8) & 0xFF, x_start & 0xFF, ((x_end - 1) >> 8) & 0xFF, (x_end - 1) & 0xFF,
    }, 4), TAG, "caset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(p->io, LCD_CMD_RASET, (uint8_t[]){
        (y_start >> 8) & 0xFF, y_start & 0xFF, ((y_end - 1) >> 8) & 0xFF, (y_end - 1) & 0xFF,
    }, 4), TAG, "raset failed");
    size_t len = (size_t)(x_end - x_start) * (y_end - y_start) * 2;
    return esp_lcd_panel_io_tx_color(p->io, LCD_CMD_RAMWR, color_data, len);
}

static esp_err_t panel_invert_color(esp_lcd_panel_t *panel, bool invert)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    return esp_lcd_panel_io_tx_param(p->io, invert ? LCD_CMD_INVON : LCD_CMD_INVOFF, NULL, 0);
}

static esp_err_t panel_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    if (mirror_x) p->madctl_val |= LCD_CMD_MX_BIT; else p->madctl_val &= ~LCD_CMD_MX_BIT;
    if (mirror_y) p->madctl_val |= LCD_CMD_MY_BIT; else p->madctl_val &= ~LCD_CMD_MY_BIT;
    return esp_lcd_panel_io_tx_param(p->io, LCD_CMD_MADCTL, (uint8_t[]){p->madctl_val}, 1);
}

static esp_err_t panel_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    if (swap_axes) p->madctl_val |= LCD_CMD_MV_BIT; else p->madctl_val &= ~LCD_CMD_MV_BIT;
    return esp_lcd_panel_io_tx_param(p->io, LCD_CMD_MADCTL, (uint8_t[]){p->madctl_val}, 1);
}

static esp_err_t panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    p->x_gap = x_gap;
    p->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_disp_on_off(esp_lcd_panel_t *panel, bool on)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    return esp_lcd_panel_io_tx_param(p->io, on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0);
}

static esp_err_t panel_disp_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    jd9853_panel_t *p = __containerof(panel, jd9853_panel_t, base);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(p->io, sleep ? LCD_CMD_SLPIN : LCD_CMD_SLPOUT, NULL, 0), TAG, "sleep cmd failed");
    vTaskDelay(pdMS_TO_TICKS(120));   // the panel needs this long before the next sleep in / out
    return ESP_OK;
}

esp_err_t lcd_jd9853_panel_create(esp_lcd_panel_io_handle_t io, int reset_gpio_num, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    jd9853_panel_t *p = (jd9853_panel_t *)calloc(1, sizeof(jd9853_panel_t));
    ESP_RETURN_ON_FALSE(p, ESP_ERR_NO_MEM, TAG, "no mem for panel");

    if (reset_gpio_num >= 0) {
        gpio_config_t cfg = {};
        cfg.mode = GPIO_MODE_OUTPUT;
        cfg.pin_bit_mask = 1ULL << reset_gpio_num;
        esp_err_t err = gpio_config(&cfg);
        if (err != ESP_OK) {
            free(p);
            return err;
        }
    }

    p->io = io;
    p->reset_gpio_num = reset_gpio_num;
    p->madctl_val = 0;   // RGB order, no mirroring

    p->base.del = panel_del;
    p->base.reset = panel_reset;
    p->base.init = panel_init;
    p->base.draw_bitmap = panel_draw_bitmap;
    p->base.invert_color = panel_invert_color;
    p->base.set_gap = panel_set_gap;
    p->base.mirror = panel_mirror;
    p->base.swap_xy = panel_swap_xy;
    p->base.disp_on_off = panel_disp_on_off;
    p->base.disp_sleep = panel_disp_sleep;
    *ret_panel = &p->base;
    return ESP_OK;
}
