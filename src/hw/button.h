#pragma once
#include <stdint.h>

/** The load / unload push button on PIN_BUTTON (active low, internal pull-up). */
enum class ButtonEvent { None, Click, Hold, PowerOff };

/** Holds released from here up to BUTTON_CANCEL_MS are a Hold (auto load). */
constexpr uint32_t BUTTON_HOLD_MS = 800;
/** From here on the UI shows the power-off countdown, and letting go cancels it. */
constexpr uint32_t BUTTON_CANCEL_MS = 1500;

void button_init();

/**
 * Next pending event, or None.
 * - Click: let go before BUTTON_HOLD_MS.
 * - Hold: let go between BUTTON_HOLD_MS and BUTTON_CANCEL_MS.
 * - PowerOff: still held at POWER_OFF_HOLD_MS (hw/power.h); fires while the button is down.
 * Letting go between BUTTON_CANCEL_MS and the power-off point does nothing.
 *
 * A press already under way at button_init() (the hold that powered the remote on) is
 * ignored until it is let go.
 */
ButtonEvent button_take_event();

/** How long the button has been held so far, or 0 when it is up (or being ignored). */
uint32_t button_held_ms();
