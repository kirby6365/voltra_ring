#include "button.h"

#include <Arduino.h>
#include <atomic>

#include "driver/gpio.h"
#include "esp_timer.h"

#include "board_pins.h"
#include "power.h"

namespace {
constexpr uint32_t TICK_US = 5000;
constexpr uint32_t TICK_MS = TICK_US / 1000;
constexpr uint8_t DEBOUNCE_TICKS = 4;       // 20 ms stable before a change counts

esp_timer_handle_t s_timer = nullptr;
std::atomic<int> s_clicks{0};
std::atomic<int> s_holds{0};
std::atomic<int> s_power_offs{0};
std::atomic<uint32_t> s_held_ms{0};

bool s_down = false;        // debounced state
bool s_ignore = false;      // this press is ignored until it is let go
uint8_t s_stable = 0;       // ticks the raw level has disagreed with s_down
uint32_t s_down_ms = 0;     // held so far
bool s_power_sent = false;

void poll_cb(void *)
{
    const bool raw_down = gpio_get_level((gpio_num_t)PIN_BUTTON) == 0;
    if (raw_down != s_down) {
        if (++s_stable >= DEBOUNCE_TICKS) {
            s_stable = 0;
            s_down = raw_down;
            if (s_down) {
                s_down_ms = 0;
                s_power_sent = false;
            } else {
                if (!s_ignore && !s_power_sent) {
                    if (s_down_ms < BUTTON_HOLD_MS) s_clicks++;
                    else if (s_down_ms < BUTTON_CANCEL_MS) s_holds++;
                    // longer: the power-off countdown was let go, which cancels it
                }
                s_ignore = false;
            }
        }
    } else {
        s_stable = 0;
    }
    if (s_down) {
        s_down_ms += TICK_MS;
        if (!s_ignore && !s_power_sent && s_down_ms >= POWER_OFF_HOLD_MS) {
            s_power_sent = true;
            s_power_offs++;
        }
    }
    s_held_ms = (s_down && !s_ignore) ? s_down_ms : 0;
}

/** Take one pending event from a counter. */
bool take(std::atomic<int> &n)
{
    int v = n.load();
    while (v > 0 && !n.compare_exchange_weak(v, v - 1)) {}
    return v > 0;
}
}  // namespace

void button_init()
{
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << PIN_BUTTON;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    // Still held from powering on: that press must not also load or unload.
    if (gpio_get_level((gpio_num_t)PIN_BUTTON) == 0) {
        s_down = true;
        s_ignore = true;
    }

    esp_timer_create_args_t args = {};
    args.callback = poll_cb;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "button";
    esp_timer_create(&args, &s_timer);
    esp_timer_start_periodic(s_timer, TICK_US);
}

ButtonEvent button_take_event()
{
    if (take(s_power_offs)) return ButtonEvent::PowerOff;
    if (take(s_holds)) return ButtonEvent::Hold;
    if (take(s_clicks)) return ButtonEvent::Click;
    return ButtonEvent::None;
}

uint32_t button_held_ms()
{
    return s_held_ms.load();
}
