#pragma once
#include <stdint.h>

/**
 * Scroll wheel input: a mechanical quadrature encoder on PIN_ENC_A / PIN_ENC_B, decoded
 * in hardware by the ESP32-S3 pulse counter (all four edges, with its glitch filter).
 *
 * KNOB_COUNTS_PER_DETENT is how many counted edges one detent makes: 1 for the case's
 * wheel encoder (platformio.ini), 2 for encoders that rest with A and B both high or both
 * low, 4 for one full cycle per detent. Define KNOB_INVERT=1 to flip the direction.
 */
void knob_init();

/** Detents turned since the last call (positive = wheel rolled up, towards the screen top). */
int knob_take_delta();
