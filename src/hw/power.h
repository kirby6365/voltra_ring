#pragma once

/**
 * Power off / on with the button: deep sleep, woken only by the button.
 *
 * In deep sleep the ESP32-S3 is down to its RTC domain and just watches PIN_BUTTON; the
 * radio, CPU, PSRAM and flash are off, the panel and the touch controller are in their
 * sleep modes, and the backlight is held off. What is left is outside software
 * control: the board's 3.3 V regulator, the panel and touch chips' standby current, and
 * the battery-sense divider.
 */

/** Hold this long to power off (the UI counts down towards it). */
constexpr unsigned POWER_OFF_HOLD_MS = 5000;
/** Hold this long, from waking, to power back on; a shorter press goes straight back to sleep. */
constexpr unsigned POWER_ON_HOLD_MS = 3000;

/**
 * Call first thing in setup(). After a cold boot it just releases the pins held during
 * sleep. After a button wake it keeps everything dark and waits: if the button is let go
 * before POWER_ON_HOLD_MS it sleeps again, otherwise it returns and the boot carries on
 * (with the button still down; the button driver ignores that press).
 */
void power_boot_check();

/** Wait for the button to be let go, turn the screen off, then deep sleep. Does not return. */
[[noreturn]] void power_off();
