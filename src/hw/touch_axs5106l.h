#pragma once
#include <stdint.h>

/** Initialise the AXS5106L touch controller on the I2C bus (Wire). */
bool touch_init();

/**
 * Put the controller into its sleep mode (about 8 uA per the vendor) for power off. The
 * reset in touch_init() at the next boot wakes it again.
 */
void touch_sleep();

/** Read the current touch point in screen coordinates. Returns true while a finger is down. */
bool touch_read(uint16_t &x, uint16_t &y);
