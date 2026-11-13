#include "knob.h"

#include <Arduino.h>

#include "driver/pulse_cnt.h"

#include "board_pins.h"

#ifndef KNOB_COUNTS_PER_DETENT
#define KNOB_COUNTS_PER_DETENT 1
#endif
#ifndef KNOB_INVERT
#define KNOB_INVERT 0
#endif

namespace {
// The hardware counter wraps at these limits; accum_count carries the overflow into the
// value pcnt_unit_get_count() returns, so the running total never wraps in practice.
constexpr int COUNT_LIMIT = 10000;
constexpr uint32_t GLITCH_NS = 1000;   // the filter's maximum at the 80 MHz APB clock is ~12 us

pcnt_unit_handle_t s_unit = nullptr;
int s_taken = 0;   // counts already handed out as detents
}  // namespace

void knob_init()
{
    // The encoder's common pin goes to GND. Pulls first: the pulse counter only routes
    // the pins to itself and (on current ESP-IDF) leaves their pulls alone.
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);

    pcnt_unit_config_t unit_cfg = {};
    unit_cfg.low_limit = -COUNT_LIMIT;
    unit_cfg.high_limit = COUNT_LIMIT;
    unit_cfg.flags.accum_count = 1;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, &s_unit));

    pcnt_glitch_filter_config_t filter = {};
    filter.max_glitch_ns = GLITCH_NS;
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_unit, &filter));

    // Two channels, each counting the edges of one phase with the other as direction:
    // full x4 quadrature decoding, so contact bounce counts up and back down again.
    pcnt_chan_config_t a_cfg = {};
    a_cfg.edge_gpio_num = PIN_ENC_A;
    a_cfg.level_gpio_num = PIN_ENC_B;
    pcnt_channel_handle_t chan_a = nullptr;
    ESP_ERROR_CHECK(pcnt_new_channel(s_unit, &a_cfg, &chan_a));

    pcnt_chan_config_t b_cfg = {};
    b_cfg.edge_gpio_num = PIN_ENC_B;
    b_cfg.level_gpio_num = PIN_ENC_A;
    pcnt_channel_handle_t chan_b = nullptr;
    ESP_ERROR_CHECK(pcnt_new_channel(s_unit, &b_cfg, &chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // accum_count needs the limits as watch points to catch each overflow.
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_unit, -COUNT_LIMIT));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_unit, COUNT_LIMIT));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_unit));
}

int knob_take_delta()
{
    if (!s_unit) return 0;
    int count = 0;
    if (pcnt_unit_get_count(s_unit, &count) != ESP_OK) return 0;
    // Whole detents only (C division truncates towards zero); a half-turned detent stays
    // in the counter until it completes or rolls back.
    int d = (count - s_taken) / KNOB_COUNTS_PER_DETENT;
    s_taken += d * KNOB_COUNTS_PER_DETENT;
#if KNOB_INVERT
    d = -d;
#endif
    return d;
}
