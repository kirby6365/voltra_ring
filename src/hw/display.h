#pragma once
#include <lvgl.h>

/** Bring up the SPI panel, LVGL, touch input and the LVGL task. */
void display_init();

/** Serialise LVGL API calls from other tasks. */
bool display_lock(int timeout_ms = -1);
void display_unlock();

/**
 * For power off: take the LVGL lock for good (nothing may draw to the panel again), turn
 * the backlight off and put the panel to sleep.
 */
void display_power_off();

/** Backlight 0..255 */
void display_set_backlight(uint8_t level);

/** RAII helper */
class LvLock {
public:
    LvLock() { display_lock(-1); }
    ~LvLock() { display_unlock(); }
    LvLock(const LvLock &) = delete;
    LvLock &operator=(const LvLock &) = delete;
};
